#pragma once

#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <random>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET (-1)
#endif

class Slowloris {
public:
    Slowloris(const std::string& target, int duration, int threads, int port,
              std::function<void(const std::string&)> callback = nullptr);
    ~Slowloris();

    std::map<std::string, std::string> run();
    void stop();

private:
    SOCKET create_connection(const std::string& ip);
    void send_partial_header(SOCKET sock);
    void keep_alive_worker();
    std::string resolve_host(const std::string& hostname);

    std::string target_;
    int duration_;
    int num_sockets_;
    int port_;
    std::function<void(const std::string&)> callback_;

    std::atomic<bool> stop_flag_{false};
    std::atomic<uint64_t> connections_opened_{0};
    std::atomic<uint64_t> connections_active_{0};

    std::vector<SOCKET> sockets_;
    std::mutex sockets_mtx_;
    std::vector<std::thread> threads_;
};
