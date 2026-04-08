/*
 * DoS Testing Tool - CLI Interface (C Version)
 * ASCII art banner, matrix rain, glitch text, color cycling, countdown, live stats
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include "compat.h"

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define usleep_ms(ms) Sleep(ms)
#define usleep_us(us) Sleep((us) / 1000 > 0 ? (us) / 1000 : 1)
#else
#include <unistd.h>
#include <sys/ioctl.h>
#define usleep_ms(ms) usleep((ms) * 1000)
#define usleep_us(us) usleep(us)
#endif

#include "config.h"
#include "attacks/http_flood.h"
#include "attacks/slowloris.h"
#include "attacks/tcp_syn.h"
#include "attacks/udp_flood.h"

/* =========================================================================
 *  ANSI Color Codes
 * ========================================================================= */
#define RESET       "\033[0m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define CYAN        "\033[36m"
#define MAGENTA     "\033[35m"
#define WHITE       "\033[37m"
#define LIGHT_GREEN "\033[92m"
#define DARK_GRAY   "\033[90m"

/* =========================================================================
 *  ASCII Art & Data
 * ========================================================================= */
static const char *BANNER_LINES[] = {
    " \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97    \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97",
    " \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d    \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91",
    " \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97       \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91",
    " \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91       \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91",
    " \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91       \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97",
    " \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d       \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d    \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d",
};
#define NUM_BANNER_LINES 6

static const char *BOX_LINES[] = {
    "\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x97",
    "\xe2\x95\x91           Denial of Service Tool                  \xe2\x95\x91",
    "\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d",
};
#define NUM_BOX_LINES 3

static const char *SKULL[] = {
    "     \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
    "   \xe2\x96\x88\xe2\x96\x88          \xe2\x96\x88\xe2\x96\x88",
    " \xe2\x96\x88\xe2\x96\x88    \xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88    \xe2\x96\x88\xe2\x96\x88",
    " \xe2\x96\x88\xe2\x96\x88    \xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88    \xe2\x96\x88\xe2\x96\x88",
    " \xe2\x96\x88\xe2\x96\x88              \xe2\x96\x88\xe2\x96\x88",
    "   \xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88",
    "     \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
    "     \xe2\x96\x88\xe2\x96\x88 \xe2\x96\x88\xe2\x96\x88 \xe2\x96\x88\xe2\x96\x88 \xe2\x96\x88\xe2\x96\x88",
    "     \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88",
};
#define NUM_SKULL_LINES 9

static const char *HACKER_MESSAGES[] = {
    "Bypassing firewall...",
    "Injecting packets...",
    "Spoofing headers...",
    "Rotating user agents...",
    "Flooding connection pool...",
    "Saturating bandwidth...",
    "Overloading buffers...",
    "Fragmenting packets...",
    "Exhausting resources...",
    "Amplifying traffic...",
    "Cycling attack vectors...",
    "Escalating payload size...",
};
#define NUM_HACKER_MESSAGES 12

static const char MATRIX_CHARS[] = "abcdefghijklmnopqrstuvwxyz0123456789@#$%^&*()!~";
#define MATRIX_CHARS_LEN 47

#define CONTENT_WIDTH 66

/* =========================================================================
 *  Utility Functions
 * ========================================================================= */

