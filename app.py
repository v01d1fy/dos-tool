from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
import threading
import time
import logging
import random
import validators
from datetime import datetime

from config import (
    AUTHORIZED_TARGETS, MAX_DURATION_SECONDS, MAX_THREADS,
    ATTACK_TYPES, HOST, PORT, DEBUG, DEFAULT_THREADS, DEFAULT_DURATION
)
from attacks.http_flood import HTTPFlood
from attacks.slowloris import Slowloris
from attacks.tcp_syn import TCPSYNFlood
from attacks.udp_flood import UDPFlood

app = Flask(__name__)
app.config['SECRET_KEY'] = 'your-secret-key-change-in-production'
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('attacks.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

# Global attack state
active_attacks = {}
attack_history = []

class AttackManager:
    def __init__(self, attack_id, attack_type, target, duration, threads, port):
        self.attack_id = attack_id
        self.attack_type = attack_type
        self.target = target
        self.duration = duration
        self.threads = threads
        self.port = port
        self.attack_instance = None
        self.thread = None
        self.logs = []
        self.status = "starting"
        self.stats = {}
    
    def log_callback(self, message):
        timestamp = datetime.now().strftime("%H:%M:%S")
        log_entry = f"[{timestamp}] {message}"
        self.logs.append(log_entry)
        logger.info(f"[{self.attack_id}] {message}")
        
        # Emit to connected clients
        socketio.emit('attack_log', {
            'attack_id': self.attack_id,
            'message': message,
            'timestamp': timestamp
        })
    
    def run_attack(self):
        self.status = "running"
        self.log_callback(f"Initializing {self.attack_type} attack...")
        
        try:
            if self.attack_type == "http_flood":
                self.attack_instance = HTTPFlood(
                    self.target, self.duration, self.threads, self.port, self.log_callback
                )
            elif self.attack_type == "slowloris":
                self.attack_instance = Slowloris(
                    self.target, self.duration, self.threads, self.port, self.log_callback
                )
            elif self.attack_type == "tcp_syn":
                self.attack_instance = TCPSYNFlood(
                    self.target, self.duration, self.threads, self.port, self.log_callback
                )
            elif self.attack_type == "udp_flood":
                self.attack_instance = UDPFlood(
                    self.target, self.duration, self.threads, self.port, callback=self.log_callback
                )
            else:
                self.log_callback(f"Unknown attack type: {self.attack_type}")
                self.status = "failed"
                return
            
            self.stats = self.attack_instance.run()
            self.status = "completed"
            self.log_callback("Attack completed successfully")
            
        except Exception as e:
            self.log_callback(f"Error: {str(e)}")
            self.status = "failed"
        
        # Add to history
        attack_history.append({
            'id': self.attack_id,
            'type': self.attack_type,
            'target': self.target,
            'duration': self.duration,
            'threads': self.threads,
            'status': self.status,
            'stats': self.stats,
            'logs': self.logs
        })
        
        # Remove from active
        if self.attack_id in active_attacks:
            del active_attacks[self.attack_id]
    
    def start(self):
        self.thread = threading.Thread(target=self.run_attack)
        self.thread.daemon = True
        self.thread.start()
    
    def stop(self):
        if self.attack_instance:
            self.attack_instance.stop()
        self.status = "stopped"
        self.log_callback("Attack stopped by user")

# Routes
@app.route('/')
def index():
    return render_template('index.html', attack_types=ATTACK_TYPES)

@app.route('/api/start_attack', methods=['POST'])
def start_attack():
    data = request.json
    
    target = data.get('target', '').strip()
    attack_type = data.get('attack_type', '')
    duration = int(data.get('duration', DEFAULT_DURATION))
    threads = int(data.get('threads', DEFAULT_THREADS))
    port = int(data.get('port', 80))
    
    # Validate target authorization
    if AUTHORIZED_TARGETS and target not in AUTHORIZED_TARGETS:
        return jsonify({
            'success': False,
            'error': 'Target not in authorized list. Add to config.py AUTHORIZED_TARGETS first.'
        }), 403
    
    # Validate inputs
    if not target:
        return jsonify({'success': False, 'error': 'Target is required'}), 400
    
    if attack_type not in ATTACK_TYPES:
        return jsonify({'success': False, 'error': 'Invalid attack type'}), 400
    
    if duration > MAX_DURATION_SECONDS:
        return jsonify({'success': False, 'error': f'Duration exceeds maximum of {MAX_DURATION_SECONDS}s'}), 400
    
    if threads > MAX_THREADS:
        return jsonify({'success': False, 'error': f'Threads exceed maximum of {MAX_THREADS}'}), 400
    
    # Generate attack ID
    attack_id = f"attack_{int(time.time())}_{random.randint(1000, 9999)}"
    
    # Create and start attack
    manager = AttackManager(attack_id, attack_type, target, duration, threads, port)
    active_attacks[attack_id] = manager
    manager.start()
    
    logger.info(f"Started attack {attack_id}: {attack_type} on {target}")
    
    return jsonify({
        'success': True,
        'attack_id': attack_id,
        'message': f'{attack_type} attack started on {target}'
    })

@app.route('/api/stop_attack', methods=['POST'])
def stop_attack():
    data = request.json
    attack_id = data.get('attack_id')
    
    if attack_id in active_attacks:
        active_attacks[attack_id].stop()
        return jsonify({'success': True, 'message': 'Attack stopped'})
    else:
        return jsonify({'success': False, 'error': 'Attack not found'}), 404

@app.route('/api/active_attacks', methods=['GET'])
def get_active_attacks():
    attacks = []
    for aid, manager in active_attacks.items():
        attacks.append({
            'id': aid,
            'type': manager.attack_type,
            'target': manager.target,
            'status': manager.status,
            'duration': manager.duration,
            'threads': manager.threads
        })
    return jsonify(attacks)

@app.route('/api/attack_history', methods=['GET'])
def get_history():
    return jsonify(attack_history[-50:])  # Last 50 attacks

@app.route('/api/attack_logs/<attack_id>', methods=['GET'])
def get_attack_logs(attack_id):
    # Check active attacks first
    if attack_id in active_attacks:
        return jsonify({'logs': active_attacks[attack_id].logs})
    
    # Check history
    for attack in attack_history:
        if attack['id'] == attack_id:
            return jsonify({'logs': attack['logs']})
    
    return jsonify({'error': 'Attack not found'}), 404

@socketio.on('connect')
def handle_connect():
    logger.info('Client connected')
    emit('connected', {'message': 'Connected to DoS Testing Tool'})

@socketio.on('disconnect')
def handle_disconnect():
    logger.info('Client disconnected')

if __name__ == '__main__':
    print("=" * 60)
    print("DoS Testing Tool - Authorized Penetration Testing Only")
    print("=" * 60)
    print(f"\nServer starting on http://{HOST}:{PORT}")
    print(f"Authorized targets: {AUTHORIZED_TARGETS if AUTHORIZED_TARGETS else 'ALL (configure config.py)'}")
    print(f"\nOpen your browser and navigate to http://localhost:{PORT}")
    print("=" * 60)
    
    socketio.run(app, host=HOST, port=PORT, debug=DEBUG, allow_unsafe_werkzeug=True)