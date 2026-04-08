#ifndef UDP_FLOOD_H
#define UDP_FLOOD_H

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
typedef void (*udp_flood_callback_t)(const char *message);

/* Stats structure */
typedef struct {
    long long packets_sent;
    long long bytes_sent;
    double    start_time;
    double    end_time;
    pthread_mutex_t lock;
} udp_flood_stats_t;

/* UDP Flood attack context */
typedef struct {
    char                 target[256];       /* hostname */
    char                 target_ip[64];     /* resolved IP */
    int                  duration;
    int                  threads;
    int                  port;
    int                  packet_size;
    volatile int         stop_flag;
    udp_flood_stats_t    stats;
    udp_flood_callback_t callback;
} udp_flood_t;

/*
 * Initialize a UDP Flood attack context.
 * Returns 0 on success, -1 on failure.
 */
int udp_flood_init(udp_flood_t *ctx, const char *target, int duration,
                   int threads, int port, int packet_size,
                   udp_flood_callback_t callback);

/*
 * Run the UDP Flood attack (blocking). Returns stats on completion.
 */
udp_flood_stats_t udp_flood_run(udp_flood_t *ctx);

/*
 * Signal the attack to stop.
 */
void udp_flood_stop(udp_flood_t *ctx);

/*
 * Clean up resources.
 */
void udp_flood_destroy(udp_flood_t *ctx);

#endif /* UDP_FLOOD_H */