static int get_terminal_width(void)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
#else
    struct winsize w;
    if (ioctl(0, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
    return 80;
#endif
}

static void print_pad(void)
{
    int w = get_terminal_width();
    int spaces = (w - CONTENT_WIDTH) / 2;
    if (spaces < 0) spaces = 0;
    for (int i = 0; i < spaces; i++) putchar(' ');
}

static void clear_screen(void)
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

static void typewrite(const char *text, const char *color, int delay_ms)
{
    print_pad();
    for (int i = 0; text[i] != '\0'; i++) {
        printf("%s%c" RESET, color, text[i]);
        fflush(stdout);
        usleep_ms(delay_ms);
    }
    printf("\n");
}

static void loading_bar(void)
{
    int bar_width = 40;
    printf("\n");
    for (int i = 0; i <= bar_width; i++) {
        int percent = (int)((i * 100.0) / bar_width);
        printf("\r");
        print_pad();
        printf(RED "[");
        for (int j = 0; j < i; j++) printf("\xe2\x96\x88");
        for (int j = 0; j < bar_width - i; j++) printf("\xe2\x96\x91");
        printf("] " WHITE "%d%%" RESET, percent);
        fflush(stdout);
        usleep_ms(30);
    }
    printf("\n");
}

static void glitch_text(const char *text, const char *color, int glitch_rounds)
{
    const char *glitch_chars = "!@#$%^&*()_+-=[]{}|;:',.<>?/~`";
    int glen = (int)strlen(glitch_chars);
    int tlen = (int)strlen(text);
    char *buf = (char *)malloc(tlen + 1);
    if (!buf) return;

    for (int r = 0; r < glitch_rounds; r++) {
        for (int i = 0; i < tlen; i++) {
            if (text[i] != ' ' && (rand() % 100) > 50) {
                buf[i] = glitch_chars[rand() % glen];
            } else {
                buf[i] = text[i];
            }
        }
        buf[tlen] = '\0';
        printf("\r");
        print_pad();
        printf("%s%s" RESET, color, buf);
        fflush(stdout);
        usleep_ms(80);
    }
    printf("\r");
    print_pad();
    printf("%s%s" RESET "\n", color, text);
    fflush(stdout);

    free(buf);
}

static void matrix_rain(double duration_sec)
{
    int w = get_terminal_width();
    int *columns = (int *)calloc(w, sizeof(int));
    if (!columns) return;

    double end_time;
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    end_time = (double)counter.QuadPart / (double)freq.QuadPart + duration_sec;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    end_time = ts.tv_sec + ts.tv_nsec / 1e9 + duration_sec;
#endif

    clear_screen();

    while (1) {
        double now;
#ifdef _WIN32
        QueryPerformanceCounter(&counter);
        now = (double)counter.QuadPart / (double)freq.QuadPart;
#else
        clock_gettime(CLOCK_MONOTONIC, &ts);
        now = ts.tv_sec + ts.tv_nsec / 1e9;
#endif
        if (now >= end_time) break;

        for (int i = 0; i < w; i++) {
            if ((rand() % 100) > 85) columns[i] = 1;
            if (columns[i]) {
                int r = rand() % 100;
                if (r > 70)
                    printf(GREEN "%c", MATRIX_CHARS[rand() % MATRIX_CHARS_LEN]);
                else if (r > 50)
                    printf(LIGHT_GREEN "%c", MATRIX_CHARS[rand() % MATRIX_CHARS_LEN]);
                else
                    printf(DARK_GRAY "%c", MATRIX_CHARS[rand() % MATRIX_CHARS_LEN]);
                if ((rand() % 100) > 80) columns[i] = 0;
            } else {
                putchar(' ');
            }
        }
        printf(RESET "\n");
        fflush(stdout);
        usleep_ms(40);
    }

    free(columns);
    clear_screen();
}

static void color_cycle_text(const char *text, int cycles)
{
    static const char *colors[] = { RED, YELLOW, GREEN, CYAN, MAGENTA, RED };
    int num_colors = 6;
    int w = get_terminal_width();
    int tlen = (int)strlen(text);
    int inner = (CONTENT_WIDTH - tlen) / 2;
    if (inner < 0) inner = 0;
    int left = (w - CONTENT_WIDTH) / 2;
    if (left < 0) left = 0;

    for (int c = 0; c < cycles; c++) {
        for (int ci = 0; ci < num_colors; ci++) {
            printf("\r");
            for (int i = 0; i < left + inner; i++) putchar(' ');
            printf("%s%s" RESET, colors[ci], text);
            fflush(stdout);
            usleep_ms(100);
        }
    }
    printf("\r");
    for (int i = 0; i < left + inner; i++) putchar(' ');
    printf(RED "%s" RESET "\n", text);
    fflush(stdout);
}

static void target_acquired_display(const char *target)
{
    int tlen = (int)strlen(target);
    int width = tlen + 12;

    printf("\n");
    typewrite("Locking target...", DARK_GRAY, 30);
    usleep_ms(300);

    for (int i = 0; i < 3; i++) {
        printf("\r");
        print_pad();
        printf(YELLOW "Scanning");
        for (int j = 0; j <= i; j++) putchar('.');
        for (int j = 0; j < 3 - i; j++) putchar(' ');
        printf(RESET);
        fflush(stdout);
        usleep_ms(400);
    }
    printf("\n");

    usleep_ms(200);

    /* Top border */
    print_pad();
    printf(RED);
    printf("\xe2\x95\x94");
    for (int i = 0; i < width - 2; i++) printf("\xe2\x95\x90");
    printf("\xe2\x95\x97" RESET "\n");
    usleep_ms(100);

    /* Target line */
    print_pad();
    printf(RED "\xe2\x95\x91" WHITE "  >>> " YELLOW "%s" WHITE " <<<  " RED "\xe2\x95\x91" RESET "\n", target);
    usleep_ms(100);

    /* Bottom border */
    print_pad();
    printf(RED);
    printf("\xe2\x95\x9a");
    for (int i = 0; i < width - 2; i++) printf("\xe2\x95\x90");
    printf("\xe2\x95\x9d" RESET "\n");
    usleep_ms(200);

    color_cycle_text("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88 TARGET ACQUIRED \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88", 3);
    printf("\n");
}

static void countdown_animation(void)
{
    /* Number 3 */
    static const char *num3[] = {
        "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 ",
        "\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97",
        " \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d",
        " \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97",
        " \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d",
        " \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d ",
    };

    /* Number 2 */
    static const char *num2[] = {
        "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 ",
        "\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91",
        "  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x9d",
        " \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d ",
        " \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97",
        " \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d",
    };

    /* Number 1 */
    static const char *num1[] = {
        "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97   ",
        " \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   ",
        " \xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   ",
        "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   ",
        "  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   ",
        "  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d   ",
    };

    const char **nums[] = { num3, num2, num1 };
    int num_lines = 6;
    int inner_pad_count = (CONTENT_WIDTH - 10) / 2;

    printf("\n");

    for (int n = 0; n < 3; n++) {
        for (int l = 0; l < num_lines; l++) {
            print_pad();
            for (int s = 0; s < inner_pad_count; s++) putchar(' ');
            printf(RED "%s" RESET "\n", nums[n][l]);
        }
        fflush(stdout);
        usleep_ms(700);
        /* Clear the lines we just printed */
        for (int l = 0; l < num_lines; l++) {
            printf("\033[A\033[K");
        }
        fflush(stdout);
    }

    /* ATTACK! text */
    static const char *attack_text[] = {
        " \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97",
        "\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91 \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91",
        "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91      \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x9d \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91",
        "\xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91      \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91     \xe2\x96\x88\xe2\x96\x88\xe2\x95\x94\xe2\x95\x90\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97 \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d",
        "\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91      \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91   \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x91\xe2\x95\x9a\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x95\x97\xe2\x96\x88\xe2\x96\x88\xe2\x95\x97",
        "\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d   \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d      \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d   \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d  \xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x9d",
    };

    for (int l = 0; l < 6; l++) {
        print_pad();
        printf(RED "%s" RESET "\n", attack_text[l]);
    }
    fflush(stdout);
    usleep_ms(1000);

    for (int l = 0; l < 6; l++) {
        printf("\033[A\033[K");
    }
    fflush(stdout);
}

static void show_skull(void)
{
    int inner = (CONTENT_WIDTH - 22) / 2;
    if (inner < 0) inner = 0;
    printf("\n");
    for (int i = 0; i < NUM_SKULL_LINES; i++) {
        print_pad();
        for (int s = 0; s < inner; s++) putchar(' ');
        printf(RED "%s" RESET "\n", SKULL[i]);
        usleep_ms(80);
    }
    printf("\n");
}

static void log_msg(const char *message, const char *color)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[16];
    strftime(ts, sizeof(ts), "%H:%M:%S", t);
    print_pad();
    printf(WHITE "[" GREEN "%s" WHITE "] %s%s" RESET "\n", ts, color, message);
}

