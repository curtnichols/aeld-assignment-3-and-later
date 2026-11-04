#ifndef MYBUF_H
#define MYBUF_H

#include <stddef.h>

struct mybuf {
    char* buffer;
    size_t buffer_len;
};

void init_mybuf(struct mybuf *buf);
void free_mybuf(struct mybuf *buf);
int add_buffer_length(size_t size, struct mybuf *buf);
int append_to_buffer(const char *data, size_t len, struct mybuf *buf);
int length_of_incoming_data(const struct mybuf *buf);
int truncate_data(size_t skip_first, struct mybuf *buf);

#endif // MYBUF_H
