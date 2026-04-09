#ifndef HTTP_FLOOD_H
#define HTTP_FLOOD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void (*http_flood_log_fn)(const char *msg);

typedef struct {
    int64_t requests_sent;
    int64_t failed_requests;
    double  start_time;
    double  end_time;
} http_flood_stats_t;

typedef struct http_flood_ctx http_flood_t;

/*
 * Initialize HTTP Flood context.
 * target   - hostname or IP
 * duration - seconds to run
 * threads  - number of worker threads
 * port     - target port (usually 80 or 443)
 * log_cb   - callback for log messages (may be NULL)
 */
int  http_flood_init(http_flood_t **ctx, const char *target, int duration,
                     int threads, int port, http_flood_log_fn log_cb);

/*
 * Run the HTTP Flood (blocking). Returns stats when done.
 */
http_flood_stats_t http_flood_run(http_flood_t *ctx);

/*
 * Signal stop from another thread.
 */
void http_flood_stop(http_flood_t *ctx);

/*
 * Free all resources.
 */
void http_flood_destroy(http_flood_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_FLOOD_H */