/* =========================================================================
 *  Hacker Message Background Thread
 * ========================================================================= */

typedef struct {
    volatile int *stop_flag;
} hacker_thread_arg_t;

static void *hacker_message_thread(void *arg)
{
    hacker_thread_arg_t *hta = (hacker_thread_arg_t *)arg;

    while (!*(hta->stop_flag)) {
        /* Sleep 3-7 seconds, checking stop flag */
        int delay = (rand() % 5 + 3) * 10;  /* in 100ms increments */
        for (int i = 0; i < delay && !*(hta->stop_flag); i++) {
            usleep_ms(100);
        }
        if (!*(hta->stop_flag)) {
            const char *msg = HACKER_MESSAGES[rand() % NUM_HACKER_MESSAGES];
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char ts[16];
            strftime(ts, sizeof(ts), "%H:%M:%S", t);
            print_pad();
            printf(WHITE "[" GREEN "%s" WHITE "] " MAGENTA "%s" RESET "\n", ts, msg);
            fflush(stdout);
        }
    }

    return NULL;
}

/* =========================================================================
 *  Attack Callback
 * ========================================================================= */

static void attack_callback(const char *message)
{
    if (strstr(message, "Requests sent:") ||
        strstr(message, "Connections") ||
        strstr(message, "Packets")) {
        log_msg(message, YELLOW);
    } else if (strstr(message, "Error") || strstr(message, "Failed")) {
        log_msg(message, RED);
    } else if (strstr(message, "completed") || strstr(message, "Completed")) {
        log_msg(message, GREEN);
    } else {
        log_msg(message, CYAN);
    }
    fflush(stdout);
}

