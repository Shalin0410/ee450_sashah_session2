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
#include <stdbool.h>

#define PORT "22928"
#define MAX_BUFFER 1024
#define FILENAME "filenames.txt"

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

// Taken from Beej’s Guide to Network Programming
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
        fprintf(stderr, "Server R getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Loop through results and bind to the first we can
    struct addrinfo *p;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        // Try to create socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("Server R socket");
            continue;
        }

        // Try to bind socket
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Server R bind");
            continue;
        }

        break;
    }

    // Check if we found a socket to bind
    if (p == NULL) {
        fprintf(stderr, "Server R failed to bind socket\n");
        return -1;
    }

    return sockfd;
}

void handle_lookup_request(char *member, char *username, struct sockaddr *client_addr, socklen_t addr_len, int server_socket) {
    FILE *file = fopen(FILENAME, "r");
    if (file == NULL) {
        perror("fopen");
        char response[MAX_BUFFER] = "user not found";
        sendto(server_socket, response, strlen(response), 0, client_addr, addr_len);
        return;
    }

    char line[MAX_BUFFER];
    char response[MAX_BUFFER] = {0};
    bool user_found = false;

    while (fgets(line, sizeof(line), file)) {
        char file_user[128], filename[128];
        sscanf(line, "%s %s", file_user, filename);
        if (strcmp(file_user, username) == 0) {
            strcat(response, filename);
            strcat(response, " ");
            user_found = true;
        }
    }

    fclose(file);

    if (!user_found) {
        strcpy(response, "user not found");
    }

    if (response[strlen(response)-1] == ' ') {
        response[strlen(response)-1] = '\0';
    }

    sendto(server_socket, response, strlen(response), 0, client_addr, addr_len);
    printf("Server R has finished sending the response to the main server.\n");
}

void handle_push_request(char *member, char *filename, struct sockaddr *client_addr, socklen_t addr_len, int server_socket) {
    FILE *file = fopen(FILENAME, "r");
    if (file == NULL) {
        perror("fopen");
        return;
    }

    bool file_exists = false;
    char line[MAX_BUFFER];
    while (fgets(line, sizeof(line), file)) {
        char file_user[128], file_name[128];
        sscanf(line, "%s %s", file_user, file_name);
        if (strcmp(file_user, member) == 0 && strcmp(file_name, filename) == 0) {
            file_exists = true;
            break;
        }
    }
    fclose(file);

    if (file_exists) {
        printf("%s exists in %s's repository; requesting overwrite confirmation.\n", filename, member);
        char response[MAX_BUFFER] = "duplicate file";
        ssize_t bytes_sent = sendto(server_socket, response, strlen(response), 0, client_addr, addr_len);
        if (bytes_sent == -1) {
            perror("sendto");
            return;
        }

        // Receive overwrite confirmation
        char confirmation_buffer[2];
        ssize_t recv_bytes = recvfrom(server_socket, confirmation_buffer, sizeof(confirmation_buffer), 0, client_addr, &addr_len);
        if (recv_bytes == -1) {
            perror("recvfrom");
            return;
        }

        if (confirmation_buffer[0] == 'Y' || confirmation_buffer[0] == 'y') {
            // Overwrite the file entry
            printf("User requested overwrite; overwrite successful.\n");
            FILE *file = fopen(FILENAME, "r");
            FILE *temp = fopen("temp.txt", "w");
            if (!file || !temp) {
                perror("File open");
                return;
            }
            while (fgets(line, sizeof(line), file)) {
                char file_user[128], file_name[128];
                sscanf(line, "%s %s", file_user, file_name);
                if (!(strcmp(file_user, member) == 0 && strcmp(file_name, filename) == 0)) {
                    fprintf(temp, "%s %s\n", file_user, file_name);
                }
            }
            fprintf(temp, "%s %s\n", member, filename); // Add updated entry
            fclose(file);
            fclose(temp);
            remove(FILENAME);
            rename("temp.txt", FILENAME);

            strcpy(response, "file overwritten");
        } else {
            printf("Overwrite denied\n");
            strcpy(response, "overwrite denied");
        }
        sendto(server_socket, response, strlen(response), 0, client_addr, addr_len);
    } else {
        // Add new file entry
        FILE *file = fopen(FILENAME, "a");
        if (file == NULL) {
            perror("fopen");
            return;
        }
        fprintf(file, "%s %s\n", member, filename);
        fclose(file);
        printf("%s uploaded successfully.\n", filename);

        char response[MAX_BUFFER] = "file pushed";
        sendto(server_socket, response, strlen(response), 0, client_addr, addr_len);
    }
}

