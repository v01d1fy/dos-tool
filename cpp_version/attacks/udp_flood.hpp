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
#include <cstdint>

class UDPFlood {
public:
    UDPFlood(const std::string& target, int duration, int threads, int port,
             int payload_size = 1024,
             std::function<void(const std::string&)> callback = nullptr);
    ~UDPFlood();

    std::map<std::string, std::string> run();
    void stop();

private:
    void worker(int worker_id);
    std::string resolve_host(const std::string& hostname);

    std::string target_;
    int duration_;
    int num_threads_;
    int port_;
    int payload_size_;
    std::function<void(const std::string&)> callback_;

    std::atomic<bool> stop_flag_{false};
    std::atomic<uint64_t> packets_sent_{0};
    std::atomic<uint64_t> bytes_sent_{0};
    std::atomic<uint64_t> failed_packets_{0};

    std::vector<std::thread> threads_;
    std::mutex mtx_;

    static constexpr int MAX_PAYLOAD_SIZE = 65507;
};
