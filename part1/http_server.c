#define _GNU_SOURCE

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5

int keep_going = 1;

void handle_sigint(int signo) {
    keep_going = 0;
}

int main(int argc, char **argv) {
    // Set up signal handler for interrupt
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    // ensure SA_RESTART is not set
    sa.sa_flags = 0;

    // install the signal mask
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    // First argument is directory to serve, second is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }
    // Uncomment the lines below to use these definitions:
    const char *serve_dir = argv[1];
    const char *port = argv[2];

    // set up the server
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    struct addrinfo *server;
    // set up the TCP server flags
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    // set up the addrinfo structs to prepare for server
    if (getaddrinfo(NULL, port, &hints, &server) == -1) {
        perror("getaddrinfo");
        return 1;
    }

    // set up the socket to read and write from
    int sockfd;
    if ((sockfd = socket(server->ai_family, server->ai_socktype, server->ai_protocol)) == -1) {
        perror("socket");
        freeaddrinfo(&hints);
        return 1;
    }

    // finalize setting up the socket
    if (bind(sockfd, server->ai_addr, server->ai_addrlen) == -1) {
        perror("bind");
        close(sockfd);
        freeaddrinfo(&hints);
        return 1;
    }

    // no longer need the addrinfo struct
    freeaddrinfo(server);

    // mark the socket to act as a server
    if (listen(sockfd, LISTEN_QUEUE_LEN) == -1) {
        perror("listen");
        close(sockfd);
        return 1;
    }

    // while loop to accept server connections - breaks on sigint handler call
    while (keep_going) {
        int client_fd = accept(sockfd, NULL, NULL);
        if (client_fd == -1) {       // error check if the client did not connect, etc
            if (errno != EINTR) {    // if interrupted via expected sigint interruption, cleanup,
                                     // otherwise error.
                perror("accept");
                close(sockfd);
                return 1;
            } else {
                break;
            }
        }

        // set up the buffer to hold the file name from client tcp request
        char temp[BUFSIZE];
        // read the clients tcp request and extract the file name
        if (read_http_request(client_fd, temp) == -1) {
            // if the file name is not valid (as per the construction of this project,
            // not broadly), prompt for correct output through socket
            if (strlen(temp) < 3) {
                strncpy(temp, "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n", BUFSIZE);

                if (write(client_fd, temp, strlen(temp)) == -1) {
                    perror("write");
                    close(sockfd);
                    return 1;
                }
                return 0;
            } else {    // if errored due to another reason
                fprintf(stderr, "read_http_request");
                close(client_fd);
                close(sockfd);
                return 1;
            }
        }
        // create a buffer for the path
        char path_var[BUFSIZE];
        // make a copy of the server directory as per argv - do not change the original argument
        strncpy(path_var, serve_dir, BUFSIZE);
        // concatenate the path variable and the extracted file name to use for the response to
        // client
        strcat(path_var, temp);

        // write the response utilizing the path of the extracted file
        if (write_http_response(client_fd, path_var) == -1) {
            fprintf(stderr, "write_http_request");
            close(client_fd);
            close(sockfd);
            return 1;
        }

        // clean up the current client
        if (close(client_fd) == -1) {
            perror("close");
            close(sockfd);
            return 1;
        }
    }

    // once done, clean up the socket
    if (close(sockfd) == -1) {
        perror("close");
        return 1;
    }

    return 0;
}
