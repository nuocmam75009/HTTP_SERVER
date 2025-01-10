#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 2048

// HTTP response templates
const char *response_404 =
    "HTTP/1.0 404 Not Found\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "<html><body><h1>Error 404: Not Found</h1></body></html>";

const char *response_400 =
    "HTTP/1.0 400 Bad Request\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "<html><body><h1>Error 400: Bad Request</h1></body></html>";

const char *response_501 =
    "HTTP/1.0 501 Not Implemented\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "<html><body><h1>Error 501: Not Implemented</h1></body></html>";

void send_file_response(int client_fd, const char *path) {
    char buffer[BUFFER_SIZE];
    int file_fd = open(path, O_RDONLY);

    if (file_fd < 0) {
        perror("File opening failed");
        send(client_fd, response_404, strlen(response_404), 0);
        return;
    }

    // Determine content type based on file extension
    const char *header = "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\n";
    if (strstr(path, ".html")) {
        header = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n";
    } else if (strstr(path, ".jpg") || strstr(path, ".jpeg")) {
        header = "HTTP/1.0 200 OK\r\nContent-Type: image/jpeg\r\n\r\n";
    } else if (strstr(path, ".png")) {
        header = "HTTP/1.0 200 OK\r\nContent-Type: image/png\r\n\r\n";
    }

    // Send HTTP header
    send(client_fd, header, strlen(header), 0);

    // Send file contents
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        send(client_fd, buffer, bytes_read, 0);
    }
    close(file_fd);
}

int parse_request(char *request, char *path, size_t path_len) {
    // Check if the request starts with "GET"
    if (strncmp(request, "GET ", 4) != 0) {
        return 501;
    }

    // Extract the path
    char *start = request + 4; // Skip "GET "
    char *end = strchr(start, ' '); // Find the space after the path
    if (!end || (end - start) >= path_len) {
        return 400;
    }

    strncpy(path, start, end - start); // Copy the path
    path[end - start] = '\0'; // Null-terminate the path

    // Ensure the path is not malformed
    if (strlen(path) == 0 || path[0] != '/') {
        return 400;
    }

    return 200; // Success
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <root_dir>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    const char *root_dir = argv[2];

    // Verify root directory
    struct stat dir_stat;
    if (stat(root_dir, &dir_stat) != 0 || !S_ISDIR(dir_stat.st_mode)) {
        fprintf(stderr, "Invalid root directory: %s\n", root_dir);
        exit(1);
    }

    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(1);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(1);
    }

    // Configure address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(1);
    }

    printf("Listening on port %d\n", port);

    while (1) {
        // Accept connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        // Read request
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_read < 0) {
            perror("Read failed");
            close(client_fd);
            continue;
        }
        buffer[bytes_read] = '\0';

        // Parse request
        char requested_path[BUFFER_SIZE];
        int status = parse_request(buffer, requested_path, sizeof(requested_path));

        if (status == 200) {
            char full_path[BUFFER_SIZE];
            snprintf(full_path, sizeof(full_path), "%s%s",
                     root_dir, strcmp(requested_path, "/") == 0 ? "/index.html" : requested_path);

            struct stat file_stat;
            if (stat(full_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
                send_file_response(client_fd, full_path);
            } else {
                send(client_fd, response_404, strlen(response_404), 0);
            }
        } else {
            const char *response = (status == 400) ? response_400 : response_501;
            send(client_fd, response, strlen(response), 0);
        }

        // Close client connection
        close(client_fd);
        printf("Closed connection\n");

        // Clear buffer
        memset(buffer, 0, BUFFER_SIZE);
    }

    return 0;
}
