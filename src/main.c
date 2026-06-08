#include "server.h"
#include <sys/epoll.h>

volatile int running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = 0;
    }
}

void handle_client_request(int client_fd, ServerConfig *config) {
    ClientContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.client_fd = client_fd;
    
    // Read request
    char buffer[BUFFER_SIZE];
    ssize_t bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes <= 0) {
        close(client_fd);
        return;
    }
    
    buffer[bytes] = '\0';
    memcpy(ctx.buffer, buffer, bytes + 1);
    ctx.buffer_len = bytes;
    
    // Parse request
    parse_http_request(&ctx);
    
    // Get client IP
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    char client_ip[INET_ADDRSTRLEN];
    getpeername(client_fd, (struct sockaddr*)&addr, &addr_len);
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    // Route request
    Route *route = find_route(config, ctx.request.method, ctx.request.path);
    
    if (route && route->is_proxy) {
        handle_proxy_request(&ctx, route);
        log_access(config, client_ip, ctx.request.method, ctx.request.path, 200);
    } else if (route && !route->is_proxy) {
        // Internal redirect (simplified)
        strcpy(ctx.request.path, route->target);
        handle_static_file(&ctx, config);
        log_access(config, client_ip, ctx.request.method, ctx.request.path, 200);
    } else {
        // Try static file
        handle_static_file(&ctx, config);
        log_access(config, client_ip, ctx.request.method, ctx.request.path, 200);
    }
    
    close(client_fd);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    ServerConfig config;
    init_server_config(&config);
    
    // Load config file
    if (load_config(&config, "config/server.conf") != 0) {
        printf("Using default configuration\n");
    }
    
    // Create log directory
    mkdir("logs", 0755);
    
    // Create server socket
    int server_fd = create_server_socket(config.port);
    if (server_fd < 0) {
        log_error(&config, "Failed to create server socket on port %d", config.port);
        return 1;
    }
    
    printf("Custom HTTP Server running on port %d\n", config.port);
    printf("Root directory: %s\n", config.root_dir);
    printf("Logging: %s\n", config.enable_logging ? "enabled" : "disabled");
    
    // Setup epoll
    int epoll_fd;
    setup_epoll(server_fd, &epoll_fd);
    
    struct epoll_event events[MAX_EVENTS];
    
    // Main event loop
    while (running) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                // New connection
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                
                if (client_fd >= 0) {
                    // Set non-blocking
                    int flags = fcntl(client_fd, F_GETFL, 0);
                    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                    
                    // Handle request immediately (simplified - in production use worker pool)
                    handle_client_request(client_fd, &config);
                }
            }
        }
    }
    
    close(server_fd);
    close(epoll_fd);
    printf("\nServer shutdown complete\n");
    
    return 0;
}