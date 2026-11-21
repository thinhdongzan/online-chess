#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
    // Create socket
    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to server
    connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // Read input and send it
    char line[128];

    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        size_t sent = 0;

        while (sent < len) {
            ssize_t n = send(client_socket, line + sent,
                             len - sent, 0);

            if (n < 0 && errno == EINTR)
                continue;

            if (n <= 0) {
                perror("send");
                close(client_socket);
                return 1;
            }

            sent += (size_t)n;
        }
    }

    // 5. Finish sending and close
    shutdown(client_socket, SHUT_WR);
    close(client_socket);
    return 0;
    
}
