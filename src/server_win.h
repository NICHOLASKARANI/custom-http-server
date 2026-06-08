#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_EVENTS 1024
#define BUFFER_SIZE 8192
#define MAX_PATH 4096
#define MAX_HEADERS 64
#define BACKLOG 512
#define WORKER_THREADS 4

typedef struct {
    char method[16];
    char path[MAX_PATH];
    char version[16];
    char headers[MAX_HEADERS][256];
    int header_count;
    char body[BUFFER_SIZE];
    size_t body_length;
} HTTPRequest;

typedef struct {
    int status_code;
    char status_text[64];
    char headers[MAX_HEADERS][256];
    int header_count;
    char body[BUFFER_SIZE];
    size_t body_length;
} HTTPResponse;

typedef struct {
    char method[16];
    char path[MAX_PATH];
    char target[MAX_PATH];
    int is_proxy;
    char proxy_host[256];
    int proxy_port;
} Route;

typedef struct {
    Route routes[256];
    int route_count;
    char root_dir[MAX_PATH];
    int port;
    int enable_logging;
    char access_log[MAX_PATH];
    char error_log[MAX_PATH];
} ServerConfig;

typedef struct {
    int client_fd;
    HTTPRequest request;
    char buffer[BUFFER_SIZE];
    size_t buffer_len;
} ClientContext;

// Function declarations
void init_server_config(ServerConfig *config);
int load_config(ServerConfig *config, const char *filename);
int create_server_socket(int port);
void setup_epoll(int server_fd, int *epoll_fd);
void handle_client(int client_fd, ServerConfig *config, int epoll_fd);
void parse_http_request(ClientContext *ctx);
void handle_static_file(ClientContext *ctx, ServerConfig *config);
void handle_proxy_request(ClientContext *ctx, Route *route);
void log_access(ServerConfig *config, const char *client_ip, const char *method, const char *path, int status);
void log_error(ServerConfig *config, const char *format, ...);
void send_response(int client_fd, HTTPResponse *response);
void set_response(HTTPResponse *response, int status, const char *status_text);
void add_response_header(HTTPResponse *response, const char *key, const char *value);
void send_file_response(int client_fd, const char *filepath, const char *mime_type);
char* get_mime_type(const char *path);

#endif