#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define TCP_PORT "25928"
#define UDP_PORT "24928"
#define BACKLOG 10
#define MAX_BUFFER 1024

// Backend server ports as strings
#define SERVER_A_PORT "21928"
#define SERVER_R_PORT "22928"
#define SERVER_D_PORT "23928"

int tcp_socket, udp_socket;
struct addrinfo *tcp_servinfo, *udp_servinfo;
struct addrinfo *server_a_info, *server_r_info, *server_d_info;

// Taken from Beej’s Guide to Network Programming
// Get IP address (IPv4 or IPv6)
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Taken from Beej’s Guide to Network Programming
// Signal handler to prevent zombie processes
void sigchld_handler(int s) {
    (void)s; // Suppress unused variable warning
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

// Taken from Beej’s Guide to Network Programming
// Setup UDP socket
int setup_udp_socket() {
    struct addrinfo hints;
    int rv, sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // Use IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, UDP_PORT, &hints, &udp_servinfo)) != 0) {
        fprintf(stderr, "UDP getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Loop through results and bind to the first we can
    struct addrinfo *p;
    for (p = udp_servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("UDP socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("UDP bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "UDP: failed to bind socket\n");
        return -1;
    }

    return sockfd;
}

// Taken from Beej’s Guide to Network Programming
// Setup TCP socket
int setup_tcp_socket() {
    struct addrinfo hints;
    int rv, sockfd;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // Use IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, TCP_PORT, &hints, &tcp_servinfo)) != 0) {
        fprintf(stderr, "TCP getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Loop through results and bind to the first we can
    struct addrinfo *p;
    for (p = tcp_servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("TCP socket");
            continue;
        }

        // Allow address reuse
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("TCP setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("TCP bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "TCP: failed to bind socket\n");
        return -1;
    }

    // Start listening
    if (listen(sockfd, BACKLOG) == -1) {
        perror("TCP listen");
        exit(1);
    }

    return sockfd;
}

// Taken from Beej’s Guide to Network Programming
// Setup backend server addresses
int setup_backend_servers() {
    struct addrinfo hints;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // Use IPv4
    hints.ai_socktype = SOCK_DGRAM;

    // Resolve Server A
    if ((rv = getaddrinfo("127.0.0.1", SERVER_A_PORT, &hints, &server_a_info)) != 0) {
        fprintf(stderr, "Server A getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Resolve Server R
    if ((rv = getaddrinfo("127.0.0.1", SERVER_R_PORT, &hints, &server_r_info)) != 0) {
        fprintf(stderr, "Server R getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Resolve Server D
    if ((rv = getaddrinfo("127.0.0.1", SERVER_D_PORT, &hints, &server_d_info)) != 0) {
        fprintf(stderr, "Server D getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    return 0;
}

// Function to log operations
void log_operation(const char *member, const char *operation) {
    FILE *log_file = fopen("operation_log.txt", "a");
    if (log_file == NULL) {
        perror("Could not open operation log file");
        return;
    }
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_now);

    fprintf(log_file, "%s|%s|%s\n", member, timestamp, operation);
    fclose(log_file);
}

// Handle authentication request
void handle_auth_request(int client_socket, char *buffer) {
    char username[128], password[128];
    sscanf(buffer, "auth %s %s", username, password);
    printf("Server M has received username %s and password ****.\n", username);

    // Send request to Server A via UDP
    ssize_t sent_bytes = sendto(udp_socket, buffer, strlen(buffer), 0,
                                server_a_info->ai_addr, server_a_info->ai_addrlen);
    printf("Server M has sent authentication request to Server A\n");
    if (sent_bytes == -1) {
        perror("UDP sendto");
        send(client_socket, "Server error", 12, 0);
        return;
    }

    // Receive response from Server A
    char response[MAX_BUFFER] = {0};
    struct sockaddr_storage server_addr;
    socklen_t addr_len = sizeof(server_addr);
    ssize_t recv_bytes = recvfrom(udp_socket, response, MAX_BUFFER, 0,
                                  (struct sockaddr *)&server_addr, &addr_len);
    if (recv_bytes == -1) {
        perror("UDP recvfrom");
        send(client_socket, "Server error", 12, 0);
        return;
    }
    response[recv_bytes] = '\0';
    printf("The main server has received the response from server A using UDP over %s.\n", UDP_PORT);

    // Forward response to client
    send(client_socket, response, recv_bytes, 0);
    printf("The main server has sent the response from server A to client using TCP over port %s.\n", TCP_PORT);
}

// Handle lookup request
void handle_lookup_request(int client_socket, char *buffer) {
    char member[128], command[128], username[128];
    sscanf(buffer, "%s %s %s", member, command, username);

    printf("The main server received a %s request from %s to lookup %s's repository using TCP over port %s.\n", command, member, username, TCP_PORT);

    // Send request to Server R via UDP
    ssize_t sent_bytes = sendto(udp_socket, buffer, strlen(buffer), 0,
                                server_r_info->ai_addr, server_r_info->ai_addrlen);
    if (sent_bytes == -1) {
        perror("UDP sendto");
        send(client_socket, "Server error", 12, 0);
        close(client_socket);
        return;
    }
    printf("The main server sent a lookup request to server R.\n");

    // Receive response from Server R
    char response[MAX_BUFFER] = {0};
    struct sockaddr_storage server_addr;
    socklen_t addr_len = sizeof(server_addr);
    ssize_t recv_bytes = recvfrom(udp_socket, response, MAX_BUFFER, 0,
                                  (struct sockaddr *)&server_addr, &addr_len);
    if (recv_bytes == -1) {
        perror("UDP recvfrom");
        send(client_socket, "Server error", 12, 0);
        return;
    }
    response[recv_bytes] = '\0';
    printf("The main server received the response from server R using UDP over port %s.\n", UDP_PORT);

    // Forward response to client
    send(client_socket, response, recv_bytes, 0);
    printf("The main server has sent the response to the client.\n");

    // Log the operation
    if (strcmp(member, "guest") != 0) {
        strcat(command, " ");
        strcat(command, username);
        log_operation(member, command);
    }
}

// Handle push request
void handle_push_request(int client_socket, char *buffer) {
    char member[128], command[128], filename[128];
    sscanf(buffer, "%s %s %s", member, command, filename);

    printf("The main server received a push request from %s using TCP over port %s.\n", member, TCP_PORT);

    // Send request to Server R via UDP
    ssize_t sent_bytes = sendto(udp_socket, buffer, strlen(buffer), 0,
                                server_r_info->ai_addr, server_r_info->ai_addrlen);
    if (sent_bytes == -1) {
        perror("UDP sendto");
        send(client_socket, "Server error", 12, 0);
        close(client_socket);
        return;
    }
    printf("The main server has sent the push request to server R.\n");

    // Receive response from Server R
    char response[MAX_BUFFER] = {0};
    struct sockaddr_storage server_addr;
    socklen_t addr_len = sizeof(server_addr);
    ssize_t recv_bytes = recvfrom(udp_socket, response, MAX_BUFFER, 0,
                                  (struct sockaddr *)&server_addr, &addr_len);
    if (recv_bytes == -1) {
        perror("UDP recvfrom");
        send(client_socket, "Server error", 12, 0);
        close(client_socket);
        return;
    }

    response[recv_bytes] = '\0';

    if (strcmp(response, "duplicate file") == 0) {
        printf("The main server has received the response from server R using UDP over port %s, asking for overwrite confirmation\n", UDP_PORT);
        send(client_socket, "confirm_overwrite", 17, 0);
        printf("The main server has sent the overwrite confirmation request to the client.\n");

        // Receive overwrite confirmation from client
        char overwrite_response[10] = {0};
        int valread = recv(client_socket, overwrite_response, sizeof(overwrite_response), 0);
        if (valread <= 0) {
            return;
        }
        overwrite_response[valread] = '\0';
        printf("The main server has received the overwrite confirmation response from %s using TCP over port %s.\n", member, TCP_PORT);

        // Send overwrite confirmation to server R
        ssize_t sent_bytes = sendto(udp_socket, overwrite_response, strlen(overwrite_response), 0,
                                    server_r_info->ai_addr, server_r_info->ai_addrlen);
        if (sent_bytes == -1) {
            perror("UDP sendto");
            send(client_socket, "Server error", 12, 0);
            return;
        }
        printf("The main server has sent the overwrite confirmation response to server R.\n");

        // Receive final response from Server R
        memset(response, 0, sizeof(response));
        recv_bytes = recvfrom(udp_socket, response, MAX_BUFFER, 0,
                              (struct sockaddr *)&server_addr, &addr_len);
        if (recv_bytes == -1) {
            perror("UDP recvfrom");
            send(client_socket, "Server error", 12, 0);
            return;
        }
        response[recv_bytes] = '\0';
        printf("The main server has received the response from server R using UDP over port %s.\n", UDP_PORT);
    } else {
        printf("The main server received the response from server R using UDP over port %s.\n", UDP_PORT);
    }

    // Forward response to client
    send(client_socket, response, recv_bytes, 0);
    printf("The main server has sent the response to the client.\n");

    // Log the operation
    strcat(command, " ");
    strcat(command, filename);
    log_operation(member, command);
}

// Handle remove request
void handle_remove_request(int client_socket, char *buffer) {
    char member[128], command[128], filename[128];
    sscanf(buffer, "%s %s %s", member, command, filename);

    printf("The main server received a remove request from member %s TCP over port %s.\n", member, TCP_PORT);

    // Send request to Server R via UDP
    ssize_t sent_bytes = sendto(udp_socket, buffer, strlen(buffer), 0,
                                server_r_info->ai_addr, server_r_info->ai_addrlen);
    if (sent_bytes == -1) {
        perror("UDP sendto");
        send(client_socket, "Server error", 12, 0);
        close(client_socket);
        return;
    }

    // Receive response from Server R
    char response[MAX_BUFFER] = {0};
    struct sockaddr_storage server_addr;
    socklen_t addr_len = sizeof(server_addr);
    ssize_t recv_bytes = recvfrom(udp_socket, response, MAX_BUFFER, 0,
                                  (struct sockaddr *)&server_addr, &addr_len);
    if (recv_bytes == -1) {
        perror("UDP recvfrom");
        send(client_socket, "Server error", 12, 0);
        return;
    }
    response[recv_bytes] = '\0';
    printf("The main server has received confirmation of the remove request done by the server R.\n");

    // Forward response to client
    send(client_socket, response, recv_bytes, 0);

    // Log the operation
    strcat(command, " ");
    strcat(command, filename);
    log_operation(member, command);
}

// Handle deploy request
void handle_deploy_request(int client_socket, char *buffer) {
    char member[128], command[128];
    sscanf(buffer, "%s %s", member, command);

    printf("The main server has received a deploy request from member %s TCP over port %s.\n", member, TCP_PORT);

    // Send request to Server R via UDP to get the list of files
    printf("The main server has sent the lookup request to server R.\n");
    char lookup_request[MAX_BUFFER];
    sprintf(lookup_request, "%s deploy %s", member, member);
    ssize_t sent_bytes = sendto(udp_socket, lookup_request, strlen(lookup_request), 0,
                                server_r_info->ai_addr, server_r_info->ai_addrlen);
    if (sent_bytes == -1) {
        perror("UDP sendto");
        send(client_socket, "Server error", 12, 0);
        return;
    }

    // Receive response from Server R
    char files_response[MAX_BUFFER] = {0};
    struct sockaddr_storage server_addr;
    socklen_t addr_len = sizeof(server_addr);
    ssize_t recv_bytes = recvfrom(udp_socket, files_response, MAX_BUFFER, 0,
                                  (struct sockaddr *)&server_addr, &addr_len);
    if (recv_bytes == -1) {
        perror("UDP recvfrom");
        send(client_socket, "Server error", 12, 0);
        return;
    }
    files_response[recv_bytes] = '\0';
    printf("The main server received the lookup response from server R.\n");

    if (strcmp(files_response, "user not found") == 0 || strlen(files_response) == 0) {
        char error_message[MAX_BUFFER];
        sprintf(error_message, "no files found");
        send(client_socket, error_message, strlen(error_message), 0);
        return;
    }

    // Send deploy request to Server D via UDP
    printf("The main server sent the deploy request to server D.\n");
    char deploy_buffer[MAX_BUFFER];
    sprintf(deploy_buffer, "%s %s", member, files_response);
    sent_bytes = sendto(udp_socket, deploy_buffer, strlen(deploy_buffer), 0,
                        server_d_info->ai_addr, server_d_info->ai_addrlen);
    if (sent_bytes == -1) {
        perror("UDP sendto");
        send(client_socket, "Server error", 12, 0);
        close(client_socket);
        return;
    }

    // Receive response from Server D
    char response[MAX_BUFFER] = {0};
    recv_bytes = recvfrom(udp_socket, response, MAX_BUFFER, 0,
                          (struct sockaddr *)&server_addr, &addr_len);
    if (recv_bytes == -1) {
        perror("UDP recvfrom");
        send(client_socket, "Server error", 12, 0);
        return;
    }
    response[recv_bytes] = '\0';

    printf("The user %s's repository has been deployed at server D.\n", member);

    // Forward response to client
    send(client_socket, files_response, strlen(files_response), 0);

    // Log the operation
    log_operation(member, command);

}

// Handle log request
void handle_log_request(int client_socket, char *buffer) {
    char member[128], command[128];
    sscanf(buffer, "%s %s", member, command);

    printf("The main server received a log request from %s using TCP over port %s.\n", member, TCP_PORT);

    // Read the operation log file
    FILE *log_file = fopen("operation_log.txt", "r");
    if (log_file == NULL) {
        perror("Could not open operation log file");
        send(client_socket, "No operations have been logged.", 31, 0);
        return;
    }

    char line[256];
    char response[MAX_BUFFER * 10] = {0};  // Increased buffer size for logs
    int operation_count = 0;
    while (fgets(line, sizeof(line), log_file)) {
        char log_member[128], timestamp[64], operation[128];
        sscanf(line, "%[^|]|%[^|]|%[^\n]", log_member, timestamp, operation);
        if (strcmp(log_member, member) == 0) {
            strcat(response, operation);
            strcat(response, ";");
            operation_count++;
        }
    }
    fclose(log_file);

    if (operation_count == 0) {
        strcpy(response, "No operations have been logged.");
    }

    // Send the response to the client
    send(client_socket, response, strlen(response), 0);
    printf("The main server has sent the log result to the client.\n");
}

void handle_client_request(int client_socket) {
    char buffer[MAX_BUFFER] = {0};
    int valread;

    while (1) {
        memset(buffer, 0, sizeof(buffer));

        // Receive client request
        valread = recv(client_socket, buffer, MAX_BUFFER, 0);
        if (valread <= 0) {
            if (valread == 0) {
                // Connection closed by client
                printf("The main server noticed the client has closed the connection.\n");
            } else {
                perror("recv");
            }
            close(client_socket);
            _exit(0);
        }

        buffer[valread] = '\0';
        printf("DEGUB: Received from client: %s\n", buffer);

        // Determine backend server based on request
        if (strncmp(buffer, "auth", 4) == 0) {
            handle_auth_request(client_socket, buffer);
        } else if (strstr(buffer, " lookup ") != NULL) {
            handle_lookup_request(client_socket, buffer);
        } else if (strstr(buffer, " push ") != NULL) {
            handle_push_request(client_socket, buffer);
        } else if (strstr(buffer, " remove ") != NULL) {
            handle_remove_request(client_socket, buffer);
        } else if (strstr(buffer, " deploy") != NULL) {
            handle_deploy_request(client_socket, buffer);
        } else if (strstr(buffer, " log") != NULL) {
            handle_log_request(client_socket, buffer);
        } else {
            send(client_socket, "Invalid request", 15, 0);
        }
    }
}

int main(void) {
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    struct sigaction sa;

    // Setup UDP socket
    udp_socket = setup_udp_socket();
    if (udp_socket == -1) exit(1);

    // Setup TCP socket
    tcp_socket = setup_tcp_socket();
    if (tcp_socket == -1) exit(1);

    // Setup backend server addresses
    if (setup_backend_servers() == -1) exit(1);

    // Setup signal handling
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("Server M is up and running using UDP on port %s.\n", UDP_PORT);

    while(1) {
        // Accept TCP connection
        sin_size = sizeof client_addr;
        int new_fd = accept(tcp_socket, (struct sockaddr *)&client_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        // Fork to handle client
        pid_t pid = fork();
        if (pid == 0) { // Child process
            close(tcp_socket);  // Child doesn't need listener
            handle_client_request(new_fd);
            close(new_fd);
            _exit(0);
        }

        // Parent process
        close(new_fd);  // Parent doesn't need client socket
    }

    // Cleanup (these lines will never be reached in this implementation)
    freeaddrinfo(tcp_servinfo);
    freeaddrinfo(udp_servinfo);
    freeaddrinfo(server_a_info);
    freeaddrinfo(server_r_info);
    freeaddrinfo(server_d_info);

    close(tcp_socket);
    close(udp_socket);

    return 0;
}
