//  /***************************************************************************
//   * AESD Assignment 5
//   * Author: Chinmay Shalawadi
//   * Institution: University of Colorado Boulder
//   * Mail id: chsh1552@colorado.edu
//   * References: Stack Overflow, Man Pages, Beejus
//   ***************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/time.h>
#include "queue.h"
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <linux/fs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <pthread.h>

//-----------------------Global--Defines-----------------------------
#define BACKLOG (10)
#define RECEIVE_BUFFER (1024)
#define SEND_BUFFER (1024)
#define TEMP_BUFFER (1024)

//--------------------Globals-Struct---------------------------------------

typedef struct thread_data
{
    pthread_t threadID;
    int conn_fd;
    int thread_complete_flag;
    SLIST_ENTRY(thread_data)
    entries;
} thread_data;

struct globals_aesdsocket
{
    int sockfd;
    int filefd;
    int file_writehead;
    thread_data head;
    pthread_mutex_t file_mutex;
} aesdsocket;

SLIST_HEAD(slisthead, thread_data);

//--------------------Function Declarations--------------------------------
static void register_signal_handlers();
static void bind_to_port(char *port);
static void open_temp_file(char *file);
static void daemonify(int argc, char *argv[]);
static void accept_connections_loop(struct slisthead *head);
static void write_to_file();
static void send_file();
static void write_timestamp();
static void start_timer();
void *thread_function(void *threadparams);
// static void create_head_element();

// ---------------------------------main--------------------------------------------

int main(int argc, char *argv[])
{
    // Init Logging
    openlog(NULL, 0, LOG_USER);

    register_signal_handlers();

    bind_to_port("9000");

    pthread_mutex_init(&aesdsocket.file_mutex, NULL);

    daemonify(argc, argv);

    open_temp_file("/var/tmp/aesdsocketdata");

    start_timer();

    struct slisthead head;

    SLIST_INIT(&head);

    while (1)
    {
        accept_connections_loop(&head);
    }
    return 0;
}

static void write_timestamp()
{
    time_t timer;
    char buffer[26], finalString[40];
    struct tm *tm_info;

    timer = time(NULL);
    tm_info = localtime(&timer);

    strftime(buffer, 26, "%a, %d %b %Y %T %z", tm_info);
    sprintf(finalString, "timestamp:%s\n", buffer);

    write_to_file(finalString, strlen(finalString));
}

// ---------------------------------accept_connections_loop--------------------------------------------
static void accept_connections_loop(struct slisthead *head)
{

    struct sockaddr_storage test_addr;
    socklen_t addr_size;
    addr_size = sizeof(test_addr);
    int conn_fd;

    conn_fd = accept(aesdsocket.sockfd, (struct sockaddr *)&test_addr, &addr_size);
    if (conn_fd == -1)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
        }
        else
        {
            perror("accept():");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        char addrstr[INET6_ADDRSTRLEN];
        struct sockaddr_in *p = (struct sockaddr_in *)&test_addr;
        syslog(LOG_DEBUG, "Accepted connection from %s",
               inet_ntop(AF_INET, &p->sin_addr, addrstr, sizeof(addrstr)));

        thread_data *new_node = NULL;

        new_node = malloc(sizeof(thread_data));

        new_node->conn_fd = conn_fd;
        new_node->thread_complete_flag = 0;
        SLIST_INSERT_HEAD(head, new_node, entries);
        pthread_create(&(new_node->threadID), NULL, thread_function, new_node);

        thread_data *test;
        thread_data *temp;

    
        SLIST_FOREACH_SAFE(test, head, entries,temp)
        {
            if (test->thread_complete_flag)
            {
                pthread_join(test->threadID, NULL);
                SLIST_REMOVE(head, test, thread_data, entries);
                free(test);
            }
        }
    }
}

void *thread_function(void *threadparams)
{
    thread_data *data = (thread_data *)threadparams;

    int bytes_received, newlineflag = 0;

    char *receive_buffer, *temp_buffer;
    int temp_buffer_writehead, temp_buffer_size;

    // printf("Allocating memory for buffers:");
    receive_buffer = (char *)calloc((size_t)RECEIVE_BUFFER, sizeof(char));

    if (receive_buffer == NULL)
    {
        printf("Error allocating memeroy for receive buffer \n");
        exit(EXIT_FAILURE);
    }

    temp_buffer = (char *)calloc((size_t)TEMP_BUFFER, sizeof(char));
    if (temp_buffer == NULL)
    {
        printf("Error allocating memeroy for temp buffer \n");
        exit(EXIT_FAILURE);
    }
    temp_buffer_writehead = 0;
    temp_buffer_size = TEMP_BUFFER;

    printf("SUCCESS\n");

    while (1)
    {

        bytes_received = recv(data->conn_fd, receive_buffer, RECEIVE_BUFFER, 0);

        if (bytes_received == -1)
            perror("recv():");

        if (bytes_received == 0)
        {
            printf("Closing the connection \n");
            close(data->conn_fd);
            break;
        }

        memcpy(&temp_buffer[temp_buffer_writehead], receive_buffer, bytes_received);
        temp_buffer_writehead += bytes_received;

        int i;
        for (i = 0; i < temp_buffer_writehead; i++)
        {
            if (temp_buffer[i] == '\n')
            {
                newlineflag = 1;
                break;
            }
        }

        if (newlineflag)
        {

            write_to_file(temp_buffer, temp_buffer_writehead);
            temp_buffer_writehead = 0;

            send_file(data->conn_fd);
            newlineflag = 0;
        }
        else
        {
            temp_buffer = (char *)realloc(temp_buffer,
                                          ((size_t)(temp_buffer_size + TEMP_BUFFER)) *
                                              sizeof(char));
            temp_buffer_size += TEMP_BUFFER;
        }
    }

    free(receive_buffer);
    free(temp_buffer);
    close(data->conn_fd);

    data->thread_complete_flag = 1;    
    return NULL;
}

