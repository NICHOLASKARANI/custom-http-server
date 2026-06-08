#include "server_win.h"

CRITICAL_SECTION log_cs;
HANDLE completion_port;
ServerConfig global_config;

void init_server_config(ServerConfig *config) {
    config->route_count = 0;
    strcpy(config->root_dir, ".\\www");
    config->port = 8080;
    config->enable_logging = 1;
    strcpy(config->access_log, ".\\logs\\access.log");
    strcpy(config->error_log, ".\\logs\\error.log");
}

int load_config(ServerConfig *config, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;
    
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char key[256], value[256];
        if (sscanf(line, "%255s = %255s", key, value) == 2) {
            if (strcmp(key, "port") == 0) config->port = atoi(value);
            else if (strcmp(key, "root_dir") == 0) strcpy(config->root_dir, value);
            else if (strcmp(key, "enable_logging") == 0) config->enable_logging = atoi(value);
            else if (strcmp(key, "access_log") == 0) strcpy(config->access_log, value);
            else if (strcmp(key, "error_log") == 0) strcpy(config->error_log, value);
        }
        
        char route_method[16], route_path[256], route_target[512];
        if (sscanf(line, "route %15s %255s %511s", route_method, route_path, route_target) == 3) {
            Route *r = &config->routes[config->route_count++];
            strcpy(r->method, route_method);
            strcpy(r->path, route_path);
            
            if (strstr(route_target, "http://") == route_target) {
                r->is_proxy = 1;
                char *host_start = route_target + 7;
                char *colon = strchr(host_start, ':');
                char *slash = strchr(host_start, '/');
                
                if (colon && colon < slash) {
                    strncpy(r->proxy_host, host_start, colon - host_start);
                    r->proxy_host[colon - host_start] = '\0';
                    r->proxy_port = atoi(colon + 1);
                } else if (slash) {
                    strncpy(r->proxy_host, host_start, slash - host_start);
                    r->proxy_host[slash - host_start] = '\0';
                    r->proxy_port = 80;
                }
            } else {
                r->is_proxy = 0;
                strcpy(r->target, route_target);
            }
        }
    }
    fclose(fp);
    return 0;
}

SOCKET create_server_socket(int port) {
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == INVALID_SOCKET) return INVALID_SOCKET;
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(server_fd);
        return INVALID_SOCKET;
    }
    
    if (listen(server_fd, BACKLOG) == SOCKET_ERROR) {
        closesocket(server_fd);
        return INVALID_SOCKET;
    }
    
    return server_fd;
}

void log_access(ServerConfig *config, const char *client_ip, const char *method, const char *path, int status) {
    if (!config->enable_logging) return;
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%d/%b/%Y:%H:%M:%S", tm_info);
    
    EnterCriticalSection(&log_cs);
    FILE *log = fopen(config->access_log, "a");
    if (log) {
        fprintf(log, "%s - - [%s] \"%s %s HTTP/1.1\" %d\n", client_ip, timestamp, method, path, status);
        fclose(log);
    }
    LeaveCriticalSection(&log_cs);
}

void log_error(ServerConfig *config, const char *format, ...) {
    va_list args;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    EnterCriticalSection(&log_cs);
    FILE *log = fopen(config->error_log, "a");
    if (log) {
        fprintf(log, "[%s] ", timestamp);
        va_start(args, format);
        vfprintf(log, format, args);
        va_end(args);
        fprintf(log, "\n");
        fclose(log);
    }
    LeaveCriticalSection(&log_cs);
}

char* get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    
    return "application/octet-stream";
}

void set_response(HTTPResponse *response, int status, const char *status_text) {
    response->status_code = status;
    strcpy(response->status_text, status_text);
    response->header_count = 0;
    response->body_length = 0;
    
    add_response_header(response, "Server", "CustomHTTP-Windows/1.1");
    add_response_header(response, "Connection", "close");
}

void add_response_header(HTTPResponse *response, const char *key, const char *value) {
    if (response->header_count < MAX_HEADERS) {
        snprintf(response->headers[response->header_count], 255, "%s: %s", key, value);
        response->header_count++;
    }
}

void send_response(SOCKET client_fd, HTTPResponse *response) {
    char buffer[BUFFER_SIZE];
    int len = snprintf(buffer, BUFFER_SIZE, "HTTP/1.1 %d %s\r\n", 
                      response->status_code, response->status_text);
    
    for (int i = 0; i < response->header_count; i++) {
        len += snprintf(buffer + len, BUFFER_SIZE - len, "%s\r\n", response->headers[i]);
    }
    
    len += snprintf(buffer + len, BUFFER_SIZE - len, "\r\n");
    
    send(client_fd, buffer, len, 0);
    
    if (response->body_length > 0) {
        send(client_fd, response->body, response->body_length, 0);
    }
}

void send_file_response(SOCKET client_fd, const char *filepath, const char *mime_type) {
    HANDLE hFile = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, 
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        HTTPResponse response;
        set_response(&response, 404, "Not Found");
        add_response_header(&response, "Content-Type", "text/html");
        snprintf(response.body, BUFFER_SIZE, "<h1>404 Not Found</h1><p>File not found: %s</p>", filepath);
        response.body_length = strlen(response.body);
        send_response(client_fd, &response);
        return;
    }
    
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    
    char header_buffer[512];
    int header_len = snprintf(header_buffer, 512, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "\r\n", mime_type, fileSize.QuadPart);
    
    send(client_fd, header_buffer, header_len, 0);
    
    // Send file in chunks
    char file_buffer[8192];
    DWORD bytes_read;
    while (ReadFile(hFile, file_buffer, sizeof(file_buffer), &bytes_read, NULL) && bytes_read > 0) {
        send(client_fd, file_buffer, bytes_read, 0);
    }
    
    CloseHandle(hFile);
}

