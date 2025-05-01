#include "connection_queue.h"

#include <stdio.h>
#include <string.h>

int connection_queue_init(connection_queue_t *queue) {
    for (int i = 0; i < CAPACITY; i++) {
        queue->client_fds[i] = -1;
    }
    queue->length = 0;
    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->shutdown = 0;

    int result;
    pthread_mutex_t mutex;
    if ((result = pthread_mutex_init(&mutex, NULL)) == -1) {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(result));
        return -1;
    }

    if ((result = pthread_cond_init(&(queue->queue_empty), NULL)) == -1) {
        fprintf(stderr, "pthread_cond_init: %s\n", strerror(result));
        return -1;
    }

    if ((result = pthread_cond_init(&(queue->queue_full), NULL)) == -1) {
        fprintf(stderr, "pthread_cond_full: %s\n", strerror(result));
        return -1;
    }

    return 0;
}

int connection_queue_enqueue(connection_queue_t *queue, int connection_fd) {
    int result;
    if ((result = pthread_mutex_lock(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }
    printf("enqueue\n");
    while ((queue->length == CAPACITY) && (queue->shutdown == 0)) {
        if ((result = pthread_cond_wait(&(queue->queue_full), &(queue->lock))) == -1) {
            fprintf(stderr, "pthread_cond_wait: %s\n", strerror(result));
            pthread_mutex_unlock(&(queue->lock));
            return -1;
        }
    }
    printf("mid enqueue\n");
    if (queue->shutdown == 1) {
        pthread_mutex_unlock(&(queue->lock));
        return -1;
    }
    printf("after enqueue shutdown segment\n");
    queue->client_fds[queue->write_idx] = connection_fd;
    if (queue->write_idx == 4) {
        queue->write_idx = 0;
    } else {
        queue->write_idx++;
    }
    queue->length++;

    if ((result = pthread_cond_signal(&(queue->queue_empty))) == -1) {
        fprintf(stderr, "pthread_cond_signal: %s\n", strerror(result));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    printf("pre enqueue unlock\n");
    if ((result = pthread_mutex_unlock(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_cond_unlock: %s\n", strerror(result));
        return -1;
    }
    printf("post enqueue unlock\n");
    // printf("queue length after enqueue: %d\n", queue->length);
    // printf("%d, %d, %d, %d, %d\n", queue->client_fds[0], queue->client_fds[1],
    // queue->client_fds[2],
    //        queue->client_fds[3], queue->client_fds[4]);
    printf("end enqueue\n");
    return 0;
}

int connection_queue_dequeue(connection_queue_t *queue) {
    int result;
    if ((result = pthread_mutex_lock(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }
    printf("dequeue\n");
    while ((queue->length <= 0) && (queue->shutdown == 0)) {
        if ((result = pthread_cond_wait(&(queue->queue_empty), &(queue->lock))) == -1) {
            fprintf(stderr, "pthread_cond_wait: %s\n", strerror(result));
            pthread_mutex_unlock(&(queue->lock));

            return -1;
        }
    }
    printf("mid dequeue\n");
    if (queue->shutdown == 1) {
        printf("shutdown if\n");
        pthread_mutex_unlock(&(queue->lock));
        return -1;
    }
    printf("after dequeue shutdown check\n");
    int temp_fd = 0;
    temp_fd = queue->client_fds[queue->read_idx];
    queue->client_fds[queue->read_idx] = -1;
    queue->length--;
    if (queue->read_idx == 4) {
        queue->read_idx = 0;
    } else {
        queue->read_idx++;
    }
    printf("cond_signal\n");
    if ((result = pthread_cond_signal(&(queue->queue_full))) == -1) {
        fprintf(stderr, "pthread_cond_signal: %s\n", strerror(result));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    printf("unlock\n");
    if ((result = pthread_mutex_unlock(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_cond_unlock: %s\n", strerror(result));
        return -1;
    }
    // printf("queue length after dequeue: %d\n", queue->length);
    // printf("%d, %d, %d, %d, %d\n", queue->client_fds[0], queue->client_fds[1],
    // queue->client_fds[2],
    //        queue->client_fds[3], queue->client_fds[4]);
    return temp_fd;
}

int connection_queue_shutdown(connection_queue_t *queue) {
    int result;
    if ((result = pthread_mutex_lock(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }
    queue->shutdown = 1;
    printf("shutdown\n");

    if ((result = pthread_cond_broadcast(&(queue->queue_full))) == -1) {
        fprintf(stderr, "pthread_cond_broadcast: %s\n", strerror(result));
        pthread_mutex_unlock(&(queue->lock));
        return -1;
    }

    if ((result = pthread_cond_broadcast(&(queue->queue_empty))) == -1) {
        fprintf(stderr, "pthread_cond_broadcast: %s\n", strerror(result));
        pthread_mutex_unlock(&(queue->lock));
        return -1;
    }

    if ((result = pthread_mutex_unlock(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_cond_unlock: %s\n", strerror(result));
        return -1;
    }
    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    int result;
    if ((result = pthread_cond_destroy(&(queue->queue_full))) == -1) {
        fprintf(stderr, "pthread_cond_destroy: %s\n", strerror(result));
        return -1;
    }
    if ((result = pthread_cond_destroy(&(queue->queue_empty))) == -1) {
        fprintf(stderr, "pthread_cond_destroy: %s\n", strerror(result));
        return -1;
    }
    if ((result = pthread_mutex_destroy(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_mutex_destroy: %s\n", strerror(result));
        return -1;
    }
    return 0;
}
