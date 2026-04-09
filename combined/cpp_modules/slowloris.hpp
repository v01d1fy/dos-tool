#ifndef SLOWLORIS_HPP
#define SLOWLORIS_HPP

#include <string>
#include <map>
#include <functional>
#include <atomic>
#include <mutex>
#include <vector>
#include <thread>

class Slowloris {
public:
    using LogCallback = std::function<void(const std::string&)>;

    /**
     * Construct a Slowloris attack context.
     * @param target   hostname or IP
     * @param duration seconds to run
     * @param threads  number of connections to maintain
     * @param port     target port
     * @param callback log callback function
     */
    Slowloris(const std::string& target, int duration, int threads, int port,
              LogCallback callback = nullptr);

    ~Slowloris();

    /**
     * Run the attack (blocking). Returns a map of stats.
     * Keys: "connections_opened", "connections_active", "start_time", "end_time", "duration"
     */
    std::map<std::string, std::string> run();

    /**
     * Signal stop from another thread.
     */
    void stop();

private:
    std::string         m_target;
    int                 m_duration;
    int                 m_num_threads;
    int                 m_port;
    LogCallback         m_callback;

    std::atomic<bool>   m_stop_flag;
    std::atomic<int64_t> m_connections_opened;
    std::atomic<int64_t> m_connections_active;

    std::vector<std::thread> m_workers;

    void log(const std::string& msg);
    void worker(int thread_id);
};

/* ---- extern "C" wrappers for C interop (optional) ---- */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int64_t connections_opened;
    int64_t connections_active;
    double  start_time;
    double  end_time;
} slowloris_stats_t;

typedef void (*slowloris_log_fn)(const char *msg);

void *slowloris_create(const char *target, int duration, int threads, int port,
                       slowloris_log_fn log_cb);
slowloris_stats_t slowloris_run(void *handle);
void slowloris_stop(void *handle);
void slowloris_destroy(void *handle);

#ifdef __cplusplus
}
#endif

#endif /* SLOWLORIS_HPP */
