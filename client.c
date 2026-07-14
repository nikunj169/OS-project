#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    char input[BUFFER_SIZE];

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address\n");
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection failed\n");
        return -1;
    }

    printf("Connected to server\n");

    // Main communication loop
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = read(sock, buffer, BUFFER_SIZE - 1);

        if (bytes <= 0) {
            printf("Disconnected from server\n");
            break;
        }

        buffer[bytes] = '\0';
        printf("%s", buffer);

        // Check if server is waiting for input (ends with ": ")
        int len = strlen(buffer);
        if (len >= 2 && buffer[len-2] == ':' && buffer[len-1] == ' ') {
            // Get user input
            memset(input, 0, BUFFER_SIZE);
            if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
                break;
            }

            // Remove newline
            input[strcspn(input, "\n")] = '\0';

            // Send to server
            write(sock, input, strlen(input));
        }
    }

    close(sock);
    return 0;
}