/* =========================================================================
 *  Banner & Menu
 * ========================================================================= */

static void animate_banner(void)
{
    matrix_rain(2.5);
    clear_screen();
    printf("\n");

    for (int i = 0; i < NUM_BANNER_LINES; i++) {
        glitch_text(BANNER_LINES[i], RED, 3);
        usleep_ms(50);
    }
    printf("\n");

    typewrite("Initializing systems...", DARK_GRAY, 20);
    loading_bar();
    printf("\n");

    for (int i = 0; i < NUM_BOX_LINES; i++) {
        typewrite(BOX_LINES[i], YELLOW, 8);
    }
    printf("\n");
    usleep_ms(300);

    typewrite("[+] Modules loaded", GREEN, 20);
    usleep_ms(200);
    typewrite("[+] Attack vectors ready", GREEN, 20);
    usleep_ms(200);
    typewrite("[+] System online", GREEN, 20);
    printf("\n");
    usleep_ms(500);
}

static void print_menu(void)
{
    printf("\n");
    print_pad(); printf(RED "[" WHITE "1" RED "]" CYAN " HTTP Flood       " WHITE "- " DARK_GRAY "High volume HTTP requests" RESET "\n");
    print_pad(); printf(RED "[" WHITE "2" RED "]" CYAN " Slowloris        " WHITE "- " DARK_GRAY "Slow HTTP connections" RESET "\n");
    print_pad(); printf(RED "[" WHITE "3" RED "]" CYAN " TCP SYN Flood    " WHITE "- " DARK_GRAY "Raw socket SYN packets" RESET "\n");
    print_pad(); printf(RED "[" WHITE "4" RED "]" CYAN " UDP Flood        " WHITE "- " DARK_GRAY "UDP packet saturation" RESET "\n");
    print_pad(); printf(RED "[" WHITE "5" RED "]" CYAN " Exit" RESET "\n");
    printf("\n");
}

static void get_input(const char *prompt, const char *defval, char *buf, int bufsize)
{
    print_pad();
    if (defval && defval[0]) {
        printf(RED ">" WHITE " %s " DARK_GRAY "[%s]: " GREEN, prompt, defval);
    } else {
        printf(RED ">" WHITE " %s: " GREEN, prompt);
    }
    fflush(stdout);

    if (fgets(buf, bufsize, stdin) == NULL) {
        buf[0] = '\0';
    }
    printf(RESET);

    /* Strip newline */
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }

    /* Use default if empty */
    if (buf[0] == '\0' && defval) {
        snprintf(buf, bufsize, "%s", defval);
    }
}

/* =========================================================================
 *  Run Attack
 * ========================================================================= */

