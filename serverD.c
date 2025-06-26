#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define UDPPORT "23928"
#define MAX_BUFFER 1024
#define DEPLOYED_FILE "deployed.txt"

int server_socket;
struct addrinfo *servinfo;

// Taken from Beej’s Guide to Network Programming
// Setup UDP socket
int setup_socket() {
    struct addrinfo hints;
    int rv, sockfd;

    // Zero out hints structure
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;     // Use IPv4
    hints.ai_socktype = SOCK_DGRAM;  // UDP socket
    hints.ai_flags = AI_PASSIVE;     // Use local IP

    // Get address info
    if ((rv = getaddrinfo(NULL, UDPPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "Server D getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Loop through results and bind to the first we can
    struct addrinfo *p;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        // Try to create socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("Server D socket");
            continue;
        }

        // Try to bind socket
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Server D bind");
            continue;
        }

        break;  // If we get here, we've succeeded in creating and binding the socket
    }

    // Check if we found a socket to bind
    if (p == NULL) {
        fprintf(stderr, "Server D failed to bind socket\n");
        return -1;
    }

    return sockfd;
}

int main() {
    struct sockaddr_storage client_addr;
    socklen_t addr_len;
    char buffer[MAX_BUFFER];

    // Setup socket
    server_socket = setup_socket();
    if (server_socket == -1) {
        exit(1);
    }
    freeaddrinfo(servinfo);
    printf("The Server D is up and running using UDP on port %s.\n", UDPPORT);

    while (1) {
        addr_len = sizeof(client_addr);
        int n = recvfrom(server_socket, buffer, MAX_BUFFER, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (n == -1){
            perror("recvfrom");
            continue;
        }

        buffer[n] = '\0';
        char member[128];
        char files[MAX_BUFFER];
        sscanf(buffer, "%s %[^\n]", member, files);

        printf("Server D has received a deploy request from the main server.\n");

        // Open or create the deployed.txt file
        FILE *file = fopen(DEPLOYED_FILE, "a");
        if (file == NULL) {
            perror("fopen");
            // Send error message back to main server
            char response[MAX_BUFFER] = "Deployment failed due to server error.";
            sendto(server_socket, response, strlen(response), 0, (struct sockaddr *)&client_addr, addr_len);
            continue;
        }

        // Check if file is empty and First line is the username and filename headers
        fseek(file, 0, SEEK_END);
        long fsize = ftell(file);
        if (fsize == 0) {
            fprintf(file, "UserName Files Deployed\n");
        }

        char *token = strtok(files, " ");
        while (token != NULL) {
            fprintf(file, "%s %s\n", member, token);
            token = strtok(NULL, " ");
        }

        fprintf(file, "----\n");
        fclose(file);

        // Prepare the response
        char response[MAX_BUFFER];
        sprintf(response, "files deployed");

        // Send the response back to the main server
        if (sendto(server_socket, response, strlen(response), 0, (struct sockaddr *)&client_addr, addr_len) == -1){
            perror("sendto");
            continue;
        }

        printf("Server D has deployed the user %s's repository.\n", member);
    }

    close(server_socket);

    return 0;
}