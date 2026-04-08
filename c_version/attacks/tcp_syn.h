#ifndef TCP_SYN_H
#define TCP_SYN_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include "compat.h"

/* Callback function type for logging */
typedef void (*tcp_syn_callback_t)(const char *message);

/* Stats structure */
typedef struct {
    long long packets_sent;
    double    start_time;
    double    end_time;
    pthread_mutex_t lock;
} tcp_syn_stats_t;

/* TCP SYN Flood attack context */
typedef struct {
    char                target[256];       /* hostname */
    char                target_ip[64];     /* resolved IP */
    int                 duration;
    int                 threads;
    int                 port;
    volatile int        stop_flag;
    tcp_syn_stats_t     stats;
    tcp_syn_callback_t  callback;
} tcp_syn_t;

/*
 * Initialize a TCP SYN Flood attack context.
 * Returns 0 on success, -1 on failure.
 */
int tcp_syn_init(tcp_syn_t *ctx, const char *target, int duration,
                 int threads, int port, tcp_syn_callback_t callback);

/*
 * Run the TCP SYN Flood attack (blocking). Returns stats on completion.
 * Note: Requires root/admin privileges for raw sockets.
 */
tcp_syn_stats_t tcp_syn_run(tcp_syn_t *ctx);

/*
 * Signal the attack to stop.
 */
void tcp_syn_stop(tcp_syn_t *ctx);

/*
 * Clean up resources.
 */
void tcp_syn_destroy(tcp_syn_t *ctx);

#endif /* TCP_SYN_H */
