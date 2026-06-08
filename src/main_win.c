#include "server_win.h"

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    
    InitializeCriticalSection(&log_cs);
    
    init_server_config(&global_config);
    load_config(&global_config, "config\\server.conf");
    
    // Create directories
    CreateDirectoryA("logs", NULL);
    CreateDirectoryA("www", NULL);
    
    SOCKET server_fd = create_server_socket(global_config.port);
    if (server_fd == INVALID_SOCKET) {
        log_error(&global_config, "Failed to create server socket on port %d", global_config.port);
        WSACleanup();
        return 1;
    }
    
    printf("========================================\n");
    printf("Custom HTTP Server (Windows) Running\n");
    printf("========================================\n");
    printf("Port: %d\n", global_config.port);
    printf("Root Directory: %s\n", global_config.root_dir);
    printf("Logging: %s\n", global_config.enable_logging ? "Enabled" : "Disabled");
    printf("========================================\n");
    printf("Visit: http://localhost:%d\n", global_config.port);
    printf("Press Ctrl+C to stop\n\n");
    
    completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    
    CreateIoCompletionPort((HANDLE)server_fd, completion_port, 0, 0);
    
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    int thread_count = min(sysInfo.dwNumberOfProcessors * 2, MAX_WORKER_THREADS);
    
    for (int i = 0; i < thread_count; i++) {
        HANDLE thread = CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
        CloseHandle(thread);
    }
    
    printf("Worker threads: %d\n\n", thread_count);
    
    // Main accept loop
    while (TRUE) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd != INVALID_SOCKET) {
            ClientContext *ctx = (ClientContext*)malloc(sizeof(ClientContext));
            memset(ctx, 0, sizeof(ClientContext));
            ctx->client_fd = client_fd;
            
            ctx->wsaBuf.buf = ctx->buffer;
            ctx->wsaBuf.len = BUFFER_SIZE;
            ctx->flags = 0;
            memset(&ctx->overlapped, 0, sizeof(OVERLAPPED));
            
            DWORD bytes;
            DWORD flags = 0;
            WSARecv(client_fd, &ctx->wsaBuf, 1, &bytes, &flags, &ctx->overlapped, NULL);
            
            CreateIoCompletionPort((HANDLE)client_fd, completion_port, 0, 0);
        }
    }
    
    closesocket(server_fd);
    WSACleanup();
    DeleteCriticalSection(&log_cs);
    
    return 0;
}