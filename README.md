# Custom HTTP Server + Reverse Proxy

A high-performance HTTP/1.1 server built from scratch in C with epoll-based concurrency and reverse proxy capabilities.

## Features

- ✅ Pure C implementation with POSIX sockets
- ✅ HTTP/1.1 request parsing and routing
- ✅ Static file serving with MIME type detection
- ✅ Reverse proxy with load balancing support
- ✅ Epoll-based event loop for high concurrency
- ✅ Configurable routing via server.conf
- ✅ Access and error logging
- ✅ Memory optimization and non-blocking I/O

## Quick Start

```bash
# Clone the repository
git clone https://github.com/NICHOLASKARANI/custom-http-server.git
cd custom-http-server

# Build the server
make

# Create necessary directories
mkdir -p logs www

# Copy configuration
cp config/server.conf .

# Run the server
make run