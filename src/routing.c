#include "server.h"

Route* find_route(ServerConfig *config, const char *method, const char *path) {
    for (int i = 0; i < config->route_count; i++) {
        Route *r = &config->routes[i];
        if (strcmp(r->method, method) == 0) {
            // Check exact match or prefix match for /api/*
            if (strcmp(r->path, path) == 0) return r;
            
            // Check prefix for reverse proxy
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
    
    // Security: prevent directory traversal
    if (strstr(full_path, "..")) {
        HTTPResponse response;
        set_response(&response, 403, "Forbidden");
        send_response(ctx->client_fd, &response);
        return;
    }
    
    // Default to index.html
    if (full_path[strlen(full_path) - 1] == '/') {
        strcat(full_path, "index.html");
    }
    
    char *mime = get_mime_type(full_path);
    send_file_response(ctx->client_fd, full_path, mime);
}