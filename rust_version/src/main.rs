mod attacks;
mod config;

use std::io::{self, Write};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};
use std::collections::HashMap;

use rand::Rng;

use attacks::http_flood::HttpFlood;
use attacks::slowloris::Slowloris;
use attacks::tcp_syn::TcpSynFlood;
use attacks::udp_flood::UdpFlood;
use config::*;

// ─── ANSI color helpers ───────────────────────────────────────────────────────

const RED: &str = "\x1b[31m";
const GREEN: &str = "\x1b[32m";
const YELLOW: &str = "\x1b[33m";
const CYAN: &str = "\x1b[36m";
const WHITE: &str = "\x1b[37m";
const MAGENTA: &str = "\x1b[35m";
const LIGHT_GREEN: &str = "\x1b[92m";
const LIGHT_BLACK: &str = "\x1b[90m";
const RESET: &str = "\x1b[0m";

// ─── Static data ──────────────────────────────────────────────────────────────

const BANNER_LINES: &[&str] = &[
    " \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557}  \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557} \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557}    \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557} \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557}  \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557} \u{2588}\u{2588}\u{2557}",
    " \u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{2588}\u{2588}\u{2557}\u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{2550}\u{2588}\u{2588}\u{2557}\u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{2550}\u{2550}\u{255d}    \u{255a}\u{2550}\u{2550}\u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{255d}\u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{2550}\u{2588}\u{2588}\u{2557}\u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{2550}\u{2588}\u{2588}\u{2557}\u{2588}\u{2588}\u{2551}",
    " \u{2588}\u{2588}\u{2551}  \u{2588}\u{2588}\u{2551}\u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2551}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557}       \u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2551}\u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2551}\u{2588}\u{2588}\u{2551}",
    " \u{2588}\u{2588}\u{2551}  \u{2588}\u{2588}\u{2551}\u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2551}\u{255a}\u{2550}\u{2550}\u{2550}\u{2550}\u{2588}\u{2588}\u{2551}       \u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2551}\u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2551}\u{2588}\u{2588}\u{2551}",
    " \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2554}\u{255d}\u{255a}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2554}\u{255d}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2551}       \u{2588}\u{2588}\u{2551}   \u{255a}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2554}\u{255d}\u{255a}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2554}\u{255d}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557}",
    " \u{255a}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{255d}  \u{255a}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{255d} \u{255a}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{255d}       \u{255a}\u{2550}\u{255d}    \u{255a}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{255d}  \u{255a}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{255d} \u{255a}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{255d}",
];

const BOX_LINES: &[&str] = &[
    "\u{2554}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2557}",
    "\u{2551}           Denial of Service Tool                  \u{2551}",
    "\u{255a}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{255d}",
];

const SKULL: &[&str] = &[
    "     \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}",
    "   \u{2588}\u{2588}          \u{2588}\u{2588}",
    " \u{2588}\u{2588}    \u{2588}\u{2588}  \u{2588}\u{2588}    \u{2588}\u{2588}",
    " \u{2588}\u{2588}    \u{2588}\u{2588}  \u{2588}\u{2588}    \u{2588}\u{2588}",
    " \u{2588}\u{2588}              \u{2588}\u{2588}",
    "   \u{2588}\u{2588}  \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}  \u{2588}\u{2588}",
    "     \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}",
    "     \u{2588}\u{2588} \u{2588}\u{2588} \u{2588}\u{2588} \u{2588}\u{2588}",
    "     \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}",
];

const HACKER_MESSAGES: &[&str] = &[
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
];

const MATRIX_CHARS: &[u8] = b"abcdefghijklmnopqrstuvwxyz0123456789@#$%^&*()!~";

const CONTENT_WIDTH: usize = 66;

const COUNTDOWN_3: &[&str] = &[
    "\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557} ",
    "\u{255a}\u{2550}\u{2550}\u{2550}\u{2550}\u{2588}\u{2588}\u{2557}",
    " \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2554}\u{255d}",
    " \u{255a}\u{2550}\u{2550}\u{2550}\u{2588}\u{2588}\u{2557}",
    " \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2554}\u{255d}",
    " \u{255a}\u{2550}\u{2550}\u{2550}\u{2550}\u{255d} ",
];

const COUNTDOWN_2: &[&str] = &[
    "\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557} ",
    "\u{255a}\u{2550}\u{2550}\u{2550}\u{2588}\u{2588}\u{2551}",
    "  \u{2588}\u{2588}\u{2588}\u{2554}\u{2550}\u{255d}",
    " \u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{255d} ",
    " \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557}",
    " \u{255a}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{255d}",
];

