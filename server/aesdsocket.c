#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

void clean_up(struct addrinfo **res) {
    if (*res != NULL) {
        freeaddrinfo(*res);
        *res = NULL;
    }
}

int open_and_bind_socket() {
    // hard-coding as we have this in the assignment instructions:

    struct addrinfo hints, *res = NULL;
    int result;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;

    result = getaddrinfo(NULL, "9000", &hints, &res);
    if (result != 0) {
        printf("Couldn't gai: error=%d; %s\n", result, gai_strerror(result));
        return -1;
    }

    result = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (result == -1) {
        clean_up(&res);
        return -1;
    }

    result = bind(result /*fd*/, res->ai_addr, res->ai_addrlen);
    if (result == -1) {
        clean_up(&res);
        return -1;
    }

    freeaddrinfo(res);

    return result;
}

int main(int argc, char *argv[]) {
    int sock = open_and_bind_socket();
    if (sock == -1) {
        printf("Couldn't open the socket\n");
        return -1;
    }
    else {
        printf("Opened the socket\n");
        close(sock);
        return 0;
    }
}

