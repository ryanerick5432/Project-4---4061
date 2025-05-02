#include "connection_queue.h"

#include <stdio.h>
#include <string.h>

int connection_queue_init(connection_queue_t *queue) {
    // initialize queue array of file descriptors with -1 as "empty"
    for (int i = 0; i < CAPACITY; i++) {
        queue->client_fds[i] = -1;
    }
    // initialize values to 0
    queue->length = 0;
    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->shutdown = 0;

    int result;
    pthread_mutex_t mutex;
    // initialize mutex lock
    if ((result = pthread_mutex_init(&mutex, NULL)) == -1) {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(result));
        return -1;
    }

    // initialize conditions
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
    // lock thread
    if ((result = pthread_mutex_lock(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }

    // check if queue is full or if we are in shutdown mode
    while ((queue->length == CAPACITY) && (queue->shutdown == 0)) {
        // while both checks are true -> wait for the queue to not be full (queue_full)
        if ((result = pthread_cond_wait(&(queue->queue_full), &(queue->lock))) == -1) {
            fprintf(stderr, "pthread_cond_wait: %s\n", strerror(result));
            pthread_mutex_unlock(&(queue->lock));
            return -1;
        }
    }

    // if in shutdown mode -> unlock thread and return error code (-1)
    if (queue->shutdown == 1) {
        pthread_mutex_unlock(&(queue->lock));
        return -1;
    }

    // enqueue the relevant connetion_fd to place in queue (write_idx)
    queue->client_fds[queue->write_idx] = connection_fd;
    // increment write_idx
    // circular queue -> if at end of circle go back to the beginning (0)
    if (queue->write_idx == 4) {
        queue->write_idx = 0;
    } else {
        queue->write_idx++;
    }
    // enqueue adds to the queue -> increase length
    queue->length++;

    // after enqueue -> signal queue_empty as queue has been added to (dequeue can happen)
    if ((result = pthread_cond_signal(&(queue->queue_empty))) == -1) {
        fprintf(stderr, "pthread_cond_signal: %s\n", strerror(result));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    // unlock thread
    if ((result = pthread_mutex_unlock(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_cond_unlock: %s\n", strerror(result));
        return -1;
    }

    return 0;
}

int connection_queue_dequeue(connection_queue_t *queue) {
    int result;
    // lock thread
    if ((result = pthread_mutex_lock(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }

    // check if queue is empty and not in shutdown mode
    while ((queue->length <= 0) && (queue->shutdown == 0)) {
        // if conditions true -> wait for queue_empty signal to know there is content in queue
        if ((result = pthread_cond_wait(&(queue->queue_empty), &(queue->lock))) == -1) {
            fprintf(stderr, "pthread_cond_wait: %s\n", strerror(result));
            pthread_mutex_unlock(&(queue->lock));

            return -1;
        }
    }

    // if in shutdown mode -> unlock thread and return error code (-1)
    if (queue->shutdown == 1) {
        pthread_mutex_unlock(&(queue->lock));
        return -1;
    }

    int temp_fd = 0;
    // grab fd we will dequeue to return later
    temp_fd = queue->client_fds[queue->read_idx];
    // dequeue fd (from read_idx) and set to empty (-1)
    queue->client_fds[queue->read_idx] = -1;
    // dequeued so subtract from length
    queue->length--;
    // increment read_idx
    // circular queue -> if at end of queue circle to beginning (0)
    if (queue->read_idx == 4) {
        queue->read_idx = 0;
    } else {
        queue->read_idx++;
    }

    // singal queue_full as something has been removed from the queue
    if ((result = pthread_cond_signal(&(queue->queue_full))) == -1) {
        fprintf(stderr, "pthread_cond_signal: %s\n", strerror(result));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    // unlock thread
    if ((result = pthread_mutex_unlock(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_cond_unlock: %s\n", strerror(result));
        return -1;
    }

    // return previously saved fd
    return temp_fd;
}

int connection_queue_shutdown(connection_queue_t *queue) {
    int result;
    // lock thread
    if ((result = pthread_mutex_lock(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }
    // set shutdown to true (in shutdown mode)
    queue->shutdown = 1;

    // broadcast queue_full & queue_empty to cleanup leftover waiting actions
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

    // unlock threads
    if ((result = pthread_mutex_unlock(&(queue->lock))) == -1) {
        fprintf(stderr, "pthread_cond_unlock: %s\n", strerror(result));
        return -1;
    }
    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    int result;
    // cleanup -> destroy condition variables & mutex lock
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