const COUNTDOWN_1: &[&str] = &[
    "  \u{2588}\u{2588}\u{2557}   ",
    " \u{2588}\u{2588}\u{2588}\u{2551}   ",
    " \u{255a}\u{2588}\u{2588}\u{2551}   ",
    "  \u{2588}\u{2588}\u{2551}   ",
    "  \u{2588}\u{2588}\u{2551}   ",
    "  \u{255a}\u{2550}\u{255d}   ",
];

const ATTACK_TEXT: &[&str] = &[
    " \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557} \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557} \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557}  \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557}\u{2588}\u{2588}\u{2557}  \u{2588}\u{2588}\u{2557}\u{2588}\u{2588}\u{2557}",
    "\u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{2588}\u{2588}\u{2557}\u{255a}\u{2550}\u{2550}\u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{255d}\u{255a}\u{2550}\u{2550}\u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{255d}\u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{2588}\u{2588}\u{2557}\u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{2550}\u{2550}\u{255d}\u{2588}\u{2588}\u{2551} \u{2588}\u{2588}\u{2554}\u{255d}\u{2588}\u{2588}\u{2551}",
    "\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2551}      \u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2551}\u{2588}\u{2588}\u{2551}     \u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2554}\u{255d} \u{2588}\u{2588}\u{2551}",
    "\u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2551}      \u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2554}\u{2550}\u{2550}\u{2588}\u{2588}\u{2551}\u{2588}\u{2588}\u{2551}     \u{2588}\u{2588}\u{2554}\u{2550}\u{2588}\u{2588}\u{2557} \u{255a}\u{2550}\u{255d}",
    "\u{2588}\u{2588}\u{2551}  \u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2551}      \u{2588}\u{2588}\u{2551}   \u{2588}\u{2588}\u{2551}  \u{2588}\u{2588}\u{2551}\u{255a}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2588}\u{2557}\u{2588}\u{2588}\u{2551}  \u{2588}\u{2588}\u{2557}\u{2588}\u{2588}\u{2557}",
    "\u{255a}\u{2550}\u{255d}  \u{255a}\u{2550}\u{255d}   \u{255a}\u{2550}\u{255d}      \u{255a}\u{2550}\u{255d}   \u{255a}\u{2550}\u{255d}  \u{255a}\u{2550}\u{255d} \u{255a}\u{2550}\u{2550}\u{2550}\u{2550}\u{2550}\u{255d}\u{255a}\u{2550}\u{255d}  \u{255a}\u{2550}\u{255d}\u{255a}\u{2550}\u{255d}",
];

// ─── Utility functions ────────────────────────────────────────────────────────

fn get_width() -> usize {
    // Try to read terminal width; fall back to 80
    #[cfg(target_os = "windows")]
    {
        // Use a simple heuristic on Windows
        if let Ok(val) = std::env::var("COLUMNS") {
            if let Ok(w) = val.parse::<usize>() {
                return w;
            }
        }
        // Try using the Windows console API via a quick command
        use std::process::Command;
        if let Ok(output) = Command::new("powershell")
            .args(["-NoProfile", "-Command", "[Console]::WindowWidth"])
            .output()
        {
            if let Ok(s) = String::from_utf8(output.stdout) {
                if let Ok(w) = s.trim().parse::<usize>() {
                    return w;
                }
            }
        }
        80
    }
    #[cfg(not(target_os = "windows"))]
    {
        use std::process::Command;
        if let Ok(output) = Command::new("tput").arg("cols").output() {
            if let Ok(s) = String::from_utf8(output.stdout) {
                if let Ok(w) = s.trim().parse::<usize>() {
                    return w;
                }
            }
        }
        80
    }
}

fn pad() -> String {
    let w = get_width();
    let spaces = if w > CONTENT_WIDTH {
        (w - CONTENT_WIDTH) / 2
    } else {
        0
    };
    " ".repeat(spaces)
}

fn clear() {
    if cfg!(target_os = "windows") {
        let _ = std::process::Command::new("cmd")
            .args(["/C", "cls"])
            .status();
    } else {
        let _ = std::process::Command::new("clear").status();
    }
}

fn typewrite(text: &str, color: &str, delay_ms: u64) {
    let p = pad();
    print!("{}", p);
    let stdout = io::stdout();
    let mut handle = stdout.lock();
    for ch in text.chars() {
        let _ = write!(handle, "{}{}{}", color, ch, RESET);
        let _ = handle.flush();
        std::thread::sleep(Duration::from_millis(delay_ms));
    }
    let _ = writeln!(handle);
}

