#ifndef CHAT_H
#define CHAT_H

#include <pthread.h>
#include <sys/queue.h>

typedef struct chat_data {
    pthread_t thread_id;
    volatile int done;

    SLIST_ENTRY(chat_data) entries;
} chat_data_t;

typedef int connection_thread_proc_fn(
    int sock, // owned
    char* ip_str, // owned
    pthread_mutex_t *output_mutex // not owned
    );

int create_chat_thread(
    connection_thread_proc_fn *thread_proc,
    int sock,
    char *ip_str,
    pthread_mutex_t *output_mutex,
    chat_data_t **ppdata);

void delete_chat_data(chat_data_t *pdata);
    
#endif // CHAT_H
