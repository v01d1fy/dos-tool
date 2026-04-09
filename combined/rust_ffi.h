#ifndef RUST_FFI_H
#define RUST_FFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Matches Rust #[repr(C)] TcpSynStats */
typedef struct {
    int64_t packets_sent;
    double  start_time;
    double  end_time;
} TcpSynStats;

/* Opaque context pointer */
typedef void TcpSynCtx;

/* Callback type for log messages */
typedef void (*tcp_syn_log_fn)(const char *msg);

/*
 * Initialize TCP SYN flood context.
 * Returns opaque pointer, or NULL on failure.
 */
TcpSynCtx *tcp_syn_init(const char *target, int duration, int threads,
                         int port, tcp_syn_log_fn callback);

/*
 * Run TCP SYN flood (blocking). Returns stats when finished.
 */
TcpSynStats tcp_syn_run(TcpSynCtx *ctx);

/*
 * Signal the attack to stop from another thread.
 */
void tcp_syn_stop(TcpSynCtx *ctx);

/*
 * Free all resources associated with the context.
 */
void tcp_syn_destroy(TcpSynCtx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* RUST_FFI_H */