fn loading_bar() {
    let bar_width = 40;
    println!();
    let p = pad();
    let stdout = io::stdout();
    let mut handle = stdout.lock();
    for i in 0..=bar_width {
        let filled = "\u{2588}".repeat(i);
        let empty = "\u{2591}".repeat(bar_width - i);
        let percent = (i * 100) / bar_width;
        let _ = write!(
            handle,
            "\r{}{}[{}{}] {}{}{}{}", p, RED, filled, empty, WHITE, percent, "%", RESET
        );
        let _ = handle.flush();
        std::thread::sleep(Duration::from_millis(30));
    }
    let _ = writeln!(handle);
}

fn glitch_text(text: &str, color: &str, glitch_rounds: u32) {
    let glitch_chars: &[u8] = b"!@#$%^&*()_+-=[]{}|;:',.<>?/~`";
    let p = pad();
    let stdout = io::stdout();
    let mut handle = stdout.lock();
    let mut rng = rand::thread_rng();

    for _ in 0..glitch_rounds {
        let glitched: String = text
            .chars()
            .map(|c| {
                if c != ' ' && rng.gen::<f64>() > 0.5 {
                    glitch_chars[rng.gen_range(0..glitch_chars.len())] as char
                } else {
                    c
                }
            })
            .collect();
        let _ = write!(handle, "\r{}{}{}{}", p, color, glitched, RESET);
        let _ = handle.flush();
        std::thread::sleep(Duration::from_millis(80));
    }
    let _ = write!(handle, "\r{}{}{}{}", p, color, text, RESET);
    let _ = handle.flush();
    let _ = writeln!(handle);
}

fn matrix_rain(duration_secs: f64) {
    let w = get_width();
    let mut columns = vec![false; w];
    let start = Instant::now();
    let dur = Duration::from_secs_f64(duration_secs);
    let mut rng = rand::thread_rng();
    clear();

    let stdout = io::stdout();
    let mut handle = stdout.lock();

    while start.elapsed() < dur {
        let mut line = String::with_capacity(w * 10);
        for i in 0..w {
            if rng.gen::<f64>() > 0.85 {
                columns[i] = true;
            }
            if columns[i] {
                let ch = MATRIX_CHARS[rng.gen_range(0..MATRIX_CHARS.len())] as char;
                if rng.gen::<f64>() > 0.7 {
                    line.push_str(GREEN);
                } else if rng.gen::<f64>() > 0.5 {
                    line.push_str(LIGHT_GREEN);
                } else {
                    line.push_str(LIGHT_BLACK);
                }
                line.push(ch);
                if rng.gen::<f64>() > 0.8 {
                    columns[i] = false;
                }
            } else {
                line.push(' ');
            }
        }
        let _ = writeln!(handle, "{}{}", line, RESET);
        let _ = handle.flush();
        std::thread::sleep(Duration::from_millis(40));
    }

    clear();
}

fn color_cycle_text(text: &str, cycles: u32) {
    let colors = [RED, YELLOW, GREEN, CYAN, MAGENTA, RED];
    let p = pad();
    let visible_len = text.chars().count();
    let inner_pad_n = if CONTENT_WIDTH > visible_len {
        (CONTENT_WIDTH - visible_len) / 2
    } else {
        0
    };
    let full_pad = format!("{}{}", p, " ".repeat(inner_pad_n));

    let stdout = io::stdout();
    let mut handle = stdout.lock();

    for _ in 0..cycles {
        for color in &colors {
            let _ = write!(handle, "\r{}{}{}{}", full_pad, color, text, RESET);
            let _ = handle.flush();
            std::thread::sleep(Duration::from_millis(100));
        }
    }
    let _ = write!(handle, "\r{}{}{}{}", full_pad, RED, text, RESET);
    let _ = handle.flush();
    let _ = writeln!(handle);
}

