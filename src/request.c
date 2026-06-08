#include "server.h"

void parse_http_request(ClientContext *ctx) {
    HTTPRequest *req = &ctx->request;
    memset(req, 0, sizeof(HTTPRequest));
    
    char *line = strtok(ctx->buffer, "\r\n");
    if (!line) return;
    
    // Parse request line
    sscanf(line, "%15s %4095s %15s", req->method, req->path, req->version);
    
    // Parse headers
    req->header_count = 0;
    while ((line = strtok(NULL, "\r\n")) != NULL && strlen(line) > 0) {
        if (req->header_count < MAX_HEADERS) {
            strncpy(req->headers[req->header_count], line, 255);
            req->header_count++;
        }
    }
    
    // Parse body if present
    char *body_start = strstr(ctx->buffer, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        ctx->request.body_length = ctx->buffer_len - (body_start - ctx->buffer);
        if (ctx->request.body_length < BUFFER_SIZE) {
            memcpy(ctx->request.body, body_start, ctx->request.body_length);
        }
    }
}