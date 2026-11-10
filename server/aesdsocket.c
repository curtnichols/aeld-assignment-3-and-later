#include "chat.h"
#include "chatlist.h"
#include "mybuf.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>

#define LOG(s) printf("aesdsocket: %s\n", (s))
#define OUTPUT_FILE_PATH "/var/tmp/aesdsocketdata"

sig_atomic_t quitForSignal = 0;

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
    pthread_mutex_t *output_mutex // not owned
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

        int packet_size;

        while ((packet_size = length_of_incoming_data(&buf)) > 0) {
            int cbWritten;

            pthread_mutex_lock(output_mutex);
            int outfile = open(
                OUTPUT_FILE_PATH,
                O_RDWR | O_CREAT | O_APPEND | O_SYNC,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
            if (outfile == -1) {
                perror("Couldn't open output file");
                syslog(LOG_ERR, "Couldn't open output file %s", OUTPUT_FILE_PATH);
                goto cant_open_output;
            }

            cbWritten = write(outfile, buf.buffer, buf.buffer_len);
            if (cbWritten != buf.buffer_len) {
                result = -1;
                break;
            }

            /*
            *   Returns the full content of /var/tmp/aesdsocketdata to the client
            *   as soon as the received data packet completes.
            */
            int cbFileRead;
            lseek(outfile, 0, SEEK_SET);
            while ((cbFileRead = read(outfile, read_buffer, sizeof read_buffer)) > 0) {
                cbWritten = write(sock, read_buffer, cbFileRead);
                if (cbWritten != cbFileRead) {
                    result = -1;
                    break;
                }
            }

            if (result == -1) {
                break;
            }

            lseek(outfile, 0, SEEK_END);
            close(outfile);
            pthread_mutex_unlock(output_mutex);

            // Done with the completed packet.
            if (truncate_data(packet_size, &buf) < 0) {
                printf("truncate_data failed");
                result = -1;
                break;
            }
        }
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
    getnameinfo((struct sockaddr *)ptheir_addr, sizeof(*ptheir_addr), 
                s, NI_MAXHOST * sizeof(char), 
                NULL, 0, NI_NUMERICHOST);
    return s;
}

int start_connection_handler(
    chatlist_head_t *chatlist_head,
    int sockfd,
    pthread_mutex_t *output_mutex)
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

typedef struct {
    pthread_mutex_t *output_mutex;
    pthread_mutex_t *thread_mutex;
    pthread_cond_t *condition;
    volatile bool running;
} timestamp_data_t;

static void *timestamp_thread_proc(void *pargs) {
    syslog(LOG_INFO, "in timestamp_thread_proc");
    
    assert(pargs);
    timestamp_data_t *pdata = (timestamp_data_t*)pargs;
    syslog(LOG_INFO, "in timestamp_thread_proc 2");

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    while (true) {
        pthread_mutex_lock(pdata->thread_mutex);

        if (!pdata->running) {
            pthread_mutex_unlock(pdata->thread_mutex);
            break;
        }

        ts.tv_sec += 10;

        int waitResult = 0;
        
        // Loop for spurious wakeups. 
        while (pdata->running && waitResult != ETIMEDOUT) {
            waitResult = pthread_cond_timedwait(pdata->condition, pdata->thread_mutex, &ts);
        }

        if (!pdata->running) {
            pthread_mutex_unlock(pdata->thread_mutex);
            break;
        }

        pthread_mutex_unlock(pdata->thread_mutex);

        if (waitResult == ETIMEDOUT) {
            const char * const format2822 = "%a, %d %b %Y %T %z";
            char buf[100];
            time_t t = time(NULL);
            struct tm *tmp = localtime(&t);
            
            if (tmp) {
                size_t slen = strftime(buf, sizeof(buf), format2822, tmp);
                if (slen > 0) {
                    pthread_mutex_lock(pdata->output_mutex);

                    int outfile = open(
                        OUTPUT_FILE_PATH,
                        O_RDWR | O_CREAT | O_APPEND | O_SYNC,
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                        
                    if (outfile == -1) {
                        syslog(LOG_ERR, "Couldn't open output file %s: %m", OUTPUT_FILE_PATH);
                    }
                    else {
                        if (write(outfile, "timestamp: ", 11) > 0) {
                            if (write(outfile, buf, slen) > 0) {
                                if (write(outfile, "\n", 1) > 0) { } // compiler warnings as errors
                            }
                        }
                        close(outfile);
                    }

                    pthread_mutex_unlock(pdata->output_mutex);
                }
            }
        }
    }

    return NULL;
}

void stop_timestamp_thread(pthread_t thread_id, timestamp_data_t *pdata) {
    pthread_mutex_lock(pdata->thread_mutex);
    pdata->running = false;
    pthread_cond_signal(pdata->condition); // Instantly wake up the thread if it's sleeping
    pthread_mutex_unlock(pdata->thread_mutex);

    pthread_join(thread_id, NULL); // Wait for it to finish
}

void init_timestamp_condition(pthread_cond_t *pcond) {
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(pcond, &attr);
    pthread_condattr_destroy(&attr);
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
        *as_daemon = strcmp(argv[1], DAEMON_OPTION) == 0;
    }
}

