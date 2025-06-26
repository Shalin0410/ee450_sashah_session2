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

#define PORT "21928"
#define MAX_BUFFER 1024

int server_socket;
struct addrinfo *servinfo;

// Taken from Beej’s Guide to Network Programming
// Get IP address (IPv4 or IPv6)
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Authenticate user by comparing encrypted passwords
int authenticate_user(char *username, char *encrypted_password) {
    FILE *fp = fopen("members.txt", "r");
    if (!fp) {
        perror("Cannot open members.txt");
        return 0;
    }
    char line[256];
    char stored_username[128];
    char stored_encrypted_password[128];

    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "%s %s", stored_username, stored_encrypted_password);
        if (strcmp(username, stored_username) == 0 &&
            strcmp(encrypted_password, stored_encrypted_password) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

// Taken from Beej’s Guide to Network Programming
// Setup UDP socket using getaddrinfo
int setup_socket() {
    struct addrinfo hints;
    int rv, sockfd;

    // Zero out hints structure
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;     // Use IPv4
    hints.ai_socktype = SOCK_DGRAM;  // UDP socket
    hints.ai_flags = AI_PASSIVE;     // Use local IP

    // Get address info
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "Server A getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Loop through results and bind to the first we can
    struct addrinfo *p;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        // Try to create socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("Server A socket");
            continue;
        }

        // Try to bind socket
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Server A bind");
            continue;
        }

        break;  // If we get here, we've succeeded in creating and binding the socket
    }

    // Check if we found a socket to bind
    if (p == NULL) {
        fprintf(stderr, "Server A failed to bind socket\n");
        return -1;
    }

    return sockfd;
}

int main() {
    struct sockaddr_storage client_addr;
    socklen_t addr_len;
    char buffer[MAX_BUFFER];
    char msg_type[10], username[128], encrypted_password[128];

    // Setup socket
    server_socket = setup_socket();
    if (server_socket == -1) {
        exit(1);
    }
    freeaddrinfo(servinfo);
    printf("The Server A is up and running using UDP on port %s.\n", PORT);

    while (1) {
        // Receive data
        addr_len = sizeof(client_addr);
        int n = recvfrom(server_socket, buffer, MAX_BUFFER-1, 0,
                             (struct sockaddr *)&client_addr, &addr_len);

        if (n == -1) {
            perror("recvfrom");
            continue;
        }

        // Null-terminate the buffer
        buffer[n] = '\0';

        // Parse username and encrypted password
        sscanf(buffer, "%s %s %s", msg_type, username, encrypted_password);

        printf("ServerA received username %s and password ******\n", username);

        char response[MAX_BUFFER];
        if (strcmp(msg_type, "auth") == 0 && authenticate_user(username, encrypted_password)) {
            printf("Member %s has been authenticated\n", username);
            strcpy(response, "auth_success");
        } else {
            printf("The username %s or password ****** is incorrect\n", username);
            strcpy(response, "auth_failed");
        }

        // Send response back to main server
        if (sendto(server_socket, response, strlen(response), 0,
                   (struct sockaddr *)&client_addr, addr_len) == -1) {
            perror("sendto");
        }
    }

    close(server_socket);

    return 0;
}