static void run_attack(int attack_type, const char *target, int port,
                       int duration, int threads)
{
    char msg[256];
    volatile int hacker_stop = 0;
    pthread_t hacker_tid;
    hacker_thread_arg_t hta;

    countdown_animation();

    printf("\n");
    {
        char sep[64];
        memset(sep, '=', 50);
        sep[50] = '\0';
        log_msg(sep, RED);
    }
    log_msg("ATTACK INITIATED", RED);

    switch (attack_type) {
        case ATTACK_HTTP_FLOOD:  snprintf(msg, sizeof(msg), "Type: HTTP_FLOOD"); break;
        case ATTACK_SLOWLORIS:   snprintf(msg, sizeof(msg), "Type: SLOWLORIS"); break;
        case ATTACK_TCP_SYN:     snprintf(msg, sizeof(msg), "Type: TCP_SYN"); break;
        case ATTACK_UDP_FLOOD:   snprintf(msg, sizeof(msg), "Type: UDP_FLOOD"); break;
        default:                 snprintf(msg, sizeof(msg), "Type: UNKNOWN"); break;
    }
    log_msg(msg, YELLOW);

    snprintf(msg, sizeof(msg), "Target: %s:%d", target, port);
    log_msg(msg, YELLOW);

    snprintf(msg, sizeof(msg), "Duration: %ds | Threads: %d", duration, threads);
    log_msg(msg, YELLOW);

    {
        char sep[64];
        memset(sep, '=', 50);
        sep[50] = '\0';
        log_msg(sep, RED);
    }
    printf("\n");
    fflush(stdout);

    /* Start hacker messages thread */
    hta.stop_flag = &hacker_stop;
    pthread_create(&hacker_tid, NULL, hacker_message_thread, &hta);

    /* Run the attack */
    if (attack_type == ATTACK_HTTP_FLOOD) {
        http_flood_t ctx;
        http_flood_init(&ctx, target, duration, threads, port, attack_callback);
        http_flood_stats_t stats = http_flood_run(&ctx);

        hacker_stop = 1;
        usleep_ms(500);
        show_skull();

        {
            char sep[64]; memset(sep, '=', 50); sep[50] = '\0';
            log_msg(sep, GREEN);
        }
        log_msg("ATTACK COMPLETE", GREEN);
        snprintf(msg, sizeof(msg), "Requests Sent: %lld", stats.requests_sent);
        log_msg(msg, WHITE);
        snprintf(msg, sizeof(msg), "Failed Requests: %lld", stats.failed_requests);
        log_msg(msg, WHITE);
        {
            char sep[64]; memset(sep, '=', 50); sep[50] = '\0';
            log_msg(sep, GREEN);
        }
        http_flood_destroy(&ctx);

    } else if (attack_type == ATTACK_SLOWLORIS) {
        slowloris_t ctx;
        slowloris_init(&ctx, target, duration, threads, port, attack_callback);
        slowloris_stats_t stats = slowloris_run(&ctx);

        hacker_stop = 1;
        usleep_ms(500);
        show_skull();

        {
            char sep[64]; memset(sep, '=', 50); sep[50] = '\0';
            log_msg(sep, GREEN);
        }
        log_msg("ATTACK COMPLETE", GREEN);
        snprintf(msg, sizeof(msg), "Connections Opened: %lld", stats.connections_opened);
        log_msg(msg, WHITE);
        snprintf(msg, sizeof(msg), "Connections Active: %lld", stats.connections_active);
        log_msg(msg, WHITE);
        {
            char sep[64]; memset(sep, '=', 50); sep[50] = '\0';
            log_msg(sep, GREEN);
        }
        slowloris_destroy(&ctx);

    } else if (attack_type == ATTACK_TCP_SYN) {
        tcp_syn_t ctx;
        tcp_syn_init(&ctx, target, duration, threads, port, attack_callback);
        tcp_syn_stats_t stats = tcp_syn_run(&ctx);

        hacker_stop = 1;
        usleep_ms(500);
        show_skull();

        {
            char sep[64]; memset(sep, '=', 50); sep[50] = '\0';
            log_msg(sep, GREEN);
        }
        log_msg("ATTACK COMPLETE", GREEN);
        snprintf(msg, sizeof(msg), "Packets Sent: %lld", stats.packets_sent);
        log_msg(msg, WHITE);
        {
            char sep[64]; memset(sep, '=', 50); sep[50] = '\0';
            log_msg(sep, GREEN);
        }
        tcp_syn_destroy(&ctx);

    } else if (attack_type == ATTACK_UDP_FLOOD) {
        udp_flood_t ctx;
        udp_flood_init(&ctx, target, duration, threads, port,
                       DEFAULT_PACKET_SIZE, attack_callback);
        udp_flood_stats_t stats = udp_flood_run(&ctx);

        hacker_stop = 1;
        usleep_ms(500);
        show_skull();

        {
            char sep[64]; memset(sep, '=', 50); sep[50] = '\0';
            log_msg(sep, GREEN);
        }
        log_msg("ATTACK COMPLETE", GREEN);
        snprintf(msg, sizeof(msg), "Packets Sent: %lld", stats.packets_sent);
        log_msg(msg, WHITE);
        {
            double mb = stats.bytes_sent / (1024.0 * 1024.0);
            snprintf(msg, sizeof(msg), "Bytes Sent: %.2f MB", mb);
            log_msg(msg, WHITE);
        }
        {
            char sep[64]; memset(sep, '=', 50); sep[50] = '\0';
            log_msg(sep, GREEN);
        }
        udp_flood_destroy(&ctx);
    }

    pthread_join(hacker_tid, NULL);
}

