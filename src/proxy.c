#include "server.h"

void handle_proxy_request(ClientContext *ctx, Route *route) {
    // Create connection to backend
    int backend_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (backend_fd < 0) {
        log_error(NULL, "Failed to create backend socket");
        HTTPResponse response;
        set_response(&response, 502, "Bad Gateway");
        send_response(ctx->client_fd, &response);
        return;
    }
    
    struct hostent *server = gethostbyname(route->proxy_host);
    if (!server) {
        log_error(NULL, "Cannot resolve host: %s", route->proxy_host);
        close(backend_fd);
        HTTPResponse response;
        set_response(&response, 502, "Bad Gateway");
        send_response(ctx->client_fd, &response);
        return;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
    addr.sin_port = htons(route->proxy_port);
    
    if (connect(backend_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_error(NULL, "Cannot connect to backend: %s:%d", route->proxy_host, route->proxy_port);
        close(backend_fd);
        HTTPResponse response;
        set_response(&response, 502, "Bad Gateway");
        send_response(ctx->client_fd, &response);
        return;
    }
    
    // Forward request to backend
    send(backend_fd, ctx->buffer, ctx->buffer_len, 0);
    
    // Get response from backend
    char response_buffer[BUFFER_SIZE];
    ssize_t total_received = 0;
    ssize_t bytes;
    
    while ((bytes = recv(backend_fd, response_buffer + total_received, 
                         BUFFER_SIZE - total_received - 1, 0)) > 0) {
        total_received += bytes;
        if (total_received >= BUFFER_SIZE - 1) break;
    }
    
    response_buffer[total_received] = '\0';
    
    // Forward response to client
    send(ctx->client_fd, response_buffer, total_received, 0);
    close(backend_fd);
}