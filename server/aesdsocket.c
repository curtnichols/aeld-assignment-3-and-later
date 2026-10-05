#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>

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

struct mybuf {
    char* buffer;
    size_t buffer_len;
};

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

int process_a_connection(int sockfd) {
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

    int outfile = open(
        OUTPUT_FILE_PATH,
        O_RDWR | O_CREAT | O_APPEND | O_SYNC,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (outfile == -1) {
        perror("Couldn't open output file");
        close(cxn_fd);
        return -1;
    }

    struct mybuf buf;
    init_mybuf(&buf);
    char read_buffer[16 * 1024]; // stack size on ubuntu is 8192 KB (ulimit -s)

    int result = 0;

    while (!quitForSignal && result == 0) {
        ssize_t cb = read(cxn_fd, read_buffer, sizeof read_buffer);
        if (cb < 0) {
            result = -1;
            break;
        }
        else if (cb == 0) {
            // socket was closed in an orderly fashion
            break;
        }

        append_to_buffer(read_buffer, cb, &buf);
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
                write(cxn_fd, read_buffer, cbFileRead);
            }

            lseek(outfile, 0, SEEK_END);

            // Done with the completed packet.
            if (truncate_data(packet_size, &buf) < 0) {
                printf("truncate_data failed");
                result = -1;
                break;
            }
        }
    }

    free_mybuf(&buf);

    close(outfile);
    close(cxn_fd);

    char ip_str[NI_MAXHOST];
    getnameinfo((struct sockaddr *)&their_addr, sizeof(their_addr), 
                ip_str, sizeof(ip_str), 
                NULL, 0, NI_NUMERICHOST);
    syslog(LOG_INFO, "Closed connection from %s", ip_str);

    return result;
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
        result = process_a_connection(sockfd);
    }
    while (result == 0 && !quitForSignal);

    close(sockfd);

    if (quitForSignal) {
        remove(OUTPUT_FILE_PATH);
        syslog(LOG_INFO, "Caught signal, exiting");
    }

    return 0;
}
