/*
 * DoS Tool - C / C++ / Rust Combined
 * Main CLI entry point (C++)
 *
 * Authorized penetration testing tool for security research.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <mutex>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <conio.h>
#else
  #include <unistd.h>
  #include <sys/ioctl.h>
  #include <termios.h>
#endif

// ── Project headers ──────────────────────────────────────────
#include "config.h"

extern "C" {
#include "c_modules/http_flood.h"
#include "c_modules/udp_flood.h"
}

#include "cpp_modules/slowloris.hpp"

extern "C" {
#include "rust_ffi.h"
}

// ── ANSI color codes ─────────────────────────────────────────
#define CLR_RESET    "\033[0m"
#define CLR_RED      "\033[31m"
#define CLR_GREEN    "\033[32m"
#define CLR_YELLOW   "\033[33m"
#define CLR_BLUE     "\033[34m"
#define CLR_MAGENTA  "\033[35m"
#define CLR_CYAN     "\033[36m"
#define CLR_WHITE    "\033[37m"
#define CLR_BRED     "\033[91m"
#define CLR_BGREEN   "\033[92m"
#define CLR_BYELLOW  "\033[93m"
#define CLR_BCYAN    "\033[96m"
#define CLR_BMAGENTA "\033[95m"
#define CLR_BOLD     "\033[1m"
#define CLR_DIM      "\033[2m"

// ── Terminal utilities ───────────────────────────────────────
static void enable_ansi() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        GetConsoleMode(hOut, &mode);
        mode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
        SetConsoleMode(hOut, mode);
    }
    // Set console to UTF-8 so box-drawing and block characters render correctly
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
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

static int get_terminal_width() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 120;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 120;
#endif
}

static int get_terminal_height() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
    return 30;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_row;
    }
    return 30;
#endif
}

static void msleep(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ── Random engine ────────────────────────────────────────────
static std::mt19937& rng() {
    static std::mt19937 gen(
        (unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
    return gen;
}

static char random_char() {
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
        "@#$%&*!?<>{}[]|/\\~^";
    std::uniform_int_distribution<int> dist(0, (int)sizeof(charset) - 2);
    return charset[dist(rng())];
}

// ── Hacker messages ──────────────────────────────────────────
static const std::vector<std::string> HACKER_MESSAGES = {
    "Bypassing firewall...",
    "Injecting packets...",
    "Exploiting TCP stack...",
    "Fragmenting payloads...",
    "Spoofing source addresses...",
    "Saturating bandwidth...",
    "Overwhelming connection table...",
    "Rotating attack vectors...",
    "Amplifying traffic...",
    "Evading IDS signatures...",
    "Tunneling through proxy...",
    "Encrypting command channel...",
    "Deploying secondary payload...",
    "Scanning for open ports...",
    "Brute-forcing rate limits...",
    "Pivoting through nodes...",
    "Establishing persistence...",
    "Harvesting session tokens...",
    "Mapping network topology...",
    "Enumerating services...",
};

// ── Matrix rain effect ───────────────────────────────────────
static void matrix_rain() {
    int width = get_terminal_width();
    int height = get_terminal_height();
    clear_screen();
    hide_cursor();

    std::vector<int> drops(width, 0);
    for (auto& d : drops) {
        std::uniform_int_distribution<int> dist(0, height);
        d = dist(rng());
    }

    auto start = std::chrono::steady_clock::now();
    double duration = 2.5;

    while (true) {
        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= duration) break;

        for (int x = 0; x < width; x++) {
            std::uniform_int_distribution<int> chance(0, 2);
            if (chance(rng()) == 0) continue;

            int y = drops[x];
            if (y >= 0 && y < height) {
                move_cursor(y + 1, x + 1);
                // Bright green for head
                std::cout << CLR_BGREEN << random_char() << CLR_RESET;

                // Dim trail
                if (y > 0 && y - 1 < height) {
                    move_cursor(y, x + 1);
                    std::cout << CLR_GREEN << random_char() << CLR_RESET;
                }
                if (y > 1 && y - 2 < height) {
                    move_cursor(y - 1, x + 1);
                    std::cout << CLR_DIM << CLR_GREEN << random_char() << CLR_RESET;
                }
            }

            drops[x]++;
            if (drops[x] > height + 10) {
                std::uniform_int_distribution<int> reset_dist(-10, 0);
                drops[x] = reset_dist(rng());
            }
        }
        std::cout << std::flush;
        msleep(30);
    }

    clear_screen();
    show_cursor();
}

// ── ASCII banner with glitch ─────────────────────────────────
static const std::vector<std::string> BANNER_LINES = {
    R"( ██████╗  ██████╗ ███████╗    ████████╗ ██████╗  ██████╗ ██╗     )",
    R"( ██╔══██╗██╔═══██╗██╔════╝    ╚══██╔══╝██╔═══██╗██╔═══██╗██║     )",
    R"( ██║  ██║██║   ██║███████╗       ██║   ██║   ██║██║   ██║██║     )",
    R"( ██║  ██║██║   ██║╚════██║       ██║   ██║   ██║██║   ██║██║     )",
    R"( ██████╔╝╚██████╔╝███████║       ██║   ╚██████╔╝╚██████╔╝███████╗)",
    R"( ╚═════╝  ╚═════╝ ╚══════╝       ╚═╝    ╚═════╝  ╚═════╝ ╚══════╝)",
};

static void glitch_text(const std::string& line, const std::string& color) {
    // Print with random glitch characters, then reveal real text
    std::string glitched = line;
    int len = (int)line.size();

    // First pass: garbled
    for (int i = 0; i < len; i++) {
        if (line[i] != ' ') {
            glitched[i] = random_char();
        }
    }
    std::cout << color << glitched << CLR_RESET << "\r" << std::flush;
    msleep(40);

    // Second pass: partially revealed
    for (int i = 0; i < len; i++) {
        std::uniform_int_distribution<int> dist(0, 2);
        if (dist(rng()) == 0) {
            glitched[i] = line[i];
        }
    }
    std::cout << color << glitched << CLR_RESET << "\r" << std::flush;
    msleep(40);

    // Final: real text
    std::cout << color << line << CLR_RESET << std::endl;
}

static void show_banner() {
    std::cout << std::endl;
    for (const auto& line : BANNER_LINES) {
        glitch_text(line, std::string(CLR_BRED));
    }
    std::cout << std::endl;
}

// ── Loading bar ──────────────────────────────────────────────
static void loading_bar(const std::string& label, int duration_ms) {
    int width = 40;
    std::cout << CLR_CYAN << "  " << label << CLR_RESET << std::endl;

    int steps = 50;
    int step_ms = duration_ms / steps;

    for (int i = 0; i <= steps; i++) {
        int filled = (i * width) / steps;
        int empty = width - filled;
        int percent = (i * 100) / steps;

        std::cout << "  " << CLR_GREEN;
        std::cout << "\xE2\x96\x90"; // left half block
        for (int j = 0; j < filled; j++) std::cout << "\xE2\x96\x88"; // full block
        for (int j = 0; j < empty; j++)  std::cout << "\xE2\x96\x91"; // light shade
        std::cout << "\xE2\x96\x90"; // left half block
        std::cout << CLR_RESET;

        std::cout << " " << CLR_BYELLOW << percent << "%" << CLR_RESET << "  \r";
        std::cout << std::flush;
        msleep(step_ms);
    }
    std::cout << std::endl;
}

// ── Box drawing ──────────────────────────────────────────────
static void print_box(const std::string& text, const std::string& color) {
    int len = (int)text.size();
    int pad = 4;
    int total = len + pad * 2;

    std::string top    = "\xE2\x95\x94"; // top-left corner
    std::string bottom = "\xE2\x95\x9A"; // bottom-left corner
    std::string horiz  = "\xE2\x95\x90"; // horizontal line
    std::string vert   = "\xE2\x95\x91"; // vertical line
    std::string tr     = "\xE2\x95\x97"; // top-right corner
    std::string br     = "\xE2\x95\x9D"; // bottom-right corner

    std::cout << color;

    // Top border
    std::cout << "  " << top;
    for (int i = 0; i < total; i++) std::cout << horiz;
    std::cout << tr << std::endl;

    // Middle
    std::cout << "  " << vert;
    for (int i = 0; i < pad; i++) std::cout << " ";
    std::cout << text;
    for (int i = 0; i < pad; i++) std::cout << " ";
    std::cout << vert << std::endl;

    // Bottom border
    std::cout << "  " << bottom;
    for (int i = 0; i < total; i++) std::cout << horiz;
    std::cout << br << std::endl;

    std::cout << CLR_RESET;
}

// ── Status messages ──────────────────────────────────────────
static void status_message(const std::string& msg, const std::string& color, int delay = 300) {
    std::cout << color << "  " << msg << CLR_RESET << std::endl;
    msleep(delay);
}

// ── Menu ─────────────────────────────────────────────────────
static void show_menu() {
    std::cout << std::endl;
    std::cout << CLR_CYAN << "  ┌─────────────────────────────────────────────────────────────┐" << CLR_RESET << std::endl;
    std::cout << CLR_CYAN << "  │" << CLR_RESET << CLR_BOLD << "                      SELECT ATTACK MODE                      " << CLR_RESET << CLR_CYAN << "│" << CLR_RESET << std::endl;
    std::cout << CLR_CYAN << "  ├─────────────────────────────────────────────────────────────┤" << CLR_RESET << std::endl;
    std::cout << CLR_CYAN << "  │" << CLR_RESET
              << CLR_BRED    << "  [1] " << CLR_WHITE << "HTTP Flood       " << CLR_DIM << "- High volume HTTP requests         " << CLR_YELLOW << "[C]   " << CLR_RESET
              << CLR_CYAN << "│" << CLR_RESET << std::endl;
    std::cout << CLR_CYAN << "  │" << CLR_RESET
              << CLR_BMAGENTA << "  [2] " << CLR_WHITE << "Slowloris        " << CLR_DIM << "- Slow HTTP connections              " << CLR_BCYAN  << "[C++] " << CLR_RESET
              << CLR_CYAN << "│" << CLR_RESET << std::endl;
    std::cout << CLR_CYAN << "  │" << CLR_RESET
              << CLR_BGREEN   << "  [3] " << CLR_WHITE << "TCP SYN Flood    " << CLR_DIM << "- Raw socket SYN packets             " << CLR_BRED   << "[Rust]" << CLR_RESET
              << CLR_CYAN << "│" << CLR_RESET << std::endl;
    std::cout << CLR_CYAN << "  │" << CLR_RESET
              << CLR_BYELLOW  << "  [4] " << CLR_WHITE << "UDP Flood        " << CLR_DIM << "- UDP packet saturation              " << CLR_YELLOW << "[C]   " << CLR_RESET
              << CLR_CYAN << "│" << CLR_RESET << std::endl;
    std::cout << CLR_CYAN << "  │" << CLR_RESET
              << CLR_RED      << "  [5] " << CLR_WHITE << "Exit             " << CLR_DIM << "                                     " << "      " << CLR_RESET
              << CLR_CYAN << "│" << CLR_RESET << std::endl;
    std::cout << CLR_CYAN << "  └─────────────────────────────────────────────────────────────┘" << CLR_RESET << std::endl;
    std::cout << std::endl;
}

// ── Input helpers ────────────────────────────────────────────
static std::string prompt_string(const std::string& prompt, const std::string& default_val = "") {
    std::cout << CLR_CYAN << "  [?] " << CLR_WHITE << prompt;
    if (!default_val.empty()) {
        std::cout << CLR_DIM << " [" << default_val << "]";
    }
    std::cout << ": " << CLR_BGREEN;

    std::string input;
    std::getline(std::cin, input);
    std::cout << CLR_RESET;

    if (input.empty() && !default_val.empty()) return default_val;
    return input;
}

static int prompt_int(const std::string& prompt, int default_val) {
    std::string def_str = std::to_string(default_val);
    std::string input = prompt_string(prompt, def_str);
    try {
        return std::stoi(input);
    } catch (...) {
        return default_val;
    }
}

// ── Target acquired animation ────────────────────────────────
static void target_acquired_animation(const std::string& target, int port) {
    std::cout << std::endl;
    std::cout << CLR_BRED << "  ┌───────────────────────────────────────────┐" << CLR_RESET << std::endl;
    std::cout << CLR_BRED << "  │" << CLR_RESET
              << CLR_BOLD << CLR_WHITE << "           TARGET ACQUIRED                  " << CLR_RESET
              << CLR_BRED << "│" << CLR_RESET << std::endl;
    std::cout << CLR_BRED << "  ├───────────────────────────────────────────┤" << CLR_RESET << std::endl;

    // Scanning animation
    std::string scanning = "  Scanning";
    for (int i = 0; i < 3; i++) {
        std::cout << "\r" << CLR_YELLOW << "  │  " << scanning;
        for (int d = 0; d <= i; d++) std::cout << ".";
        for (int d = i + 1; d < 3; d++) std::cout << " ";
        std::cout << "                                " << CLR_BRED << "│" << CLR_RESET << std::flush;
        msleep(400);
    }
    std::cout << std::endl;

    // Target info
    char buf[256];
    snprintf(buf, sizeof(buf), "  Host: %-36s", target.c_str());
    std::cout << CLR_BRED << "  │" << CLR_BGREEN << buf << CLR_BRED << "│" << CLR_RESET << std::endl;

    snprintf(buf, sizeof(buf), "  Port: %-36d", port);
    std::cout << CLR_BRED << "  │" << CLR_BGREEN << buf << CLR_BRED << "│" << CLR_RESET << std::endl;

    std::cout << CLR_BRED << "  │" << CLR_BGREEN << "  Status: " << CLR_BRED << "LOCKED ON" << "                          " << CLR_BRED << "│" << CLR_RESET << std::endl;

    std::cout << CLR_BRED << "  └───────────────────────────────────────────┘" << CLR_RESET << std::endl;
    std::cout << std::endl;
    msleep(500);
}

// ── Countdown 3-2-1 ─────────────────────────────────────────
static const std::vector<std::vector<std::string>> COUNTDOWN_DIGITS = {
    // 3
    {
        " ██████╗ ",
        " ╚════██╗",
        "  █████╔╝",
        "  ╚═══██╗",
        " ██████╔╝",
        " ╚═════╝ ",
    },
    // 2
    {
        " ██████╗ ",
        " ╚════██╗",
        "  █████╔╝",
        " ██╔═══╝ ",
        " ███████╗",
        " ╚══════╝",
    },
    // 1
    {
        "    ██╗  ",
        "   ███║  ",
        "   ╚██║  ",
        "    ██║  ",
        "    ██║  ",
        "    ╚═╝  ",
    },
};

static void countdown_321() {
    const std::string colors[] = { CLR_BRED, CLR_BYELLOW, CLR_BGREEN };

    for (int d = 0; d < 3; d++) {
        clear_screen();
        int height = get_terminal_height();
        int start_row = height / 2 - 3;
        if (start_row < 1) start_row = 1;

        int width = get_terminal_width();
        int digit_width = 9;
        int start_col = (width - digit_width) / 2;
        if (start_col < 1) start_col = 1;

        for (int line = 0; line < 6; line++) {
            move_cursor(start_row + line, start_col);
            std::cout << colors[d] << COUNTDOWN_DIGITS[d][line] << CLR_RESET;
        }
        std::cout << std::flush;
        msleep(800);
    }
    clear_screen();
}

// ── "ATTACK!" ASCII art ─────────────────────────────────────
static void attack_splash() {
    static const std::vector<std::string> ATTACK_ART = {
        R"(     █████╗ ████████╗████████╗ █████╗  ██████╗██╗  ██╗██╗)",
        R"(    ██╔══██╗╚══██╔══╝╚══██╔══╝██╔══██╗██╔════╝██║ ██╔╝██║)",
        R"(    ███████║   ██║      ██║   ███████║██║     █████╔╝ ██║)",
        R"(    ██╔══██║   ██║      ██║   ██╔══██║██║     ██╔═██╗ ╚═╝)",
        R"(    ██║  ██║   ██║      ██║   ██║  ██║╚██████╗██║  ██╗██╗)",
        R"(    ╚═╝  ╚═╝   ╚═╝      ╚═╝   ╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝╚═╝)",
    };

    int height = get_terminal_height();
    int start_row = height / 2 - 3;
    if (start_row < 1) start_row = 1;

    // Flash effect
    for (int flash = 0; flash < 3; flash++) {
        clear_screen();
        for (int i = 0; i < (int)ATTACK_ART.size(); i++) {
            move_cursor(start_row + i, 5);
            std::cout << CLR_BRED << CLR_BOLD << ATTACK_ART[i] << CLR_RESET;
        }
        std::cout << std::flush;
        msleep(150);
        clear_screen();
        msleep(80);
    }

    // Final display
    clear_screen();
    for (int i = 0; i < (int)ATTACK_ART.size(); i++) {
        move_cursor(start_row + i, 5);
        std::cout << CLR_BRED << CLR_BOLD << ATTACK_ART[i] << CLR_RESET;
    }
    std::cout << std::flush;
    msleep(1000);
    clear_screen();
}

// ── Skull ASCII art ──────────────────────────────────────────
static void show_skull() {
    static const std::vector<std::string> SKULL = {
        R"(            ░░░░░░░░░░░░░░░░░░░░░░░)",
        R"(         ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░)",
        R"(       ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░)",
        R"(      ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░)",
        R"(     ░░░░░░  ░░░░░░░░░░░░░░░░░░░  ░░░░░░░░░)",
        R"(    ░░░░░░    ░░░░░░░░░░░░░░░░░    ░░░░░░░░░)",
        R"(    ░░░░░░░  ░░░░░░░░░░░░░░░░░░░  ░░░░░░░░░░)",
        R"(    ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░)",
        R"(    ░░░░░░░░░░░░░  ░░░░░  ░░░░░░░░░░░░░░░░░░)",
        R"(     ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░)",
        R"(      ░░░░ ░░ ░░ ░░ ░░ ░░ ░░ ░░ ░░ ░░░░░░)",
        R"(       ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░)",
        R"(          ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░)",
    };

    std::cout << std::endl;
    for (const auto& line : SKULL) {
        std::cout << CLR_BRED << "  " << line << CLR_RESET << std::endl;
    }
    std::cout << std::endl;
}

// ── Results table ────────────────────────────────────────────
static void show_results_table(const std::string& attack_name,
                               const std::map<std::string, std::string>& stats) {
    // Find max key/value length
    size_t max_key = 10;
    size_t max_val = 10;
    for (const auto& kv : stats) {
        if (kv.first.size() > max_key) max_key = kv.first.size();
        if (kv.second.size() > max_val) max_val = kv.second.size();
    }
    max_key += 2;
    max_val += 2;
    size_t total_width = max_key + max_val + 3;

    std::cout << std::endl;
    std::cout << CLR_CYAN << "  +" << std::string(total_width, '-') << "+" << CLR_RESET << std::endl;
    std::cout << CLR_CYAN << "  | " << CLR_BOLD << CLR_WHITE << attack_name;
    int pad = (int)(total_width - 2 - attack_name.size());
    if (pad > 0) std::cout << std::string(pad, ' ');
    std::cout << CLR_RESET << CLR_CYAN << " |" << CLR_RESET << std::endl;
    std::cout << CLR_CYAN << "  +" << std::string(max_key + 1, '-') << "+" << std::string(max_val + 1, '-') << "+" << CLR_RESET << std::endl;

    for (const auto& kv : stats) {
        std::cout << CLR_CYAN << "  | " << CLR_BGREEN << kv.first;
        int kpad = (int)(max_key - kv.first.size());
        if (kpad > 0) std::cout << std::string(kpad, ' ');
        std::cout << CLR_CYAN << "| " << CLR_WHITE << kv.second;
        int vpad = (int)(max_val - kv.second.size());
        if (vpad > 0) std::cout << std::string(vpad, ' ');
        std::cout << CLR_CYAN << "|" << CLR_RESET << std::endl;
    }

    std::cout << CLR_CYAN << "  +" << std::string(max_key + 1, '-') << "+" << std::string(max_val + 1, '-') << "+" << CLR_RESET << std::endl;
    std::cout << std::endl;
}

// ── Callback functions for each language ─────────────────────
static std::mutex g_log_mutex;

// C callback
static void c_log_callback(const char *msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << CLR_GREEN << "  [C] " << CLR_WHITE << msg << CLR_RESET << std::endl;
}

// C++ callback
static void cpp_log_callback(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << CLR_BCYAN << "  [C++] " << CLR_WHITE << msg << CLR_RESET << std::endl;
}

// Rust callback (extern "C" compatible)
static void rust_log_callback(const char *msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << CLR_BRED << "  [Rust] " << CLR_WHITE << msg << CLR_RESET << std::endl;
}

// ── Hacker messages background thread ────────────────────────
static std::atomic<bool> g_hacker_running{false};

static void hacker_messages_thread() {
    std::uniform_int_distribution<int> dist(0, (int)HACKER_MESSAGES.size() - 1);
    std::uniform_int_distribution<int> delay_dist(800, 3000);

    while (g_hacker_running.load()) {
        msleep(delay_dist(rng()));
        if (!g_hacker_running.load()) break;

        std::lock_guard<std::mutex> lock(g_log_mutex);
        std::cout << CLR_MAGENTA << "  [*] " << CLR_DIM
                  << HACKER_MESSAGES[dist(rng())]
                  << CLR_RESET << std::endl;
    }
}

// ── Startup sequence ─────────────────────────────────────────
static void startup_sequence() {
    clear_screen();
    matrix_rain();
    clear_screen();

    show_banner();

    loading_bar("Initializing system...", 1500);
    std::cout << std::endl;

    print_box("Denial of Service Tool -- C / C++ / Rust Combined", std::string(CLR_CYAN));
    std::cout << std::endl;

    status_message("[+] C modules loaded      (http_flood, udp_flood)", std::string(CLR_GREEN), 400);
    status_message("[+] C++ modules loaded    (slowloris)",             std::string(CLR_BCYAN),  400);
    status_message("[+] Rust modules loaded   (tcp_syn_flood)",         std::string(CLR_BRED),   400);
    status_message("[+] System online",                                 std::string(CLR_BGREEN), 300);
    std::cout << std::endl;
}

// ── Format helpers ───────────────────────────────────────────
static std::string format_number(int64_t n) {
    std::string s = std::to_string(n);
    int insertPos = (int)s.length() - 3;
    while (insertPos > 0) {
        s.insert(insertPos, ",");
        insertPos -= 3;
    }
    return s;
}

static std::string format_bytes(int64_t bytes) {
    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    double val = (double)bytes;
    int u = 0;
    while (val >= 1024.0 && u < 4) {
        val /= 1024.0;
        u++;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f %s", val, units[u]);
    return buf;
}

static std::string format_duration(double secs) {
    int m = (int)(secs / 60.0);
    double s = secs - m * 60.0;
    char buf[64];
    if (m > 0) {
        snprintf(buf, sizeof(buf), "%dm %.2fs", m, s);
    } else {
        snprintf(buf, sizeof(buf), "%.2fs", s);
    }
    return buf;
}

// ══════════════════════════════════════════════════════════════
// MAIN
// ══════════════════════════════════════════════════════════════
int main() {
    enable_ansi();
    startup_sequence();

    while (true) {
        show_menu();

        std::string choice_str = prompt_string("Select attack mode", "");
        if (choice_str.empty()) continue;

        int choice = 0;
        try {
            choice = std::stoi(choice_str);
        } catch (...) {
            std::cout << CLR_RED << "  [!] Invalid selection." << CLR_RESET << std::endl;
            continue;
        }

        if (choice == 5) {
            std::cout << std::endl;
            std::cout << CLR_RED << "  [!] Shutting down..." << CLR_RESET << std::endl;
            msleep(500);
            std::cout << CLR_DIM << "  Goodbye." << CLR_RESET << std::endl;
            break;
        }

        if (choice < 1 || choice > 4) {
            std::cout << CLR_RED << "  [!] Invalid selection. Choose 1-5." << CLR_RESET << std::endl;
            continue;
        }

        // ── Gather parameters ────────────────────────────────
        std::string target = prompt_string("Enter target (IP or hostname)", "");
        if (target.empty()) {
            std::cout << CLR_RED << "  [!] Target is required." << CLR_RESET << std::endl;
            continue;
        }
        // Strip protocol prefix and trailing slash
        if (target.substr(0, 8) == "https://") target = target.substr(8);
        else if (target.substr(0, 7) == "http://") target = target.substr(7);
        while (!target.empty() && target.back() == '/') target.pop_back();

        int port     = prompt_int("Enter port",     DEFAULT_PORT);
        int duration = prompt_int("Enter duration (seconds)", DEFAULT_DURATION);
        int threads  = prompt_int("Enter threads",  DEFAULT_THREADS);

        // Clamp values
        if (duration > MAX_DURATION_SECONDS) duration = MAX_DURATION_SECONDS;
        if (duration < 1) duration = 1;
        if (threads > MAX_THREADS) threads = MAX_THREADS;
        if (threads < 1) threads = 1;
        if (port < 1 || port > 65535) port = DEFAULT_PORT;

        // ── Target acquired animation ────────────────────────
        target_acquired_animation(target, port);

        // ── Countdown ────────────────────────────────────────
        countdown_321();

        // ── ATTACK! splash ───────────────────────────────────
        attack_splash();

        // ── Start hacker messages thread ─────────────────────
        g_hacker_running.store(true);
        std::thread hacker_thread(hacker_messages_thread);

        // ── Dispatch attack ──────────────────────────────────
        std::map<std::string, std::string> results;
        std::string attack_name;

        switch (choice) {
            case 1: {
                // ── HTTP Flood - C module ────────────────────
                attack_name = "HTTP Flood [C]";
                std::cout << CLR_BRED << "  >>> Launching HTTP Flood (C module) <<<" << CLR_RESET << std::endl;
                std::cout << std::endl;

                http_flood_t *ctx = nullptr;
                http_flood_init(&ctx, target.c_str(), duration, threads, port, c_log_callback);
                auto stats = http_flood_run(ctx);
                http_flood_destroy(ctx);

                double elapsed = stats.end_time - stats.start_time;
                double rps = elapsed > 0 ? stats.requests_sent / elapsed : 0;

                results["Requests Sent"]   = format_number(stats.requests_sent);
                results["Failed Requests"] = format_number(stats.failed_requests);
                results["Duration"]        = format_duration(elapsed);
                results["Requests/sec"]    = format_number((int64_t)rps);
                results["Language"]        = "C";
                break;
            }

            case 2: {
                // ── Slowloris - C++ module ───────────────────
                attack_name = "Slowloris [C++]";
                std::cout << CLR_BMAGENTA << "  >>> Launching Slowloris (C++ module) <<<" << CLR_RESET << std::endl;
                std::cout << std::endl;

                Slowloris sl(target, duration, threads, port, cpp_log_callback);
                auto sl_stats = sl.run();

                double elapsed = 0;
                try { elapsed = std::stod(sl_stats["duration"]); } catch (...) {}

                results["Connections Opened"] = sl_stats["connections_opened"];
                results["Connections Active"] = sl_stats["connections_active"];
                results["Duration"]           = format_duration(elapsed);
                results["Language"]           = "C++";
                break;
            }

            case 3: {
                // ── TCP SYN Flood - Rust module (FFI) ────────
                attack_name = "TCP SYN Flood [Rust]";
                std::cout << CLR_BGREEN << "  >>> Launching TCP SYN Flood (Rust module via FFI) <<<" << CLR_RESET << std::endl;
                std::cout << std::endl;

                auto ctx = tcp_syn_init(target.c_str(), duration, threads, port, rust_log_callback);
                if (ctx) {
                    auto stats = tcp_syn_run(ctx);
                    tcp_syn_destroy(ctx);

                    double elapsed = stats.end_time - stats.start_time;
                    double pps = elapsed > 0 ? stats.packets_sent / elapsed : 0;

                    results["Packets Sent"]  = format_number(stats.packets_sent);
                    results["Duration"]      = format_duration(elapsed);
                    results["Packets/sec"]   = format_number((int64_t)pps);
                    results["Language"]      = "Rust (FFI)";
                } else {
                    results["Error"] = "Failed to initialize (need admin/root for raw sockets)";
                    results["Language"] = "Rust (FFI)";
                }
                break;
            }

            case 4: {
                // ── UDP Flood - C module ─────────────────────
                attack_name = "UDP Flood [C]";
                std::cout << CLR_BYELLOW << "  >>> Launching UDP Flood (C module) <<<" << CLR_RESET << std::endl;
                std::cout << std::endl;

                udp_flood_t *ctx = nullptr;
                udp_flood_init(&ctx, target.c_str(), duration, threads, port,
                               DEFAULT_UDP_PAYLOAD, c_log_callback);
                auto stats = udp_flood_run(ctx);
                udp_flood_destroy(ctx);

                double elapsed = stats.end_time - stats.start_time;
                double pps = elapsed > 0 ? stats.packets_sent / elapsed : 0;

                results["Packets Sent"]  = format_number(stats.packets_sent);
                results["Bytes Sent"]    = format_bytes(stats.bytes_sent);
                results["Duration"]      = format_duration(elapsed);
                results["Packets/sec"]   = format_number((int64_t)pps);
                results["Language"]      = "C";
                break;
            }
        }

        // ── Stop hacker messages ─────────────────────────────
        g_hacker_running.store(false);
        if (hacker_thread.joinable()) hacker_thread.join();

        // ── Completion ───────────────────────────────────────
        std::cout << std::endl;
        std::cout << CLR_BGREEN << "  =============================================" << CLR_RESET << std::endl;
        std::cout << CLR_BGREEN << "         ATTACK COMPLETE" << CLR_RESET << std::endl;
        std::cout << CLR_BGREEN << "  =============================================" << CLR_RESET << std::endl;

        show_skull();
        show_results_table(attack_name, results);

        std::cout << CLR_YELLOW << "  Press Enter to return to menu..." << CLR_RESET;
        std::string dummy;
        std::getline(std::cin, dummy);
        clear_screen();
        show_banner();
    }

    return 0;
}