fn target_acquired(target: &str) {
    println!();
    let p = pad();
    let width = target.len() + 12;
    let border = "\u{2550}".repeat(width - 2);

    typewrite("Locking target...", LIGHT_BLACK, 30);
    std::thread::sleep(Duration::from_millis(300));

    let stdout = io::stdout();
    let mut handle = stdout.lock();

    for i in 1..=3 {
        let dots = ".".repeat(i);
        let spaces = " ".repeat(3 - i);
        let _ = write!(
            handle,
            "\r{}{}Scanning{}{}{}",
            p, YELLOW, dots, spaces, RESET
        );
        let _ = handle.flush();
        std::thread::sleep(Duration::from_millis(400));
    }
    let _ = writeln!(handle);
    drop(handle);

    std::thread::sleep(Duration::from_millis(200));
    println!(
        "{}{}\u{2554}{}\u{2557}{}",
        p, RED, border, RESET
    );
    std::thread::sleep(Duration::from_millis(100));
    println!(
        "{}{}\u{2551}{}  >>> {}{}{} <<<  {}\u{2551}{}",
        p, RED, WHITE, YELLOW, target, WHITE, RED, RESET
    );
    std::thread::sleep(Duration::from_millis(100));
    println!(
        "{}{}\u{255a}{}\u{255d}{}",
        p, RED, border, RESET
    );
    std::thread::sleep(Duration::from_millis(200));

    color_cycle_text("\u{2588}\u{2588}\u{2588}\u{2588} TARGET ACQUIRED \u{2588}\u{2588}\u{2588}\u{2588}", 3);
    println!();
}

fn countdown() {
    println!();
    let p = pad();
    let nums: &[&[&str]] = &[COUNTDOWN_3, COUNTDOWN_2, COUNTDOWN_1];

    let stdout = io::stdout();
    let mut handle = stdout.lock();

    for num in nums {
        let clear_lines = num.len();
        let inner_pad = " ".repeat((CONTENT_WIDTH.saturating_sub(10)) / 2);
        for line in *num {
            let _ = writeln!(handle, "{}{}{}{}{}", p, inner_pad, RED, line, RESET);
        }
        let _ = handle.flush();
        std::thread::sleep(Duration::from_millis(700));
        // Clear the lines we just printed
        for _ in 0..clear_lines {
            let _ = write!(handle, "\x1b[A\x1b[K");
        }
        let _ = handle.flush();
    }

    // Show ATTACK! text
    for line in ATTACK_TEXT {
        let _ = writeln!(handle, "{}{}{}{}", p, RED, line, RESET);
    }
    let _ = handle.flush();
    std::thread::sleep(Duration::from_secs(1));

    // Clear ATTACK! text
    for _ in 0..ATTACK_TEXT.len() {
        let _ = write!(handle, "\x1b[A\x1b[K");
    }
    let _ = handle.flush();
}

fn show_skull() {
    println!();
    let p = pad();
    let inner_pad = " ".repeat((CONTENT_WIDTH.saturating_sub(22)) / 2);
    for line in SKULL {
        println!("{}{}{}{}{}", p, inner_pad, RED, line, RESET);
        std::thread::sleep(Duration::from_millis(80));
    }
    println!();
}

fn log(message: &str, color: &str) {
    let timestamp = chrono_timestamp();
    let p = pad();
    println!(
        "{}{}[{}{}{}] {}{}{}",
        p, WHITE, GREEN, timestamp, WHITE, color, message, RESET
    );
}

fn chrono_timestamp() -> String {
    // Simple timestamp without external crate
    use std::time::SystemTime;
    let now = SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .unwrap_or_default();
    let total_secs = now.as_secs();
    let hours = (total_secs % 86400) / 3600;
    let mins = (total_secs % 3600) / 60;
    let secs = total_secs % 60;
    format!("{:02}:{:02}:{:02}", hours, mins, secs)
}

fn print_menu() {
    let p = pad();
    println!();
    println!(
        "{}{}[{}1{}]{} HTTP Flood       {}- {}High volume HTTP requests{}",
        p, RED, WHITE, RED, CYAN, WHITE, LIGHT_BLACK, RESET
    );
    println!(
        "{}{}[{}2{}]{} Slowloris        {}- {}Slow HTTP connections{}",
        p, RED, WHITE, RED, CYAN, WHITE, LIGHT_BLACK, RESET
    );
    println!(
        "{}{}[{}3{}]{} TCP SYN Flood    {}- {}Raw socket SYN packets{}",
        p, RED, WHITE, RED, CYAN, WHITE, LIGHT_BLACK, RESET
    );
    println!(
        "{}{}[{}4{}]{} UDP Flood        {}- {}UDP packet saturation{}",
        p, RED, WHITE, RED, CYAN, WHITE, LIGHT_BLACK, RESET
    );
    println!(
        "{}{}[{}5{}]{} Exit{}",
        p, RED, WHITE, RED, CYAN, RESET
    );
    println!();
}