int main(int argc, char *argv[]) {
    syslog(LOG_INFO, "AAA");

    int as_daemon;

    get_options(argc, argv, &as_daemon);
    register_signal_handlers();
    syslog(LOG_INFO, "BBB");

    chatlist_head_t chatlist_head;
    SLIST_INIT(&chatlist_head);

    pthread_mutex_t output_mutex;
    if (pthread_mutex_init(&output_mutex, NULL) != 0) {
        const char * const msg = "Couldn't create the output mutex";
        perror(msg);
        syslog(LOG_ERR, "%s", msg);
        return -1;
    }

    pthread_mutex_t thread_mutex;
    if (pthread_mutex_init(&thread_mutex, NULL) != 0) {
        const char * const msg = "Couldn't create the thread mutex";
        perror(msg);
        syslog(LOG_ERR, "%s", msg);
        return -1;
    }

    int sockfd = open_and_bind_socket();
    if (sockfd == -1) {
        const char * const msg = "Couldn't open the socket";
        perror(msg);
        syslog(LOG_ERR, "%s", msg);
        return -1;
    }

    // FORK ---------------------------------------------

    // Now we can fork for the daemon option (after bind succeeded).
    if (as_daemon) {
        int pid = fork();
        if (pid == -1) {
            const char * const msg = "fork() failed";
            perror(msg);
            syslog(LOG_ERR, "%s", msg);
            return -1;
        }
        else if (pid != 0) {
            const char * const msg = "fork() succeeded";
            syslog(LOG_INFO, "%s", msg);
            exit(EXIT_SUCCESS);
        }

        if (setsid() == -1) {
            const char * const msg = "setsid failed";
            perror(msg);
            syslog(LOG_ERR, "%s", msg);
            return -1;
        }

        umask(0);

        if (chdir("/") == -1) {
            const char * const msg = "chdir failed";
            perror(msg);
            syslog(LOG_ERR, "%s", msg);
            return -1;
        }

        int devnull = open("/dev/null", O_RDWR);
        if (devnull != -1) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);

            if (devnull > 2) {
                close(devnull);
            }
        }
    }

    // POST FORK ----------------------------------------

    openlog("aesdsocket", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon initialized and logging to syslog");

    pthread_cond_t timestamp_condition;
    init_timestamp_condition(&timestamp_condition);

    timestamp_data_t *ptimestamp_data = calloc(1, sizeof(timestamp_data_t));
    ptimestamp_data->output_mutex = &output_mutex;
    ptimestamp_data->thread_mutex = &thread_mutex;
    ptimestamp_data->condition = &timestamp_condition;
    ptimestamp_data->running = 1;

    pthread_t timestamp_thread;
    if (pthread_create(&timestamp_thread, NULL, timestamp_thread_proc, ptimestamp_data) != 0) {
        const char * const msg = "Failed to create thread";
        perror(msg);
        syslog(LOG_ERR, "%s", msg);
        return 1;
    }

    int result;
    do {
        result = start_connection_handler(&chatlist_head, sockfd, &output_mutex);
    }
    while (result == 0 && !quitForSignal);

    close(sockfd);
    pthread_mutex_destroy(&output_mutex);
    pthread_mutex_destroy(&thread_mutex);

    stop_timestamp_thread(timestamp_thread, ptimestamp_data);
    free(ptimestamp_data);

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
