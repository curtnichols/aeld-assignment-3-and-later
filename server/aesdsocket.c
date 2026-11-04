#include "chat.h"
#include "chatlist.h"
#include "mybuf.h"
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>

#define LOG(s) printf("aesdsocket: %s\n", (s))
#define OUTPUT_FILE_PATH "/var/tmp/aesdsocketdata"

int quitForSignal = 0;

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

    {
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt) != 0) {
            perror("setsockopt SO_REUSEADDR failed");
            close(sockfd);
            return -1;
        }
    }

    result = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if (result == -1) {
        perror("bind failed");
        clean_up(&res);
        return -1;
    }

    freeaddrinfo(res);

    return sockfd;
}

int listen_and_accept(int sockfd, struct sockaddr_storage *their_addr) {
    socklen_t addr_size;
    int result;

    //LOG("Listening...");
    result = listen(sockfd, 1);
    if (result == -1) {
        perror("listen()");
        return result;
    }

    addr_size = sizeof *their_addr;
    result = accept(sockfd, (struct sockaddr *)their_addr, &addr_size);
    if (result == -1) {
        return -1;
    }

    char ip_str[NI_MAXHOST];
    getnameinfo((struct sockaddr *)their_addr, sizeof(*their_addr), 
                ip_str, sizeof(ip_str), 
                NULL, 0, NI_NUMERICHOST);
    syslog(LOG_INFO, "Accepted connection from %s", ip_str);

    return result;
}

int process_a_connection(
    int sock, // owned
    char* ip_str, // owned
    pthread_mutex_t output_mutex // not owned
    )
{
    struct mybuf buf;
    init_mybuf(&buf);
    char read_buffer[16 * 1024]; // stack size on ubuntu is 8192 KB (ulimit -s)

    int result = 0;

    while (!quitForSignal && result == 0) {
        ssize_t cb = read(sock, read_buffer, sizeof read_buffer);
        if (cb < 0) {
            result = -1;
            break;
        }
        else if (cb == 0) {
            // socket was closed in an orderly fashion
            break;
        }

        append_to_buffer(read_buffer, cb, &buf);

        pthread_mutex_lock(&output_mutex);

        int outfile = open(
            OUTPUT_FILE_PATH,
            O_RDWR | O_CREAT | O_APPEND | O_SYNC,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (outfile == -1) {
            perror("Couldn't open output file");
            syslog(LOG_ERR, "Couldn't open output file %s", OUTPUT_FILE_PATH);
            goto cant_open_output;
        }

        int packet_size;

        while ((packet_size = length_of_incoming_data(&buf)) > 0) {
            write(outfile, buf.buffer, buf.buffer_len);

            /*
            *   Returns the full content of /var/tmp/aesdsocketdata to the client
            *   as soon as the received data packet completes.
            */
            int cbFileRead;
            lseek(outfile, 0, SEEK_SET);
            while ((cbFileRead = read(outfile, read_buffer, sizeof read_buffer)) > 0) {
                write(sock, read_buffer, cbFileRead);
            }

            lseek(outfile, 0, SEEK_END);

            // Done with the completed packet.
            if (truncate_data(packet_size, &buf) < 0) {
                printf("truncate_data failed");
                result = -1;
                break;
            }
        }

        close(outfile);

        pthread_mutex_unlock(&output_mutex);
    }

cant_open_output:
    syslog(LOG_INFO, "Closed connection from %s", ip_str);

    free_mybuf(&buf);
    free(ip_str);
    close(sock);

    return result;
}

char *alloc_ip_str(struct sockaddr_storage *ptheir_addr) {
    char *s = calloc(NI_MAXHOST, sizeof(char));
    char ip_str[NI_MAXHOST];
    getnameinfo((struct sockaddr *)ptheir_addr, sizeof(*ptheir_addr), 
                ip_str, NI_MAXHOST * sizeof(char), 
                NULL, 0, NI_NUMERICHOST);
    return s;
}

int start_connection_handler(
    chatlist_head_t *chatlist_head,
    int sockfd,
    pthread_mutex_t output_mutex)
{
    int cxn_fd;
    struct sockaddr_storage their_addr;

    cxn_fd = listen_and_accept(sockfd, &their_addr);
    if (cxn_fd == -1) {
        if (quitForSignal) {
            return 0;
        }

        printf("Couldn't listen and accept\n");
        return -1;
    }
    else {
        int result;
        char *ip_str = alloc_ip_str(&their_addr);
        chat_data_t *pchatdata;

        result = create_chat_thread(
            &process_a_connection,
            cxn_fd,
            ip_str,
            output_mutex,
            &pchatdata);
        if (result == 0) {
            SLIST_INSERT_HEAD(chatlist_head, pchatdata, entries);
        }

        return result;
    }
}

static void signal_handler(int signo, siginfo_t *info, void *context) {
    quitForSignal = 1;
}

void register_signal_handlers() {
    struct sigaction act = { 0 };
    act.sa_sigaction = &signal_handler;

    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}

#define DAEMON_OPTION "-d"

void get_options(int argc, char *argv[], int * const as_daemon) {
    *as_daemon = 0;

    if (argc > 1) {
        *as_daemon = strncmp(argv[1], DAEMON_OPTION, sizeof(DAEMON_OPTION)) == 0;
    }
}

int main(int argc, char *argv[]) {
    int as_daemon;

    get_options(argc, argv, &as_daemon);
    register_signal_handlers();

    chatlist_head_t chatlist_head;
    SLIST_INIT(&chatlist_head);

    pthread_mutex_t output_mutex;
    if (pthread_mutex_init(&output_mutex, NULL) != 0) {
        perror("Couldn't create the output mutex");
        return -1;
    }

    int sockfd = open_and_bind_socket();
    if (sockfd == -1) {
        printf("Couldn't open the socket\n");
        return -1;
    }

    // Now we can fork for the daemon option (after bind succeeded).
    if (as_daemon) {
        int pid = fork();
        if (pid == -1) {
            return -1;
        }
        else if (pid != 0) {
            exit(EXIT_SUCCESS);
        }

        if (setsid() == -1) {
            perror("setsid failed");
            return -1;
        }

        if (chdir("/") == -1) {
            perror("chdir failed");
            return -1;
        }
    }

    int result;
    do {
        result = start_connection_handler(&chatlist_head, sockfd, output_mutex);
    }
    while (result == 0 && !quitForSignal);

    close(sockfd);
    pthread_mutex_destroy(&output_mutex);

    while (!SLIST_EMPTY(&chatlist_head))
    {
        chat_data_t *pchatdata = SLIST_FIRST(&chatlist_head);
        SLIST_REMOVE_HEAD(&chatlist_head, entries);
        delete_chat_data(pchatdata);
    }

    if (quitForSignal) {
        remove(OUTPUT_FILE_PATH);
        syslog(LOG_INFO, "Caught signal, exiting");
    }

    return 0;
}