fn get_input(prompt: &str, default: Option<&str>) -> String {
    let suffix = match default {
        Some(d) => format!(" {}[{}]", LIGHT_BLACK, d),
        None => String::new(),
    };
    let p = pad();
    print!(
        "{}{}> {}{}{}: {}",
        p, RED, WHITE, prompt, suffix, GREEN
    );
    let _ = io::stdout().flush();

    let mut input = String::new();
    let _ = io::stdin().read_line(&mut input);
    print!("{}", RESET);
    let _ = io::stdout().flush();

    let trimmed = input.trim().to_string();
    if trimmed.is_empty() {
        default.unwrap_or("").to_string()
    } else {
        trimmed
    }
}

fn attack_callback(message: &str) {
    if message.contains("STATS") || message.contains("Requests sent:") || message.contains("Connections") || message.contains("Packets") {
        log(message, YELLOW);
    } else if message.contains("Error") || message.contains("Failed") || message.contains("[!]") {
        log(message, RED);
    } else if message.to_lowercase().contains("completed") {
        log(message, GREEN);
    } else {
        log(message, CYAN);
    }
}

fn hacker_message_loop(stop: Arc<AtomicBool>) {
    let mut rng = rand::thread_rng();
    while !stop.load(Ordering::Relaxed) {
        // Sleep 3-7 seconds, checking stop every 500ms
        let wait_ms = rng.gen_range(3000..=7000u64);
        let start = Instant::now();
        while start.elapsed() < Duration::from_millis(wait_ms) {
            if stop.load(Ordering::Relaxed) {
                return;
            }
            std::thread::sleep(Duration::from_millis(500));
        }
        if !stop.load(Ordering::Relaxed) {
            let msg = HACKER_MESSAGES[rng.gen_range(0..HACKER_MESSAGES.len())];
            let timestamp = chrono_timestamp();
            let p = pad();
            println!(
                "{}{}[{}{}{}] {}{}{}",
                p, WHITE, GREEN, timestamp, WHITE, MAGENTA, msg, RESET
            );
        }
    }
}

fn animate_banner() {
    matrix_rain(2.5);
    clear();
    println!();

    for line in BANNER_LINES {
        glitch_text(line, RED, 3);
        std::thread::sleep(Duration::from_millis(50));
    }

    println!();

    typewrite("Initializing systems...", LIGHT_BLACK, 20);
    loading_bar();

    println!();

    for line in BOX_LINES {
        typewrite(line, YELLOW, 8);
    }

    println!();
    std::thread::sleep(Duration::from_millis(300));

    let status_msgs = [
        ("[+] Modules loaded", GREEN),
        ("[+] Attack vectors ready", GREEN),
        ("[+] System online", GREEN),
    ];
    for (msg, color) in &status_msgs {
        typewrite(msg, color, 20);
        std::thread::sleep(Duration::from_millis(200));
    }

    println!();
    std::thread::sleep(Duration::from_millis(500));
}

fn run_attack(attack_type: &str, target: &str, port: u16, duration: u64, threads: u32) {
    countdown();

    println!();
    log(&"=".repeat(50), RED);
    log("ATTACK INITIATED", RED);
    log(&format!("Type: {}", attack_type.to_uppercase()), YELLOW);
    log(&format!("Target: {}:{}", target, port), YELLOW);
    log(
        &format!("Duration: {}s | Threads: {}", duration, threads),
        YELLOW,
    );
    log(&"=".repeat(50), RED);
    println!();

    // Start hacker messages thread
    let hacker_stop = Arc::new(AtomicBool::new(false));
    let hs = hacker_stop.clone();
    let hacker_handle = std::thread::spawn(move || {
        hacker_message_loop(hs);
    });

    let cb: Arc<dyn Fn(&str) + Send + Sync> = Arc::new(|msg: &str| {
        attack_callback(msg);
    });

    let stats: HashMap<String, String> = match attack_type {
        "http_flood" => {
            let attack = HttpFlood::new(
                target.to_string(),
                duration,
                threads,
                port,
                cb,
            );
            attack.run()
        }
        "slowloris" => {
            let attack = Slowloris::new(
                target.to_string(),
                duration,
                threads,
                port,
                cb,
            );
            attack.run()
        }
        "tcp_syn" => {
            let attack = TcpSynFlood::new(
                target.to_string(),
                duration,
                threads,
                port,
                cb,
            );
            attack.run()
        }
        "udp_flood" => {
            let attack = UdpFlood::new(
                target.to_string(),
                duration,
                threads,
                port,
                cb,
            );
            attack.run()
        }
        _ => {
            log("Unknown attack type", RED);
            HashMap::new()
        }
    };

    hacker_stop.store(true, Ordering::SeqCst);
    std::thread::sleep(Duration::from_millis(500));
    let _ = hacker_handle.join();

    show_skull();

    log(&"=".repeat(50), GREEN);
    log("ATTACK COMPLETE", GREEN);
    for (key, value) in &stats {
        if key != "start_time" && key != "end_time" {
            let label: String = key
                .replace('_', " ")
                .split_whitespace()
                .map(|w| {
                    let mut c = w.chars();
                    match c.next() {
                        None => String::new(),
                        Some(f) => {
                            f.to_uppercase().to_string() + &c.as_str().to_lowercase()
                        }
                    }
                })
                .collect::<Vec<_>>()
                .join(" ");
            log(&format!("{}: {}", label, value), WHITE);
        }
    }
    log(&"=".repeat(50), GREEN);
}

