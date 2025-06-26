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

#define SERVERM_PORT "25928"
#define MAXBUFLEN 1024
#define LOCALHOST "127.0.0.1"

int sockfd;
struct addrinfo hints;
struct addrinfo *servinfo;
struct addrinfo *p;
int numbytes;

// Taken from Beej’s Guide to Network Programming
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Taken from Beej’s Guide to Network Programming
int setup_tcp_socket(){
    int rv;
    //char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Client will be locally hosted on 127.0.0.1 and will connect to Server M
    if ((rv = getaddrinfo(LOCALHOST, SERVERM_PORT, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("client: socket");
            continue;
        }


        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL){
        fprintf(stderr, "client: failed to connect\n");
        exit(1);
    }

    // inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    // printf("The client is up and running.\n");

    freeaddrinfo(servinfo);

    return sockfd;
}

char* encrypt(char *password) {
    static char encrypted_password[1024] = {0};
    memset(encrypted_password, 0, sizeof(encrypted_password));

    for (int i = 0; i < strlen(password); i++) {
        if (password[i] >= 'a' && password[i] <= 'z') {
            encrypted_password[i] = ((password[i] - 'a' + 3) % 26) + 'a';
        } else if (password[i] >= 'A' && password[i] <= 'Z') {
            encrypted_password[i] = ((password[i] - 'A' + 3) % 26) + 'A';
        } else if (password[i] >= '0' && password[i] <= '9') {
            encrypted_password[i] = ((password[i] - '0' + 3) % 10) + '0';
        } else {
            encrypted_password[i] = password[i];
        }
    }
    return encrypted_password;
}

void authenticate(char *username, char *password) {
    char *encrypted_password = encrypt(password);
    char message[MAXBUFLEN] = {0};

    sprintf(message, "auth %s %s", username, encrypted_password);

    // Send authentication message to Server M
    send(sockfd, message, strlen(message), 0);

    char response[MAXBUFLEN] = {0};
    if ((numbytes = recv(sockfd, response, MAXBUFLEN-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    response[numbytes] = '\0';

    if (strcmp(response, "auth_failed") == 0) {
        printf("The credentials are incorrect. Please try again.\n");
        exit(1);
    }
}

void handle_command(char *input_command, char *username, bool guest) {
    char message[MAXBUFLEN] = {0};
    char command[1024];
    strcpy(command, input_command);  // Copy input_command because strtok modifies the string

    // Parse the command
    char *token = strtok(command, " ");
    char *command_type = token;
    char *argument = strtok(NULL, " ");

    // Get the client port number
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    getsockname(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
    int client_port = ntohs(client_addr.sin_port);

    // Check if user is guest
    if (guest){
        // Check if the command is valid
        if (strcmp(command_type, "lookup") == 0) {
            if (argument == NULL) {
                printf("Error: Username is required. Please specify a username to lookup.\n");
                return;
            }
            printf("Guest sent a lookup request to the main server.\n");
            // Prepare the message to send
            sprintf(message, "guest lookup %s", argument);
            // Send the message to the server
            send(sockfd, message, strlen(message), 0);  // Send the message to the server

            // Receive the response from the server
            char response[MAXBUFLEN] = {0};
            if ((numbytes = recv(sockfd, response, MAXBUFLEN-1, 0)) == -1) {
                perror("recv");
                exit(1);
            }
            response[numbytes] = '\0';
            if (strcmp(response, "user not found") == 0) {
                printf("The client received the response from the main server using TCP over port %d.\n", client_port);
                printf("%s does not exist. Please try again.\n", argument);
            } else {
                printf("The client received the response from the main server using TCP over port %d.\n", client_port);
                char *response_token = strtok(response, " ");
                //Until the end of the response
                if (response_token != NULL || response_token != "") {
                    while (response_token != NULL) {
                        printf("%s\n", response_token);
                        response_token = strtok(NULL, " ");
                    }
                } else {
                    printf("Empty repository.\n");
                }
            }
        } else {
            printf("Guests can only use the lookup command\n");
            return;
        }
    } else {
        // For members
        if (strcmp(command_type, "lookup") == 0) {
            if (argument == NULL) {
                printf("Username is not specified. Will lookup %s.\n", username);
                argument = username;
            }
            // Prepare the message to send
            sprintf(message, "%s lookup %s", username, argument);
            // Send the message to the server
            int bytes_sent = send(sockfd, message, strlen(message), 0);
            if (bytes_sent == -1) {
                perror("send");
                exit(1);
            }
            printf("%s sent a lookup request to the main server.\n", username);

            // Receive the response from the server
            char response[MAXBUFLEN] = {0};
            if ((numbytes = recv(sockfd, response, MAXBUFLEN-1, 0)) == -1) {
                perror("recv");
                exit(1);
            }
            response[numbytes] = '\0';
            if (strcmp(response, "user not found") == 0) {
                printf("The client received the response from the main server using TCP over port %d.\n", client_port);
                printf("%s does not exist. Please try again.\n", argument);
            } else {
                printf("The client received the response from the main server using TCP over port %d.\n", client_port);
                char *response_token = strtok(response, " ");
                //Until the end of the response and if response_token is not NULL
                if (response_token != NULL || response_token != "") {
                    while (response_token != NULL) {
                        printf("%s\n", response_token);
                        response_token = strtok(NULL, " ");
                    }
                } else {
                    printf("Empty repository.\n");
                }
            }
        } else if (strcmp(command_type, "push") == 0) {
            if (argument == NULL) {
                printf("Error: Filename is required. Please specify a filename to push.\n");
                return;
            }
            // Check current directory for the file
            FILE *file = fopen(argument, "r");
            if (file == NULL) {
                printf("Error: Invalid file: %s\n", argument);
                return;
            }
            fclose(file);
            //printf("%s sent a push request to the main server.\n", username);
            // Prepare the message to send
            sprintf(message, "%s push %s", username, argument);
            // Send the message to the server
            if (send(sockfd, message, strlen(message), 0) == -1) {
                perror("send");
                return;
            }

            // Receive the response from the server
            char response[MAXBUFLEN] = {0};
            if ((numbytes = recv(sockfd, response, MAXBUFLEN-1, 0)) == -1) {
                perror("recv");
                exit(1);
            }
            response[numbytes] = '\0';

            if (strcmp(response, "confirm_overwrite") == 0) {
                printf("%s exists in %s's repository, do you want to overwrite (Y/N)?\n", argument, username);
                char overwrite_choice[8] = {0};
                if (fgets(overwrite_choice, sizeof(overwrite_choice), stdin) == NULL) {
                    printf("Error reading input\n");
                    return;
                }
                overwrite_choice[strcspn(overwrite_choice, "\n")] = '\0';
                // Check if input format is correct
                while (strcasecmp(overwrite_choice, "Y") != 0 && strcasecmp(overwrite_choice, "N") != 0) {
                    printf("Invalid input. Please enter Y or N: ");
                    if (fgets(overwrite_choice, sizeof(overwrite_choice), stdin) == NULL) {
                        printf("Error reading input\n");
                        return;
                    }
                    overwrite_choice[strcspn(overwrite_choice, "\n")] = '\0';
                }
                // Send the overwrite confirmation to server
                if (send(sockfd, overwrite_choice, strlen(overwrite_choice), 0) == -1) {
                    perror("send");
                    return;
                }
                // Receive the final response
                memset(response, 0, sizeof(response));
                if ((numbytes = recv(sockfd, response, MAXBUFLEN-1, 0)) == -1) {
                    perror("recv");
                    return;
                }
                response[numbytes] = '\0';
                if (strcmp(response, "file overwritten") == 0) {
                    printf("%s pushed successfully\n", argument);
                } else {
                    printf("%s was not pushed successfully.\n", argument);
                }
            } else if (strcmp(response, "file pushed") == 0 || strcmp(response, "file overwritten") == 0) {
                printf("%s pushed successfully\n", argument);
            } else {
                printf("%s was not pushed successfully.\n", argument);
            }
            return;

        } else if (strcmp(command_type, "remove") == 0) {
            if (argument == NULL) {
                printf("Error: Filename is required. Please specify a filename to remove.\n");
                return;
            }
            // Prepare the message to send
            sprintf(message, "%s remove %s", username, argument);
            // Send the message to the server
            send(sockfd, message, strlen(message), 0);
            printf("%s sent a remove request to the main server.\n", username);

            // Receive the response from the server
            char response[MAXBUFLEN] = {0};
            if ((numbytes = recv(sockfd, response, MAXBUFLEN-1, 0)) == -1) {
                perror("recv");
                exit(1);
            }
            response[numbytes] = '\0';
            if (strcmp(response, "file removed") == 0) {
                printf("The remove request was successful.\n");
            } else {
                printf("File not found\n");
            }

        } else if (strcmp(command_type, "deploy") == 0) {
            // Prepare the message to send
            sprintf(message, "%s deploy", username);
            // Send the message to the server
            send(sockfd, message, strlen(message), 0);
            printf("%s sent a lookup request to the main server.\n", username);

            // Receive the response from the server
            char response[MAXBUFLEN] = {0};
            if ((numbytes = recv(sockfd, response, MAXBUFLEN, 0)) == -1) {
                perror("recv");
                exit(1);
            }
            response[numbytes] = '\0';
            if (strcmp(response, "no files found") == 0) {
                printf("The client received the response from the main server using TCP over port %d, and no files were found for deployment.\n", client_port);
            } else {
                //if (strcmp(response, "files deployed") == 0)
                printf("The client received the response from the main server using TCP over port %d. The following files in his/her repository have been deployed.\n", client_port);
                char *response_token = strtok(response, " ");
                //Until the end of the response and if response_token is not NULL
                if (response_token != NULL || response_token != "\0") {
                    while (response_token != NULL) {
                        printf("%s\n", response_token);
                        response_token = strtok(NULL, " ");
                    }
                } else {
                    printf("No files to deployed.\n");
                }
            }

        } else if (strcmp(command_type, "log") == 0) {
            printf("%s sent a log request to the main server.\n", username);
            // Prepare the message to send
            sprintf(message, "%s log", username);
            // Send the message to the server
            send(sockfd, message, strlen(message), 0);

            // Receive the response from the server
            char response[MAXBUFLEN * 10] = {0};
            if ((numbytes = recv(sockfd, response, MAXBUFLEN-1, 0)) == -1) {
                perror("recv");
                exit(1);
            }
            response[numbytes] = '\0';
            printf("The client received the response from the main server using TCP over port %d.\n", client_port);
            int operation_count = 1;
            if (strcmp(response, "No operations have been logged.") == 0) {
                printf("%s\n", response);
            } else {
                char *response_token = strtok(response, ";");
                if (response_token != NULL && response_token[0] != '\0') {
                    while (response_token != NULL) {
                        printf("%d. %s\n", operation_count, response_token);
                        response_token = strtok(NULL, ";");
                        operation_count++;
                    }
                } else {
                    printf("No operations have been logged.\n");
                }
            }
        } else {
            printf("Invalid command. Please try again.\n");
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("The credentials are incorrect. Please try again.\n");
        exit(1);
    }

    sockfd = setup_tcp_socket();
    printf("The client is up and running.\n");

    bool guest = false;

    if (strcmp(argv[1], "guest") == 0 && strcmp(argv[2], "guest") == 0) {
        guest = true;
        printf("You have been granted guest access.\n");
    } else {
        authenticate(argv[1], argv[2]);
        printf("You have been granted member access.\n");
    }

    while (1) {
        char command[1024] = {0};

        if (guest) {
            printf("Please enter the command: ");
            printf("<lookup <username>>.\n");
        } else {
            printf("Please enter the command: ");
            printf("<lookup <username>>, <push <filename>>, <remove <filename>>, <deploy>, <log>.\n");
        }

        // Get the command from the user
        printf("Enter command: ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0';

        // Allow the user to exit the program
        if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
            printf("Closing connection and exiting.\n");
            break;
        }

        handle_command(command, argv[1], guest);

        printf("-----Start a new request-----\n\n");
    }

    close(sockfd);
    return 0;
}