// ---------------------------------send_file--------------------------------------------
static void send_file(int conn_fd)
{
    char *send_buffer;

    send_buffer = (char *)calloc((size_t)SEND_BUFFER, sizeof(char));
    if (send_buffer == NULL)
    {
        printf("Error allocating memeroy for send buffer \n");
        exit(EXIT_FAILURE);
    }
    // TODO add mutex protection
    if (lseek(aesdsocket.filefd, 0, SEEK_SET) == -1)
    {
        perror("lseek():");
    }

    int chunk = SEND_BUFFER;
    int current_filehead = aesdsocket.file_writehead;

    while (current_filehead > 0)
    {
        if (current_filehead < SEND_BUFFER)
            chunk = current_filehead;

        pthread_mutex_lock(&aesdsocket.file_mutex);
        if (read(aesdsocket.filefd, send_buffer, chunk) == -1)
        {
            perror("read():");
        }
        pthread_mutex_unlock(&aesdsocket.file_mutex);

        if (send(conn_fd, send_buffer, chunk, 0) == -1)
        {
            perror("send():");
        }

        current_filehead -= chunk;
    }

    free(send_buffer);
}

// ---------------------------------write_to_file--------------------------------------------
static void write_to_file(char *buffer, int buffer_size)
{
    pthread_mutex_lock(&aesdsocket.file_mutex);
    if (write(aesdsocket.filefd,
              buffer,
              (size_t)buffer_size) != buffer_size)
    {
        printf("ERR! write didn't write everything \n");
        exit(EXIT_FAILURE);
    }

    aesdsocket.file_writehead += buffer_size;
    pthread_mutex_unlock(&aesdsocket.file_mutex);
}

// ---------------------------------open_temp_file--------------------------------------------
static void open_temp_file(char *file)
{
    printf("Opening file:%s:", file);

    aesdsocket.filefd = open(file, O_RDWR | O_CREAT | O_TRUNC,
                             S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);

    if (aesdsocket.filefd == -1)
    {
        syslog(LOG_ERR, "Error opening file with error: %d", errno);
        exit(EXIT_FAILURE);
    }

    aesdsocket.file_writehead = 0;
    printf("SUCCESS\n");
}
// ---------------------------------sig_handler--------------------------------------------
void sig_handler(int signo)
{
    if (signo == SIGALRM)
    {
        write_timestamp();
    }

    if (signo == SIGINT || signo == SIGTERM)
    {
        close(aesdsocket.sockfd);
        unlink("/var/tmp/aesdsocketdata");
        exit(EXIT_SUCCESS);
    }
}

// -----------------------------------daemonify---------------------------------------------
static void daemonify(int argc, char *argv[])
{

    if (argc > 1)
    {
        char *daemon = "-d";
        if (strcmp(argv[argc - 1], daemon) == 0)
        {
            pid_t pid;
            pid = fork();

            if (pid == -1)
                exit(EXIT_FAILURE);
            else if (pid != 0)
                exit(EXIT_SUCCESS);

            if (setsid() == -1)
                exit(EXIT_FAILURE);

            if (chdir("/") == -1)
                exit(EXIT_FAILURE);

            open("/dev/null", O_RDWR);
            dup(0);
            dup(0);
        }
        printf("Attempting to become a Daemon:SUCCESS\n");
    }
}

// -----------------------------bind_to_port----------------------------------------
static void bind_to_port(char *port)
{
    int status, yes = 1;
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    printf("Getting address info to bind to:");
    if ((status = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    printf("SUCCESS\n");

    printf("Registering the Socket and Getting Sockfd:");
    aesdsocket.sockfd = socket(servinfo->ai_family,
                               servinfo->ai_socktype,
                               servinfo->ai_protocol);
    if (aesdsocket.sockfd == -1)
    {
        perror("socket():");
        exit(1);
    }

    // fcntl(aesdsocket.sockfd, F_SETFL, O_NONBLOCK);

    printf("SUCCESS\n");

    if (setsockopt(aesdsocket.sockfd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &yes,
                   sizeof(yes)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }

    printf("Attempting to bind to port: ");
    if (bind(aesdsocket.sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        perror("bind()");

        exit(1);
    }
    printf("SUCCESS\n");
    freeaddrinfo(servinfo);

    printf("Listening to port %s:", port);
    if (listen(aesdsocket.sockfd, BACKLOG) == -1)
    {

        perror("listen():");
        exit(1);
    }
    printf("SUCCESS\n");
}

// --------------------------register_signal_handlers------------------------------------------
static void register_signal_handlers()
{

    struct sigaction act;

    act.sa_handler = &sig_handler;

    sigfillset(&act.sa_mask);

    act.sa_flags = SA_RESTART;

    printf("Registering signal handler SIGINT:");
    if (sigaction(SIGINT, &act, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    printf("SUCCESS\n");

    printf("Registering signal handler SIGTERM:");
    if (sigaction(SIGTERM, &act, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    printf("SUCCESS\n");

    printf("Registering signal handler SIGALRM:");
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    printf("SUCCESS\n");
}

static void start_timer()
{
    struct itimerval delay;
    int ret;
    delay.it_value.tv_sec = 0;
    delay.it_value.tv_usec = 500;
    delay.it_interval.tv_sec = 10;
    delay.it_interval.tv_usec = 0;
    ret = setitimer(ITIMER_REAL, &delay, NULL);
    if (ret)
    {
        perror("setitimer");
        return;
    }
}
// ---------------------------------End--------------------------------------------