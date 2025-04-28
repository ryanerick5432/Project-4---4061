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
    // TODO Not yet implemented
    char buf[BUFSIZE];
    int bytes_read;
    int checked = 0;
    while ((bytes_read = read(fd, buf, BUFSIZE)) > 0) {
        if (checked == 0) {
            char *portion = strtok(buf, " ");
            portion = strtok(NULL, " ");
            strcpy(resource_name, portion);
            checked++;
        }
    }
    if (bytes_read < 0) {
        perror("read");
        return -1;
    }
    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    // TODO Not yet implemented
    char buf[BUFSIZE];
    struct stat *stat_buf;
    if (stat(resource_path, stat_buf) == -1) {
        if (errno != ENOENT) {
            perror("stat");
            return -1;
        } else {
            strcpy(buf, "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n");

            if (write(fd, buf, strlen(buf) + 1) == -1) {
                perror("write");
                return 1;
            }
        }
    }

    int content_length = stat_buf->st_size;
    char *file_cpy;
    strcpy(file_cpy, resource_path);
    if (strtok(file_cpy, ".") == -1) {
        perror("strtok");
        return -1;
    }
    char *extens = ".";
    strcat(extens, file_cpy);
    char *content_type = get_mime_type(extens);

    strcpy(buf, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n");

    return 0;
}
