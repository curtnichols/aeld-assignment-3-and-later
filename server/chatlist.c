#include "chatlist.h"
#include <stdlib.h>

int init_chat_list(chatlist_head_t **pphead) {
    chatlist_head_t *phead = calloc(1, sizeof(chatlist_head_t));
    *pphead = phead;
    return phead == NULL ? 1 : 0;
}

int free_chat_list(chatlist_head_t **pphead) {
    chatlist_head_t *phead = *pphead;
    free(phead);
    *pphead = NULL;
    return 0;
}
