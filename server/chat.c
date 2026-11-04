#include "chat.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    connection_thread_proc_fn *thread_proc;
    char *ip_str;
    int sock;
    pthread_mutex_t output_mutex;
} chat_thread_data_t;

static void *chat_thread_proc(void *pargs) {
    assert(pargs);
    chat_thread_data_t *pthreaddata = (chat_thread_data_t*)pargs;

    pthreaddata->thread_proc(
        pthreaddata->sock,
        pthreaddata->ip_str,
        pthreaddata->output_mutex);

    free(pargs);
    return NULL;
}

int create_chat_thread(
    connection_thread_proc_fn *thread_proc,
    int sock,
    char *ip_str,
    pthread_mutex_t output_mutex,
    chat_data_t **ppdata)
{
    int result = 0;
    chat_thread_data_t *pthreaddata = calloc(1, sizeof(chat_thread_data_t));
    chat_data_t *pchatdata = calloc(1, sizeof(chat_data_t));
    if (!pthreaddata || !pchatdata) goto alloc_fail;

    pthreaddata->thread_proc = thread_proc;
    pthreaddata->ip_str = ip_str;
    pthreaddata->sock = sock;
    pthreaddata->output_mutex = output_mutex;

    result = pthread_create(&pchatdata->thread_id, NULL, &chat_thread_proc, pthreaddata);
    if (result != 0) goto thread_create_fail;

    pchatdata->done = 0;
    *ppdata = pchatdata;

    return 0;

thread_create_fail:
    close(sock);
alloc_fail:
    free(pchatdata);
    free(*ppdata);
    *ppdata = NULL;
    return 1;
}

void delete_chat_data(chat_data_t *pdata) {
    assert(pdata->done);
    pthread_join(pdata->thread_id, NULL);
}
