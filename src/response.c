#include "server.h"

void set_response(HTTPResponse *response, int status, const char *status_text) {
    response->status_code = status;
    strcpy(response->status_text, status_text);
    response->header_count = 0;
    response->body_length = 0;
    
    // Add default headers
    add_response_header(response, "Server", "CustomHTTP/1.1");
    add_response_header(response, "Connection", "close");
    add_response_header(response, "Date", "Wed, 01 Jan 2025 00:00:00 GMT");
}

void add_response_header(HTTPResponse *response, const char *key, const char *value) {
    if (response->header_count < MAX_HEADERS) {
        snprintf(response->headers[response->header_count], 255, "%s: %s", key, value);
        response->header_count++;
    }
}

void send_response(int client_fd, HTTPResponse *response) {
    char buffer[BUFFER_SIZE];
    int len = snprintf(buffer, BUFFER_SIZE, "HTTP/1.1 %d %s\r\n", 
                      response->status_code, response->status_text);
    
    for (int i = 0; i < response->header_count; i++) {
        len += snprintf(buffer + len, BUFFER_SIZE - len, "%s\r\n", response->headers[i]);
    }
    
    len += snprintf(buffer + len, BUFFER_SIZE - len, "\r\n");
    
    // Send headers
    send(client_fd, buffer, len, 0);
    
    // Send body
    if (response->body_length > 0) {
        send(client_fd, response->body, response->body_length, 0);
    }
}

void send_file_response(int client_fd, const char *filepath, const char *mime_type) {
    struct stat st;
    if (stat(filepath, &st) != 0) {
        HTTPResponse response;
        set_response(&response, 404, "Not Found");
        add_response_header(&response, "Content-Type", "text/html");
        snprintf(response.body, BUFFER_SIZE, "<h1>404 Not Found</h1><p>File not found</p>");
        response.body_length = strlen(response.body);
        add_response_header(&response, "Content-Length", "70");
        send_response(client_fd, &response);
        return;
    }
    
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) return;
    
    char header_buffer[512];
    int header_len = snprintf(header_buffer, 512, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n", mime_type, st.st_size);
    
    send(client_fd, header_buffer, header_len, 0);
    
    // Send file in chunks
    char file_buffer[8192];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, file_buffer, sizeof(file_buffer))) > 0) {
        send(client_fd, file_buffer, bytes_read, 0);
    }
    
    close(file_fd);
}