#ifndef UDP_FLOOD_H
#define UDP_FLOOD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void (*udp_flood_log_fn)(const char *msg);

typedef struct {
    int64_t packets_sent;
    int64_t bytes_sent;
    double  start_time;
    double  end_time;
} udp_flood_stats_t;

typedef struct udp_flood_ctx udp_flood_t;

/*
 * Initialize UDP Flood context.
 * target       - hostname or IP
 * duration     - seconds to run
 * threads      - number of worker threads
 * port         - target port
 * payload_size - size of each UDP payload in bytes (max 65507)
 * log_cb       - callback for log messages (may be NULL)
 */
int  udp_flood_init(udp_flood_t **ctx, const char *target, int duration,
                    int threads, int port, int payload_size,
                    udp_flood_log_fn log_cb);

/*
 * Run the UDP Flood (blocking). Returns stats when done.
 */
udp_flood_stats_t udp_flood_run(udp_flood_t *ctx);

/*
 * Signal stop from another thread.
 */
void udp_flood_stop(udp_flood_t *ctx);

/*
 * Free all resources.
 */
void udp_flood_destroy(udp_flood_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UDP_FLOOD_H */
