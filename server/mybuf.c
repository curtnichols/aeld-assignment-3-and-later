#include "mybuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_mybuf(struct mybuf *buf) {
    buf->buffer = NULL;
    buf->buffer_len = 0;
}

void free_mybuf(struct mybuf *buf) {
    free(buf->buffer); // safe if NULL
    buf->buffer = NULL;
    buf->buffer_len = 0;
}

int add_buffer_length(size_t size, struct mybuf *buf) {
    if (size == 0) {
        printf("size should not be zero\n");
        return -1;
    }

    if (buf->buffer == NULL) {
        buf->buffer = malloc(size);
        if (buf->buffer == NULL) {
            return -1;
        }

        buf->buffer_len = size;
    }
    else {
        const size_t new_size = buf->buffer_len + size;
        char* b = realloc(buf->buffer, new_size);
        if (b == NULL) {
            return -1;
        }

        buf->buffer = b;
        buf->buffer_len = new_size;
    }

    return 0;
}

int append_to_buffer(const char *data, size_t len, struct mybuf *buf) {
    const size_t original_size = buf->buffer_len;
    int result = add_buffer_length(len, buf);
    if (result != 0) {
        return result;
    }

    memcpy(buf->buffer + original_size, data, len);
    return 0;
}

int length_of_incoming_data(const struct mybuf *buf) {
    char* match = memchr(buf->buffer, '\n', buf->buffer_len);
    if (match == NULL) {
        return -1;
    }
    else {
        return match - buf->buffer + 1; // +1 for the \n
    }
}

int truncate_data(size_t skip_first, struct mybuf *buf) {
    if (skip_first > buf->buffer_len) {
        printf("skip is greater than buffer len");
        return -1;
    }

    const size_t remainder = buf->buffer_len - skip_first;

    if (remainder == 0) {
        free_mybuf(buf);
    }
    else {
        // memmove() is safe for overlapping buffers.
        memmove(buf->buffer, buf->buffer + skip_first, remainder);
        char *b = realloc(buf->buffer, remainder);
        if (b == NULL) {
            printf("truncate_data: realloc failed\n");
            return -1;
        }

        buf->buffer = b;
        buf->buffer_len = remainder;
    }

    return 0;
}
