# DoS Testing Tool Configuration
# Configure authorized targets before use

# SECURITY: Only these targets can be attacked (prevents misuse)
AUTHORIZED_TARGETS = [
    # Add your authorized test targets here
    # Examples:
    # "192.168.1.100",
    # "test.target.com",
    # "10.0.0.5",
]

# Attack Safety Limits
MAX_DURATION_SECONDS = 300  # Maximum 5 minutes per attack
MAX_THREADS = 1000          # Maximum concurrent threads
MAX_REQUESTS_PER_SECOND = 10000

# Default Attack Settings
DEFAULT_THREADS = 100
DEFAULT_DURATION = 30
DEFAULT_PORT = 80

# Attack Types
ATTACK_TYPES = {
    "http_flood": "HTTP Flood - High volume HTTP requests",
    "slowloris": "Slowloris - Slow HTTP connections",
    "tcp_syn": "TCP SYN Flood - Raw socket SYN packets",
    "udp_flood": "UDP Flood - UDP packet saturation",
    "http_post": "HTTP POST Flood - Large POST body attacks",
}

# Logging
LOG_FILE = "attacks.log"
LOG_LEVEL = "INFO"

# Web Interface
HOST = "0.0.0.0"
PORT = 5000
DEBUG = False