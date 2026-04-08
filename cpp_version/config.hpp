#pragma once

#include <string>
#include <vector>
#include <map>

namespace config {

    // Authorized targets - add IPs/domains you have permission to test
    inline const std::vector<std::string> AUTHORIZED_TARGETS = {};

    // Limits
    inline constexpr int MAX_DURATION_SECONDS = 300;
    inline constexpr int MAX_THREADS = 1000;

    // Defaults
    inline constexpr int DEFAULT_THREADS = 100;
    inline constexpr int DEFAULT_DURATION = 30;
    inline constexpr int DEFAULT_PORT = 80;

    // Attack types
    enum class AttackType {
        HTTP_FLOOD = 1,
        SLOWLORIS = 2,
        TCP_SYN = 3,
        UDP_FLOOD = 4
    };

    inline const std::map<AttackType, std::string> ATTACK_TYPES = {
        { AttackType::HTTP_FLOOD, "HTTP Flood" },
        { AttackType::SLOWLORIS, "Slowloris" },
        { AttackType::TCP_SYN,   "TCP SYN Flood" },
        { AttackType::UDP_FLOOD, "UDP Flood" }
    };

} // namespace config
