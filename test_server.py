from flask import Flask

app = Flask(__name__)

@app.route('/', defaults={'path': ''})
@app.route('/<path:path>')
def catch_all(path):
    return "Test server is running!", 200

if __name__ == '__main__':
    print("Test server running on http://localhost:8888")
    app.run(host='0.0.0.0', port=8888, debug=False)
