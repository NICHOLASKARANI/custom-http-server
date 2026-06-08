#!/usr/bin/env python3
"""
Production HTTP Server with Reverse Proxy - Fixed for Windows
"""

import socket
import threading
import os
import sys
import json
import time
import logging
from datetime import datetime
from urllib.parse import urlparse, unquote
from pathlib import Path
import mimetypes
from collections import defaultdict

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

class HTTPRequest:
    def __init__(self, data):
        self.method = ""
        self.path = "/"
        self.version = ""
        self.headers = {}
        self.body = ""
        self.query_params = {}
        self.parse(data)
    
    def parse(self, data):
        try:
            parts = data.split('\r\n\r\n', 1)
            header_part = parts[0]
            if len(parts) > 1:
                self.body = parts[1]
            
            lines = header_part.split('\r\n')
            if lines:
                request_line = lines[0].split(' ')
                if len(request_line) >= 2:
                    self.method = request_line[0]
                    full_path = request_line[1]
                    
                    if '?' in full_path:
                        self.path, query = full_path.split('?', 1)
                        for param in query.split('&'):
                            if '=' in param:
                                key, value = param.split('=', 1)
                                self.query_params[key] = unquote(value)
                    else:
                        self.path = full_path
                    
                    self.version = request_line[2] if len(request_line) > 2 else "HTTP/1.1"
            
            for line in lines[1:]:
                if ': ' in line:
                    key, value = line.split(': ', 1)
                    self.headers[key] = value
        except Exception as e:
            logging.error(f"Parse error: {e}")

class HTTPResponse:
    def __init__(self, status_code=200, status_text="OK"):
        self.status_code = status_code
        self.status_text = status_text
        self.headers = {
            "Server": "PythonHTTP/1.1",
            "Connection": "close"
        }
        self.body = ""
    
    def set_header(self, key, value):
        self.headers[key] = value
    
    def set_body(self, body, content_type="text/html"):
        self.body = body if isinstance(body, str) else str(body)
        self.set_header("Content-Type", content_type)
        self.set_header("Content-Length", str(len(self.body.encode('utf-8'))))
    
    def to_bytes(self):
        response = f"HTTP/1.1 {self.status_code} {self.status_text}\r\n"
        for key, value in self.headers.items():
            response += f"{key}: {value}\r\n"
        response += "\r\n"
        return response.encode('utf-8') + self.body.encode('utf-8')

