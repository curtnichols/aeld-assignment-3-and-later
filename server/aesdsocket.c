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

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        clean_up(&res);
        return -1;
    }

    printf("open_and_bind_socket: socket=[%d]\n", sockfd);
    result = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if (result == -1) {
        clean_up(&res);
        return -1;
    }

    freeaddrinfo(res);

    return sockfd;
}

int listen_and_accept(int sockfd) {
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    int result;

    printf("sockfd=[%d]\n", sockfd);
    result = listen(sockfd, 1);
    if (result == -1) {
        perror("listen()");
        return result;
    }

    printf("Listening...\n");
    
    addr_size = sizeof their_addr;
    result = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
    if (result == -1) {
    }
    return result;
}

int process_a_connection(int sockfd) {
    int cxn_fd;

    cxn_fd = listen_and_accept(sockfd);
    if (cxn_fd == -1) {
        printf("Couldn't listen and accept\n");
        return -1;
    }

    close(cxn_fd);
    return 0;
}

int main(int argc, char *argv[]) {
    int sockfd = open_and_bind_socket();
    if (sockfd == -1) {
        printf("Couldn't open the socket\n");
        return -1;
    }

    printf("Opened the socket [%d]\n", sockfd);

    int result;

    result = process_a_connection(sockfd);

    close(sockfd);

    return 0;
}

