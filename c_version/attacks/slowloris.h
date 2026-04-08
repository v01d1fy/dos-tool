#ifndef SLOWLORIS_H
#define SLOWLORIS_H

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
typedef void (*slowloris_callback_t)(const char *message);

/* Stats structure */
typedef struct {
    long long connections_opened;
    long long connections_active;
    double    start_time;
    double    end_time;
    pthread_mutex_t lock;
} slowloris_stats_t;

/* Maximum tracked sockets */
#define SLOWLORIS_MAX_SOCKETS 4096

/* Slowloris attack context */
typedef struct {
    char                 target[256];      /* hostname */
    char                 resolved_ip[64];  /* resolved IP */
    int                  duration;
    int                  threads;
    int                  port;
    volatile int         stop_flag;
    slowloris_stats_t    stats;
    slowloris_callback_t callback;

    /* Active socket tracking */
#ifdef _WIN32
    SOCKET               active_sockets[SLOWLORIS_MAX_SOCKETS];
#else
    int                  active_sockets[SLOWLORIS_MAX_SOCKETS];
#endif
    int                  num_active_sockets;
    pthread_mutex_t      socket_lock;
} slowloris_t;

/*
 * Initialize a Slowloris attack context.
 * Returns 0 on success, -1 on failure.
 */
int slowloris_init(slowloris_t *ctx, const char *target, int duration,
                   int threads, int port, slowloris_callback_t callback);

/*
 * Run the Slowloris attack (blocking). Returns stats on completion.
 */
slowloris_stats_t slowloris_run(slowloris_t *ctx);

/*
 * Signal the attack to stop.
 */
void slowloris_stop(slowloris_t *ctx);

/*
 * Clean up resources.
 */
void slowloris_destroy(slowloris_t *ctx);

#endif /* SLOWLORIS_H */
