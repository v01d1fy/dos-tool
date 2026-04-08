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

class HTTPFlood {
public:
    HTTPFlood(const std::string& target, int duration, int threads, int port,
              std::function<void(const std::string&)> callback = nullptr);
    ~HTTPFlood();

    std::map<std::string, std::string> run();
    void stop();

private:
    void worker(int worker_id);
    std::string resolve_host(const std::string& hostname);
    std::string get_random_path();
    std::string get_random_user_agent();

    std::string target_;
    int duration_;
    int num_threads_;
    int port_;
    std::function<void(const std::string&)> callback_;

    std::atomic<bool> stop_flag_{false};
    std::atomic<uint64_t> requests_sent_{0};
    std::atomic<uint64_t> failed_requests_{0};

    std::vector<std::thread> threads_;
    std::mutex mtx_;

    static const std::vector<std::string> USER_AGENTS;
    static const std::vector<std::string> PATHS;
};