/* =========================================================================
 *  Main
 * ========================================================================= */

int main(int argc, char *argv[])
{
    char buf[256];

    (void)argc;
    (void)argv;

    srand((unsigned int)time(NULL));

#ifdef _WIN32
    /* Enable ANSI escape sequences on Windows 10+ */
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        GetConsoleMode(hOut, &mode);
        mode |= 0x0004; /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */
        SetConsoleMode(hOut, mode);
    }
    /* Set console output to UTF-8 */
    SetConsoleOutputCP(65001);

    /* Initialize Winsock */
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            fprintf(stderr, "WSAStartup failed\n");
            return 1;
        }
    }
#endif

    animate_banner();

    while (1) {
        int attack_type;
        char target[256];
        int port, duration, threads;
        char msg[256];

        print_menu();
        get_input("Select attack type", NULL, buf, sizeof(buf));

        if (strcmp(buf, "5") == 0) {
            log_msg("Exiting...", YELLOW);
            break;
        }

        if (strcmp(buf, "1") == 0) {
            attack_type = ATTACK_HTTP_FLOOD;
        } else if (strcmp(buf, "2") == 0) {
            attack_type = ATTACK_SLOWLORIS;
        } else if (strcmp(buf, "3") == 0) {
            attack_type = ATTACK_TCP_SYN;
        } else if (strcmp(buf, "4") == 0) {
            attack_type = ATTACK_UDP_FLOOD;
        } else {
            log_msg("Invalid choice", RED);
            continue;
        }

        printf("\n");
        get_input("Target (IP or domain)", NULL, target, sizeof(target));
        if (target[0] == '\0') {
            log_msg("Target is required", RED);
            continue;
        }

        target_acquired_display(target);

        get_input("Port", "80", buf, sizeof(buf));
        port = atoi(buf);

        get_input("Duration (seconds)", "30", buf, sizeof(buf));
        duration = atoi(buf);

        get_input("Threads", "100", buf, sizeof(buf));
        threads = atoi(buf);

        if (duration > MAX_DURATION_SECONDS) {
            snprintf(msg, sizeof(msg), "Duration exceeds max of %ds", MAX_DURATION_SECONDS);
            log_msg(msg, RED);
            continue;
        }

        if (threads > MAX_THREADS) {
            snprintf(msg, sizeof(msg), "Threads exceed max of %d", MAX_THREADS);
            log_msg(msg, RED);
            continue;
        }

        run_attack(attack_type, target, port, duration, threads);

        printf("\n");
        get_input("Run another attack? (y/n)", "y", buf, sizeof(buf));
        if (buf[0] != 'y' && buf[0] != 'Y') {
            log_msg("Exiting...", YELLOW);
            break;
        }
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
