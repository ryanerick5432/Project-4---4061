#define _GNU_SOURCE

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "connection_queue.h"
#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define N_THREADS 5

int keep_going = 1;
const char *serve_dir;

void handle_sigint(int signo) {
    keep_going = 0;
}

void *thread_func(void *arg) {
    while (keep_going == 1) {
        connection_queue_t *queue = (connection_queue_t *) arg;
        // printf("start thread\n");

        int client_fd = connection_queue_dequeue(queue);
        if (client_fd == -1) {
            pthread_exit((void *) 1);
        }

        char temp[BUFSIZE];
        if (read_http_request(client_fd, temp) == -1) {
            if (strlen(temp) < 3) {
                strcpy(temp, "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n");

                if (write(client_fd, temp, strlen(temp)) == -1) {
                    perror("write");
                    pthread_exit((void *) 1);
                }
                pthread_exit((void *) 0);
            } else {
                fprintf(stderr, "read_http_request");
                close(client_fd);
                pthread_exit((void *) 1);
            }
        }
        char path_var[BUFSIZE];
        strcpy(path_var, serve_dir);
        strcat(path_var, temp);
        // printf("%s\n", path_var);
        if (write_http_response(client_fd, path_var) == -1) {
            fprintf(stderr, "write_http_request");
            close(client_fd);
            pthread_exit((void *) 1);
        }
        // printf("Finished Writing command: %s :\n", path_var);
        if (close(client_fd) == -1) {
            perror("close");
            pthread_exit((void *) 1);
        }
    }
    return NULL;
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
    serve_dir = argv[1];
    const char *port = argv[2];

    connection_queue_t queue;
    if (connection_queue_init(&queue) == -1) {
        fprintf(stderr, "init");
        return 1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    struct addrinfo *server;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    if (getaddrinfo(NULL, port, &hints, &server) == -1) {
        perror("getaddrinfo");
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        return 1;
    }

    int sockfd;
    if ((sockfd = socket(server->ai_family, server->ai_socktype, server->ai_protocol)) == -1) {
        perror("socket");
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        freeaddrinfo(&hints);
        return 1;
    }

    if (bind(sockfd, server->ai_addr, server->ai_addrlen) == -1) {
        perror("bind");
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        close(sockfd);
        freeaddrinfo(&hints);
        return 1;
    }

    freeaddrinfo(server);

    if (listen(sockfd, LISTEN_QUEUE_LEN) == -1) {
        perror("listen");
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        close(sockfd);
        return 1;
    }

    sigset_t mask;
    sigset_t oldm;
    if (sigfillset(&mask) == -1) {
        perror("sigemptyset");
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        close(sockfd);
    }
    if (sigprocmask(SIG_BLOCK, &mask, &oldm) == -1) {
        perror("sigprocmask");
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        close(sockfd);
        return 1;
    }

    pthread_t *threads = malloc(N_THREADS * sizeof(pthread_t));

    if (threads == NULL) {
        perror("malloc");
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        close(sockfd);
        return 1;
    }

    for (long i = 0; i < N_THREADS; i++) {
        int result = pthread_create(threads + i, NULL, thread_func, &queue);
        if (result != 0) {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(result));
            close(sockfd);
            connection_queue_shutdown(&queue);
            connection_queue_free(&queue);
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            free(threads);
            return 1;
        }
    }

    if (sigprocmask(SIG_SETMASK, &oldm, NULL) == -1) {
        perror("sigprocmask");
        close(sockfd);
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        for (int j = 0; j < N_THREADS; j++) {
            pthread_join(threads[j], NULL);
        }
        free(threads);
        return 1;
    }

    while (keep_going) {
        // printf("accept\n");
        int client_fd = accept(sockfd, NULL, NULL);
        if (client_fd == -1) {
            if (errno != EINTR) {
                perror("accept");
                close(sockfd);
                connection_queue_shutdown(&queue);
                connection_queue_free(&queue);
                for (int j = 0; j < N_THREADS; j++) {
                    pthread_join(threads[j], NULL);
                }
                free(threads);
                return 1;
            } else {
                break;
            }
        }
        // printf("enqueue\n");
        if (connection_queue_enqueue(&queue, client_fd) == -1) {
            perror("connection_queue_enqueue");
            close(sockfd);
            connection_queue_shutdown(&queue);
            connection_queue_free(&queue);
            for (int j = 0; j < N_THREADS; j++) {
                pthread_join(threads[j], NULL);
            }
            free(threads);
            return 1;
        }

        // if (close(client_fd) == -1) {
        //     perror("close");
        //     close(sockfd);
        //     for (int j = 0; j < N_THREADS; j++) {
        //         pthread_join(threads[j], NULL);
        //     }
        //     free(threads);
        //     return 1;
        // }
    }
    // printf("shutdown");
    if (connection_queue_shutdown(&queue) == -1) {
        perror("connection_queue_shutdown");
        close(sockfd);
        for (int j = 0; j < N_THREADS; j++) {
            pthread_join(threads[j], NULL);
        }
        free(threads);
        connection_queue_free(&queue);
        return 1;
    }
    // printf("free\n");
    if (connection_queue_free(&queue) == -1) {
        perror("connection_queue_free");
        close(sockfd);
        for (int j = 0; j < N_THREADS; j++) {
            pthread_join(threads[j], NULL);
        }
        free(threads);
        return 1;
    }
    // printf("join");
    for (int i = 0; i < N_THREADS; i++) {
        int result = pthread_join(threads[i], NULL);
        if (result != 0) {
            fprintf(stderr, "pthread_join failed: %s\n", strerror(result));
            for (int j = i + 1; j < N_THREADS; j++) {
                pthread_join(threads[j], NULL);
            }
            free(threads);
            close(sockfd);
            return 1;
        }
    }

    free(threads);

    if (close(sockfd) == -1) {
        perror("close");
        return 1;
    }

    // TODO Complete the rest of this function
    return 0;
}
