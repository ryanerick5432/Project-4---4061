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
    struct sigaction sa;
    sa.sa_handler = handle_sigint;

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

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    struct addrinfo *server;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    if (getaddrinfo(NULL, port, &hints, &server) == -1) {
        perror("getaddrinfo");
        return 1;
    }

    int sockfd;
    if ((sockfd = socket(server->ai_family, server->ai_socktype, server->ai_protocol)) == -1) {
        perror("socket");
        freeaddrinfo(&hints);
        return 1;
    }

    if (bind(sockfd, server->ai_addr, server->ai_addrlen) == -1) {
        perror("bind");
        close(sockfd);
        freeaddrinfo(&hints);
        return 1;
    }

    freeaddrinfo(server);

    if (listen(sockfd, LISTEN_QUEUE_LEN) == -1) {
        perror("listen");
        close(sockfd);
        return 1;
    }

    while (keep_going) {
        int client_fd = accept(sockfd, NULL, NULL);
        if (client_fd == -1) {
            if (errno != EINTR) {
                perror("accept");
                close(sockfd);
                return 1;
            } else {
                break;
            }
        }

        char temp[BUFSIZE];
        if (read_http_request(client_fd, temp) == -1) {
            if (strlen(temp) < 3) {
                strcpy(temp, "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n");

                if (write(client_fd, temp, strlen(temp)) == -1) {
                    perror("write");
                    return 1;
                }
                return 0;
            } else {
                fprintf(stderr, "read_http_request");
                close(client_fd);
                close(sockfd);
                return 1;
            }
        }
        char path_var[BUFSIZE];
        strcpy(path_var, serve_dir);
        strcat(path_var, temp);

        if (write_http_response(client_fd, path_var) == -1) {
            fprintf(stderr, "write_http_request");
            close(client_fd);
            close(sockfd);
            return 1;
        }

        if (close(client_fd) == -1) {
            perror("close");
            close(sockfd);
            return 1;
        }
    }

    if (close(sockfd) == -1) {
        perror("close");
        return 1;
    }
    // TODO Complete the rest of this function
    return 0;
}
