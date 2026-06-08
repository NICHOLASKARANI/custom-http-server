#include "server.h"
#include <pthread.h>

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_server_config(ServerConfig *config) {
    config->route_count = 0;
    strcpy(config->root_dir, "./www");
    config->port = 8080;
    config->enable_logging = 1;
    strcpy(config->access_log, "logs/access.log");
    strcpy(config->error_log, "logs/error.log");
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
        
        // Parse route: route /api http://backend:3000
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

int create_server_socket(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd < 0) return -1;
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, BACKLOG) < 0) {
        close(server_fd);
        return -1;
    }
    
    return server_fd;
}

void setup_epoll(int server_fd, int *epoll_fd) {
    *epoll_fd = epoll_create1(0);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
}

void log_access(ServerConfig *config, const char *client_ip, const char *method, const char *path, int status) {
    if (!config->enable_logging) return;
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%d/%b/%Y:%H:%M:%S %z", tm_info);
    
    pthread_mutex_lock(&log_mutex);
    FILE *log = fopen(config->access_log, "a");
    if (log) {
        fprintf(log, "%s - - [%s] \"%s %s HTTP/1.1\" %d\n", client_ip, timestamp, method, path, status);
        fclose(log);
    }
    pthread_mutex_unlock(&log_mutex);
}

void log_error(ServerConfig *config, const char *format, ...) {
    va_list args;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    pthread_mutex_lock(&log_mutex);
    FILE *log = fopen(config->error_log, "a");
    if (log) {
        fprintf(log, "[%s] ", timestamp);
        va_start(args, format);
        vfprintf(log, format, args);
        va_end(args);
        fprintf(log, "\n");
        fclose(log);
    }
    pthread_mutex_unlock(&log_mutex);
}

char* get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    
    return "application/octet-stream";
}