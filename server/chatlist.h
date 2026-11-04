#ifndef CHATLIST_H
#define CHATLIST_H

#include "chat.h"
#include <sys/queue.h>

typedef SLIST_HEAD(chatlist_head, chat_data) chatlist_head_t;

int init_chat_list(chatlist_head_t **pphead);
int free_chat_list(chatlist_head_t **pphead);

#endif // CHATLIST_H
