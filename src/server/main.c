#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

int main(void){
    // Create a socket
    int listener = socket(AF_INET, SOCK_STREAM, 0);

    // Socket address structure
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    // Bind socket to address 
    bind(listener, (struct sockaddr*)&addr, sizeof(addr));

    // Listen for incoming connections
    listen(listener, 5);
    
    // Accept a connection
    int client = accept(listener, NULL, NULL);
    if (client < 0) {
        perror("accept");
        close(listener);
        return 1;
    }

    // Receive the TCP byte stream
    char buf[8];
    ssize_t n;

    while ((n = recv(client, buf, sizeof(buf), 0)) > 0) {
        printf("recv %zd bytes: [", n);
        fwrite(buf, 1, (size_t)n, stdout);
        puts("]");
    }

    if (n < 0)
        perror("recv");
    else
        puts("Client closed the connection");

    // Release sockets
    close(client);
    close(listener);
    return n < 0 ? 1 : 0;
}