void parse_http_request(ClientContext *ctx) {
    HTTPRequest *req = &ctx->request;
    memset(req, 0, sizeof(HTTPRequest));
    
    char *line = strtok(ctx->buffer, "\r\n");
    if (!line) return;
    
    sscanf(line, "%15s %4095s %15s", req->method, req->path, req->version);
    
    req->header_count = 0;
    while ((line = strtok(NULL, "\r\n")) != NULL && strlen(line) > 0) {
        if (req->header_count < MAX_HEADERS) {
            strncpy(req->headers[req->header_count], line, 255);
            req->header_count++;
        }
    }
    
    char *body_start = strstr(ctx->buffer, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        ctx->request.body_length = ctx->buffer_len - (body_start - ctx->buffer);
        if (ctx->request.body_length < BUFFER_SIZE) {
            memcpy(ctx->request.body, body_start, ctx->request.body_length);
        }
    }
}

Route* find_route(ServerConfig *config, const char *method, const char *path) {
    for (int i = 0; i < config->route_count; i++) {
        Route *r = &config->routes[i];
        if (stricmp(r->method, method) == 0) {
            if (strcmp(r->path, path) == 0) return r;
            
            int path_len = strlen(r->path);
            if (r->is_proxy && strncmp(r->path, path, path_len) == 0) {
                return r;
            }
        }
    }
    return NULL;
}

void handle_static_file(ClientContext *ctx, ServerConfig *config) {
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s%s", config->root_dir, ctx->request.path);
    
    // Default to index.html
    if (full_path[strlen(full_path) - 1] == '\\' || full_path[strlen(full_path) - 1] == '/') {
        strcat(full_path, "index.html");
    }
    
    // Security: prevent directory traversal
    if (strstr(full_path, "..")) {
        HTTPResponse response;
        set_response(&response, 403, "Forbidden");
        send_response(ctx->client_fd, &response);
        return;
    }
    
    char *mime = get_mime_type(full_path);
    send_file_response(ctx->client_fd, full_path, mime);
}

void handle_proxy_request(ClientContext *ctx, Route *route) {
    SOCKET backend_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (backend_fd == INVALID_SOCKET) {
        log_error(&global_config, "Failed to create backend socket");
        HTTPResponse response;
        set_response(&response, 502, "Bad Gateway");
        send_response(ctx->client_fd, &response);
        return;
    }
    
    struct hostent *server = gethostbyname(route->proxy_host);
    if (!server) {
        log_error(&global_config, "Cannot resolve host: %s", route->proxy_host);
        closesocket(backend_fd);
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
    
    if (connect(backend_fd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        log_error(&global_config, "Cannot connect to backend: %s:%d", route->proxy_host, route->proxy_port);
        closesocket(backend_fd);
        HTTPResponse response;
        set_response(&response, 502, "Bad Gateway");
        send_response(ctx->client_fd, &response);
        return;
    }
    
    send(backend_fd, ctx->buffer, ctx->buffer_len, 0);
    
    char response_buffer[BUFFER_SIZE];
    int total_received = 0;
    int bytes;
    
    while ((bytes = recv(backend_fd, response_buffer + total_received, 
                         BUFFER_SIZE - total_received - 1, 0)) > 0) {
        total_received += bytes;
        if (total_received >= BUFFER_SIZE - 1) break;
    }
    
    response_buffer[total_received] = '\0';
    send(ctx->client_fd, response_buffer, total_received, 0);
    closesocket(backend_fd);
}

DWORD WINAPI worker_thread(LPVOID lpParam) {
    DWORD bytesTransferred;
    ClientContext *ctx;
    OVERLAPPED *overlapped;
    ULONG_PTR completionKey;
    
    while (TRUE) {
        BOOL success = GetQueuedCompletionStatus(completion_port, &bytesTransferred,
                                                  &completionKey, &overlapped, INFINITE);
        
        if (!success) continue;
        
        ctx = (ClientContext*)overlapped;
        
        if (bytesTransferred == 0) {
            // Client disconnected
            closesocket(ctx->client_fd);
            free(ctx);
            continue;
        }
        
        ctx->buffer_len = bytesTransferred;
        ctx->buffer[bytesTransferred] = '\0';
        
        parse_http_request(ctx);
        
        struct sockaddr_in addr;
        int addr_len = sizeof(addr);
        char client_ip[INET_ADDRSTRLEN];
        getpeername(ctx->client_fd, (struct sockaddr*)&addr, &addr_len);
        inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        Route *route = find_route(&global_config, ctx->request.method, ctx->request.path);
        
        if (route && route->is_proxy) {
            handle_proxy_request(ctx, route);
            log_access(&global_config, client_ip, ctx->request.method, ctx->request.path, 200);
        } else {
            handle_static_file(ctx, &global_config);
            log_access(&global_config, client_ip, ctx->request.method, ctx->request.path, 200);
        }
        
        closesocket(ctx->client_fd);
        free(ctx);
    }
    return 0;
}