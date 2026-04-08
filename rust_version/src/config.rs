/// Authorized targets whitelist. If non-empty, only these targets may be attacked.
pub const AUTHORIZED_TARGETS: &[&str] = &[];

/// Maximum attack duration in seconds.
pub const MAX_DURATION_SECONDS: u64 = 300;

/// Maximum number of threads / concurrent workers.
pub const MAX_THREADS: u32 = 1000;

/// Default number of threads when user presses Enter without input.
pub const DEFAULT_THREADS: u32 = 100;

/// Default attack duration in seconds.
pub const DEFAULT_DURATION: u64 = 30;

/// Default target port.
pub const DEFAULT_PORT: u16 = 80;

/// Human-readable attack type names keyed by menu index.
pub fn attack_name(index: u32) -> &'static str {
    match index {
        1 => "HTTP Flood",
        2 => "Slowloris",
        3 => "TCP SYN Flood",
        4 => "UDP Flood",
        _ => "Unknown",
    }
}
