#include "http.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFSIZE 512

const char *get_mime_type(const char *file_extension) {
    if (strcmp(".txt", file_extension) == 0) {
        return "text/plain";
    } else if (strcmp(".html", file_extension) == 0) {
        return "text/html";
    } else if (strcmp(".jpg", file_extension) == 0) {
        return "image/jpeg";
    } else if (strcmp(".png", file_extension) == 0) {
        return "image/png";
    } else if (strcmp(".pdf", file_extension) == 0) {
        return "application/pdf";
    } else if (strcmp(".mp3", file_extension) == 0) {
        return "audio/mpeg";
    }

    return NULL;
}

int read_http_request(int fd, char *resource_name) {
    // make a temp var to iterate through the request, reading it to completion
    char buf[BUFSIZE];
    int bytes_read;
    int checked = 0;
    while ((bytes_read = read(fd, buf, BUFSIZE)) > 0) {
        // if reading the first block (guaranteed to contain the file name requested)
        // then parse for the file name
        if (checked == 0) {
            char *portion = strtok(buf, " ");
            portion = strtok(NULL, " ");
            strncpy(resource_name, portion, BUFSIZE);
            checked++;
        }
        // if at the end of the request, break the loop
        if ((buf[bytes_read - 4] == '\r') && (buf[bytes_read - 1] == '\n')) {
            break;
        }
    }
    // if ended on an error
    if (bytes_read < 0) {
        perror("read");
        return -1;
    }
    // if the file name is not valid (such as /), error
    if (strlen(resource_name) < 3) {
        return -1;
    }
    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    // create the buffer for the http response
    char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);

    // make a copy of the resource path, so as to not alter it
    char file_cpy[BUFSIZE];
    memset(file_cpy, 0, BUFSIZE);
    strncpy(file_cpy, resource_path, BUFSIZE);

    // iterate through the path to find the extension
    char *portion = strtok(file_cpy, ".");
    portion = strtok(NULL, ".");

    // store the extension for the request information
    char extens[6];
    memset(extens, 0, 6);
    if (snprintf(extens, 6, ".%s", portion) < 0) {
        fprintf(stderr, "snprintf");
        return -1;
    }

    struct stat stat_buf;

    // check that the file exists
    if (stat(resource_path, &stat_buf) == -1) {
        if (errno != ENOENT) {    // if the error was not due to nonexistent file
            perror("stat");
            return -1;
        } else {    // if file didnt exist write a 404 not found
            strncpy(buf, "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n", BUFSIZE);

            if (write(fd, buf, strlen(buf)) == -1) {
                perror("write");
                return 1;
            }
            return 0;
        }
    }

    // find the length of the files contents
    int content_length = stat_buf.st_size;

    // create the string for the http response
    if (snprintf(buf, BUFSIZE, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n",
                 get_mime_type(extens), content_length) < 0) {
        fprintf(stderr, "snprintf");
        return -1;
    }

    // send the main part of http response
    if (write(fd, buf, strlen(buf)) == -1) {
        perror("write");
        return -1;
    }

    // following sourced/based from simple_http_client.c, from the lecture code.
    // open the file
    int file_fd = open(resource_path, O_RDONLY, S_IRUSR);
    if (file_fd == -1) {
        perror("open");
        return -1;
    }

    // begin to read through the file, and write its contents to the the end of the http response
    int bytes_read;
    while ((bytes_read = read(file_fd, buf, BUFSIZE)) > 0) {
        if (write(fd, buf, bytes_read) == -1) {
            perror("write");
            close(file_fd);
            return -1;
        }
    }

    // if did not read correctly
    if (bytes_read == -1) {
        perror("read");
        close(file_fd);
        return -1;
    }

    // clean up the file
    if (close(file_fd) != 0) {
        perror("close");
        return -1;
    }

    return 0;
}