class SimpleHTTPServer:
    def __init__(self, port=8080):
        self.port = port
        self.running = False
        self.socket = None
        self.www_dir = Path("www")
        self.www_dir.mkdir(exist_ok=True)
        
        # Create sample HTML files with proper encoding
        self.create_sample_files()
    
    def create_sample_files(self):
        """Create sample HTML files with proper encoding"""
        # Main index.html
        index_content = """<!DOCTYPE html>
<html>
<head>
    <title>Custom HTTP Server</title>
    <style>
        body { font-family: 'Segoe UI', Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; background: #f0f0f0; }
        .container { background: white; border-radius: 10px; padding: 30px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; }
        .success { color: green; font-weight: bold; }
        .info { background: #f0f0f0; padding: 10px; border-radius: 5px; margin: 10px 0; }
        button { background: #007bff; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; margin: 5px; }
        button:hover { background: #0056b3; }
        pre { background: #f8f9fa; padding: 10px; border-radius: 5px; overflow-x: auto; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Custom HTTP Server Running!</h1>
        <p class="success">Server is online and responding</p>
        
        <div class="info">
            <h3>Server Information:</h3>
            <p>Port: 8080</p>
            <p>Protocol: HTTP/1.1</p>
            <p>Concurrency: Multi-threaded</p>
        </div>
        
        <h3>Available Endpoints:</h3>
        <ul>
            <li><a href="/">/</a> - Home page</li>
            <li><a href="/api/hello">/api/hello</a> - JSON response</li>
            <li><a href="/api/time">/api/time</a> - Server time</li>
            <li><a href="/test.html">/test.html</a> - Static file</li>
        </ul>
        
        <h3>Test Endpoints:</h3>
        <button onclick="testHello()">Test /api/hello</button>
        <button onclick="testTime()">Test /api/time</button>
        <div id="result" style="margin-top: 20px;"></div>
    </div>
    
    <script>
        async function testHello() {
            const result = document.getElementById('result');
            result.innerHTML = 'Loading...';
            try {
                const response = await fetch('/api/hello');
                const data = await response.json();
                result.innerHTML = '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
            } catch(e) {
                result.innerHTML = 'Error: ' + e.message;
            }
        }
        
        async function testTime() {
            const result = document.getElementById('result');
            result.innerHTML = 'Loading...';
            try {
                const response = await fetch('/api/time');
                const data = await response.json();
                result.innerHTML = '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
            } catch(e) {
                result.innerHTML = 'Error: ' + e.message;
            }
        }
    </script>
</body>
</html>"""
        
        # Save with explicit UTF-8 encoding
        with open(self.www_dir / "index.html", 'w', encoding='utf-8') as f:
            f.write(index_content)
        
        # Create test.html
        test_content = """<!DOCTYPE html>
<html>
<head><title>Test Page</title></head>
<body>
    <h1>Static File Test</h1>
    <p>This file is being served as a static asset.</p>
    <p>Server timestamp: {}</p>
</body>
</html>""".format(datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
        
        with open(self.www_dir / "test.html", 'w', encoding='utf-8') as f:
            f.write(test_content)
        
        print(" Sample files created in ./www directory")
    
    def handle_api_hello(self, request):
        """API endpoint that returns JSON"""
        response = HTTPResponse(200, "OK")
        data = {
            "message": "Hello from Custom HTTP Server",
            "status": "success",
            "timestamp": datetime.now().isoformat(),
            "method": request.method,
            "path": request.path
        }
        response.set_body(json.dumps(data, indent=2), "application/json")
        return response
    
    def handle_api_time(self, request):
        """API endpoint that returns server time"""
        response = HTTPResponse(200, "OK")
        data = {
            "server_time": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "timestamp": time.time(),
            "timezone": str(datetime.now().astimezone().tzinfo)
        }
        response.set_body(json.dumps(data, indent=2), "application/json")
        return response
    
    def serve_static_file(self, path):
        """Serve static files from www directory"""
        try:
            # Security: prevent directory traversal
            if '..' in path:
                response = HTTPResponse(403, "Forbidden")
                response.set_body("<h1>403 Forbidden</h1>")
                return response
            
            # Remove leading slash
            if path.startswith('/'):
                path = path[1:]
            
            if not path or path == '':
                path = 'index.html'
            
            file_path = self.www_dir / path
            
            # Check if file exists
            if not file_path.exists() or not file_path.is_file():
                response = HTTPResponse(404, "Not Found")
                response.set_body(f"<h1>404 Not Found</h1><p>File {path} not found</p>")
                return response
            
            # Determine content type
            content_type = "text/html"
            if path.endswith('.css'):
                content_type = "text/css"
            elif path.endswith('.js'):
                content_type = "application/javascript"
            elif path.endswith('.json'):
                content_type = "application/json"
            elif path.endswith('.png'):
                content_type = "image/png"
            elif path.endswith('.jpg') or path.endswith('.jpeg'):
                content_type = "image/jpeg"
            
            # Read and serve file
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            response = HTTPResponse(200, "OK")
            response.set_body(content, content_type)
            return response
            
        except Exception as e:
            logging.error(f"Error serving file: {e}")
            response = HTTPResponse(500, "Internal Server Error")
            response.set_body(f"<h1>500 Error</h1><p>{str(e)}</p>")
            return response
    
    def handle_client(self, client_socket, address):
        """Handle individual client connection"""
        try:
            # Receive request
            data = client_socket.recv(8192).decode('utf-8', errors='ignore')
            if not data:
                return
            
            request = HTTPRequest(data)
            
            # Log request
            print(f" {address[0]}:{address[1]} - {request.method} {request.path}")
            
            # Route request
            if request.path == '/api/hello':
                response = self.handle_api_hello(request)
            elif request.path == '/api/time':
                response = self.handle_api_time(request)
            else:
                response = self.serve_static_file(request.path)
            
            # Send response
            client_socket.send(response.to_bytes())
            
        except Exception as e:
            logging.error(f"Error: {e}")
        finally:
            client_socket.close()
    
    def start(self):
        """Start the HTTP server"""
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind(('localhost', self.port))
        self.socket.listen(100)
        self.running = True
        
        print("\n" + "="*60)
        print(" CUSTOM HTTP SERVER RUNNING")
        print("="*60)
        print(f" Address: http://localhost:{self.port}")
        print(f" Directory: {self.www_dir.absolute()}")
        print(f" Threads: Multi-threaded")
        print("="*60)
        print("\n Available endpoints:")
        print("  → http://localhost:8080/")
        print("  → http://localhost:8080/test.html")
        print("  → http://localhost:8080/api/hello")
        print("  → http://localhost:8080/api/time")
        print("\n Press Ctrl+C to stop the server\n")
        print("="*60 + "\n")
        
        try:
            while self.running:
                client_socket, address = self.socket.accept()
                thread = threading.Thread(target=self.handle_client, args=(client_socket, address))
                thread.daemon = True
                thread.start()
        except KeyboardInterrupt:
            print("\n\n Shutting down server...")
        finally:
            self.stop()
    
    def stop(self):
        """Stop the server"""
        self.running = False
        if self.socket:
            self.socket.close()
        print(" Server stopped")

def main():
    # Create www directory if it doesn't exist
    Path("www").mkdir(exist_ok=True)
    
    # Start server
    server = SimpleHTTPServer(port=8080)
    server.start()

if __name__ == "__main__":
    main()

    # Add after the existing handlers
import requests

def handle_proxy(self, request):
    """Reverse proxy to external API"""
    try:
        # Forward to external API
        target_url = f"https://httpbin.org{request.path}"
        response = requests.get(target_url)
        
        resp = HTTPResponse(response.status_code, "OK")
        resp.set_body(response.text, "application/json")
        return resp
    except Exception as e:
        resp = HTTPResponse(502, "Bad Gateway")
        resp.set_body(f'{{"error": "{str(e)}"}}', "application/json")
        return resp

# Add to routing in handle_client method:
elif request.path.startswith('/proxy/'):
    response = self.handle_proxy(request)

    # Add WebSocket upgrade support
def handle_websocket(self, client_socket, request):
    """WebSocket handshake"""
    key = request.headers.get('Sec-WebSocket-Key')
    if key:
        accept = self.generate_websocket_accept(key)
        response = f"""HTTP/1.1 101 Switching Protocols\r\n
Upgrade: websocket\r\n
Connection: Upgrade\r\n
Sec-WebSocket-Accept: {accept}\r\n\r\n"""
        client_socket.send(response.encode())
        # Now handle WebSocket frames
        return True
    return False

    class LoadBalancer:
    def __init__(self):
        self.backends = [
            "http://localhost:3000",
            "http://localhost:3001",
            "http://localhost:3002"
        ]
        self.current = 0
    
    def get_backend(self):
        backend = self.backends[self.current]
        self.current = (self.current + 1) % len(self.backends)
        return backend

        import psutil
import platform

def handle_metrics(self, request):
    """System metrics endpoint"""
    metrics = {
        "server": {
            "status": "running",
            "uptime_seconds": time.time() - self.start_time,
            "requests_served": self.request_count
        },
        "system": {
            "cpu_percent": psutil.cpu_percent(),
            "memory_percent": psutil.virtual_memory().percent,
            "platform": platform.platform()
        }
    }
    response = HTTPResponse(200, "OK")
    response.set_body(json.dumps(metrics, indent=2), "application/json")
    return response