fn main() {
    // Enable ANSI escape codes on Windows 10+
    #[cfg(target_os = "windows")]
    {
        let _ = enable_virtual_terminal_processing();
    }

    // Handle Ctrl+C gracefully
    let _ = ctrlc::set_handler(move || {
        println!("\n  {}Exiting...{}", YELLOW, RESET);
        std::process::exit(0);
    });

    animate_banner();

    loop {
        print_menu();
        let choice = get_input("Select attack type", None);

        if choice == "5" {
            log("Exiting...", YELLOW);
            std::process::exit(0);
        }

        let attack_type = match choice.as_str() {
            "1" => "http_flood",
            "2" => "slowloris",
            "3" => "tcp_syn",
            "4" => "udp_flood",
            _ => {
                log("Invalid choice", RED);
                continue;
            }
        };

        println!();
        let raw_target = get_input("Target (IP or domain)", None);
        if raw_target.is_empty() {
            log("Target is required", RED);
            continue;
        }
        // Strip protocol prefix and trailing slash so attacks don't double it
        let target = raw_target
            .trim_start_matches("https://")
            .trim_start_matches("http://")
            .trim_end_matches('/')
            .to_string();

        target_acquired(&target);

        let port: u16 = get_input("Port", Some("80"))
            .parse()
            .unwrap_or(DEFAULT_PORT);

        let duration: u64 = get_input("Duration (seconds)", Some("30"))
            .parse()
            .unwrap_or(DEFAULT_DURATION);

        let threads: u32 = get_input("Threads", Some("100"))
            .parse()
            .unwrap_or(DEFAULT_THREADS);

        if duration > MAX_DURATION_SECONDS {
            log(
                &format!("Duration exceeds max of {}s", MAX_DURATION_SECONDS),
                RED,
            );
            continue;
        }

        if threads > MAX_THREADS {
            log(&format!("Threads exceed max of {}", MAX_THREADS), RED);
            continue;
        }

        // Check authorized targets
        if !AUTHORIZED_TARGETS.is_empty()
            && !AUTHORIZED_TARGETS.contains(&target.as_str())
        {
            log("Target not in authorized targets list", RED);
            continue;
        }

        run_attack(attack_type, &target, port, duration, threads);

        println!();
        let again = get_input("Run another attack? (y/n)", Some("y"));
        if again.to_lowercase() != "y" {
            log("Exiting...", YELLOW);
            break;
        }
    }
}

/// Enable ANSI escape sequences on Windows 10+ consoles.
#[cfg(target_os = "windows")]
fn enable_virtual_terminal_processing() -> Result<(), Box<dyn std::error::Error>> {
    use std::os::windows::io::AsRawHandle;

    const ENABLE_VIRTUAL_TERMINAL_PROCESSING: u32 = 0x0004;

    #[link(name = "kernel32")]
    extern "system" {
        fn GetConsoleMode(handle: *mut std::ffi::c_void, mode: *mut u32) -> i32;
        fn SetConsoleMode(handle: *mut std::ffi::c_void, mode: u32) -> i32;
    }

    let handle = io::stdout().as_raw_handle();
    let mut mode: u32 = 0;

    unsafe {
        if GetConsoleMode(handle, &mut mode) == 0 {
            return Err("GetConsoleMode failed".into());
        }
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if SetConsoleMode(handle, mode) == 0 {
            return Err("SetConsoleMode failed".into());
        }
    }

    Ok(())
}
