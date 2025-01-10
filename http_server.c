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
const *response_404 =
	"HTTP/1.0 404 Not Found\r\n"
	"Content-Type: text/html\r\n"
	"\r\n"
	"<html><body><h1>Error 404: Not Found</h1></body></html>";

const char *response_200 =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "<html><body><h1>Hello bruh</h1></body></html>";

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
	int fd = open(path, O_RDONLY);

	if (fd < 0) {
		perror("File opening failed");
		send(client_fd, response_404, strlen(response_404), 0);
		return;
	}

	// Send HTTP header
	const char *header = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n";
	send(client_fd, header, strlen(header), 0);

	// Send file contents
	ssize_t bytes_read;
	while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
		send(client_fd, buffer, bytes_read, 0);
	}

	close(file_fd);
}

int parse_request(char *request, char *path, size_t path_len) {
    char method[10] = {0};
    int i;

    // Check if request is empty
    if (strlen(request) < 5) { // minimum "GET /"
        return 400;
    }

	// check REQUEST method
	if (strncmp(request, "GET ", 4) != 0) {
		return 501;
	}

	// extract PATH
	char *start = request + 4; // Skip the GET
	char *end = strchr(start, ' ');
	if (!end || (end - start) >= path_len) {
		return 400;
	}

	strncpy(path, start, end - start);
	path[end - start] = '\0'; // null-terminate the extracted PATH

	return 200;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(1);
    }

    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(1);
    }

    // config address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(1);
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(1);
    }

    printf("Listening on port %d\n", port);

    while(1) {
        // Accept connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept failed");
            continue;
        }

        // Read request
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_read < 0) {
            perror("read failed");
            close(client_fd);
            continue;
        }
        buffer[bytes_read] = '\0';

        // Parse request
        const char *response;
        switch (parse_request(buffer)) {
            case 200:
                response = response_200;
                break;
            case 501:
                response = response_501;
                break;
            default:
                response = response_400;
        }

        // Send response
        send(client_fd, response, strlen(response), 0);

        // Close client connection
        close(client_fd);
        printf("Closed connection\n");

        // clear buffer
        memset(buffer, 0, BUFFER_SIZE);
    }

    return 0;
}