void handle_remove_request(char *member, char *filename, struct sockaddr *client_addr, socklen_t addr_len, int server_socket) {
    FILE *file = fopen(FILENAME, "r");
    if (file == NULL) {
        perror("fopen");
        return;
    }

    FILE *temp = fopen("temp.txt", "w");
    if (temp == NULL) {
        perror("fopen");
        fclose(file);
        return;
    }

    char line[MAX_BUFFER];
    bool found = false;

    while (fgets(line, sizeof(line), file)) {
        char file_user[128], file_name[128];
        sscanf(line, "%s %s", file_user, file_name);
        if (strcmp(file_user, member) == 0 && strcmp(file_name, filename) == 0) {
            found = true;
            continue; // Skip writing this line to temp file
        }
        fprintf(temp, "%s %s\n", file_user, file_name);
    }

    fclose(file);
    fclose(temp);

    if (found) {
        remove(FILENAME);
        rename("temp.txt", FILENAME);
        //printf("%s's file %s has been removed from the repository.\n", member, filename);
        char response[MAX_BUFFER] = "file removed";
        sendto(server_socket, response, strlen(response), 0, client_addr, addr_len);
    } else {
        remove("temp.txt"); // Cleanup temp file
        char response[MAX_BUFFER] = "file not found";
        sendto(server_socket, response, strlen(response), 0, client_addr, addr_len);
    }
}

void handle_deploy_request(char *member, struct sockaddr *client_addr, socklen_t addr_len, int server_socket) {
    FILE *file = fopen(FILENAME, "r");
    if (file == NULL) {
        perror("fopen");
        char response[MAX_BUFFER] = "user not found";
        sendto(server_socket, response, strlen(response), 0, client_addr, addr_len);
        return;
    }

    char line[MAX_BUFFER];
    char response[MAX_BUFFER] = {0};
    bool user_found = false;

    while (fgets(line, sizeof(line), file)) {
        char file_user[128], filename[128];
        sscanf(line, "%s %s", file_user, filename);
        if (strcmp(file_user, member) == 0) {
            strcat(response, filename);
            strcat(response, " ");
            user_found = true;
        }
    }

    fclose(file);

    if (!user_found) {
        strcpy(response, "user not found");
    }

    if (response[strlen(response)-1] == ' ') {
        response[strlen(response)-1] = '\0';
    }

    sendto(server_socket, response, strlen(response), 0, client_addr, addr_len);
    printf("Server R has finished sending the response to the main server.\n");
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
    printf("The Server R is up and running using UDP on port %s.\n", PORT);

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
        char member[128], command[128], argument[128];
        sscanf(buffer, "%s %s %s", member, command, argument);

        // Parse command and handle accordingly
        if (strcmp(command, "lookup") == 0) {
            printf("Server R has received a lookup request from the main server.\n");
            handle_lookup_request(member, argument, (struct sockaddr *)&client_addr, addr_len, server_socket);
        }
        else if (strcmp(command, "push") == 0) {
            printf("Server R has received a push request from the main server.\n");
            handle_push_request(member, argument, (struct sockaddr *)&client_addr, addr_len, server_socket);
        }
        else if (strcmp(command, "remove") == 0) {
            printf("Server R has received a remove request from the main server.\n");
            handle_remove_request(member, argument, (struct sockaddr *)&client_addr, addr_len, server_socket);
        }
        else if (strcmp(command, "deploy") == 0) {
            printf("Server R has received a deploy request from the main server.\n");
            handle_deploy_request(member, (struct sockaddr *)&client_addr, addr_len, server_socket);
        }
        else {
            char response[MAX_BUFFER] = "invalid request";
            sendto(server_socket, response, strlen(response), 0, (struct sockaddr *)&client_addr, addr_len);
        }
    }

    close(server_socket);   

    return 0;
}
