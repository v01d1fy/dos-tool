#ifndef HTTP_FLOOD_H
#define HTTP_FLOOD_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include "compat.h"

/* Callback function type for logging */
typedef void (*http_flood_callback_t)(const char *message);

/* Stats structure */
typedef struct {
    long long requests_sent;
    long long failed_requests;
    double    start_time;
    double    end_time;
    pthread_mutex_t lock;
} http_flood_stats_t;

/* HTTP Flood attack context */
typedef struct {
    char                  target[512];     /* original target (with http://) */
    char                  host[256];       /* hostname extracted from target */
    char                  resolved_ip[64]; /* resolved IP address */
    int                   duration;
    int                   threads;
    int                   port;
    volatile int          stop_flag;
    http_flood_stats_t    stats;
    http_flood_callback_t callback;
} http_flood_t;

/*
 * Initialize an HTTP Flood attack context.
 * Returns 0 on success, -1 on failure.
 */
int http_flood_init(http_flood_t *ctx, const char *target, int duration,
                    int threads, int port, http_flood_callback_t callback);

/*
 * Run the HTTP Flood attack (blocking). Returns stats on completion.
 */
http_flood_stats_t http_flood_run(http_flood_t *ctx);

/*
 * Signal the attack to stop.
 */
void http_flood_stop(http_flood_t *ctx);

/*
 * Clean up resources.
 */
void http_flood_destroy(http_flood_t *ctx);

#endif /* HTTP_FLOOD_H */
