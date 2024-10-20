#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int read_file(const char* file_path, char** file_content) {
    FILE* file = fopen(file_path, "r");
    
    if (file == NULL) return -1;

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    *file_content = malloc(file_size + 1);

    if (*file_content == NULL) {
        fclose(file);
        return -1;
    }

    size_t read_size = fread(*file_content, 1, file_size, file);
    if (read_size != file_size) {
        free(*file_content);
        fclose(file);
        return -1;
    }

    (*file_content)[file_size] = '\0';

    fclose(file);
    return 0;
}

int setup_server() {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd < 0) {
        perror("socket creation failed");
        return -1;
    };
    struct sockaddr_in ser_info;
    memset(&ser_info, 0, sizeof(ser_info));
    ser_info.sin_family = AF_INET; 
    ser_info.sin_port = htons(PORT);
    ser_info.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock_fd, (struct sockaddr *)&ser_info, sizeof(ser_info)) < 0) {
        perror("bind failed");
        close(sock_fd);
        return -1;
    }

    if (listen(sock_fd, 10) < 0) {
        perror("listen failed");
        close(sock_fd);
        return -1;
    }
    
    return sock_fd;
}

void send_html_response(int client_sock, const char *status, const char *file_path) {
    char *file_content = NULL;

    if (read_file(file_path, &file_content) == 0) {
        char response_header[BUFFER_SIZE];
        snprintf(response_header, sizeof(response_header),
                 "HTTP/1.1 %s\r\n"
                 "Content-Type: text/html\r\n"
                 "Connection: close\r\n"
                 "\r\n", status);
        send(client_sock, response_header, strlen(response_header), 0);

        send(client_sock, file_content, strlen(file_content), 0);

        free(file_content);
    } else {
        const char *response_500 = 
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>500 Internal Server Error</h1></body></html>";
        send(client_sock, response_500, strlen(response_500), 0);
    }
}

void send_static_response(int client_sock, const char *file_path) {
    char *file_content = NULL;
    if (read_file(file_path, &file_content) == 0) {
        char response_header[BUFFER_SIZE];
        const char *content_type = "text/html";

        if (strstr(file_path, ".css")) {
            content_type = "text/css";
        } else if (strstr(file_path, ".js")) {
            content_type = "application/javascript";
        } else if (strstr(file_path, ".png")) {
            content_type = "image/png";
        } else if (strstr(file_path, ".jpg") || strstr(file_path, ".jpeg")) {
            content_type = "image/jpeg";
        }

        snprintf(response_header, sizeof(response_header),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: %s\r\n"
                 "Connection: close\r\n"
                 "\r\n", content_type);

        send(client_sock, response_header, strlen(response_header), 0);
        send(client_sock, file_content, strlen(file_content), 0);
        free(file_content);
    } else {
        send_html_response(client_sock, "404 Not Found", "./404.html");
    }
}


void *handle_client(void *client_socket_ptr) {
    int client_sock = *(int *)client_socket_ptr;
    free(client_socket_ptr);
    char buffer[BUFFER_SIZE] = {0};
    char client_ip[INET_ADDRSTRLEN];

    struct sockaddr_in client_info;
    socklen_t client_len = sizeof(client_info);
    getpeername(client_sock, (struct sockaddr *)&client_info, &client_len);
    inet_ntop(AF_INET, &(client_info.sin_addr), client_ip, INET_ADDRSTRLEN);
    printf("Client connected: %s\n", client_ip);

    recv(client_sock, buffer, BUFFER_SIZE, 0);
    printf("Received request:\n%s\n", buffer);

    char method[8], path[1024];
    sscanf(buffer, "%s %s", method, path); 

    if (strcmp(method, "GET") == 0) {
        char file_path[1024] = ".";
        strcat(file_path, path);

        if (strcmp(path, "/") == 0) {
            send_html_response(client_sock, "200 OK", "./index.html");
        } else if (access(file_path, F_OK) != -1) {
            send_static_response(client_sock, file_path); 
        } else {
            send_html_response(client_sock, "404 Not Found", "./404.html");
        }
    } else if (strcmp(method, "POST") == 0) {
        char *content = strstr(buffer, "\r\n\r\n") + 4;
        printf("POST data: %s\n", content);

        const char *response_200 = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>POST data received</h1></body></html>";
        send(client_sock, response_200, strlen(response_200), 0);
    } else if (strcmp(method, "PUT") == 0) {
        const char *response_200 = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>Resource updated (PUT)</h1></body></html>";
        send(client_sock, response_200, strlen(response_200), 0);
    } else if (strcmp(method, "DELETE") == 0) {
        const char *response_200 = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>Resource deleted (DELETE)</h1></body></html>";
        send(client_sock, response_200, strlen(response_200), 0);
    } else {
        send_html_response(client_sock, "405 Method Not Allowed", "./405.html");
    }

    close(client_sock);
    return NULL;
}

int process(int server_sock) {
    if (server_sock < 0) {
        return 1; 
    }

    printf("HTTP server is running on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_info;
        socklen_t client_len = sizeof(client_info);
        int new_sock = accept(server_sock, (struct sockaddr *)&client_info, &client_len);
        if (new_sock < 0) {
            perror("accept failed");
            continue;
        }

        pthread_t thread_id;
        int *client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = new_sock;
        pthread_create(&thread_id, NULL, handle_client, client_sock_ptr);
        pthread_detach(thread_id);
    }

    close(server_sock);

    return 0;
}

int main() {

    int server_sock = setup_server();
    process(server_sock);

    return 0;
}