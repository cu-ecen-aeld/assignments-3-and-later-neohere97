//  /***************************************************************************
//   * AESD Assignment 6
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


#define USE_AESD_CHAR_DEVICE 

//-----------------------Global--Defines----------------------------------
#define BACKLOG (10)
#define SEND_BUFFER (1024)
#define RECEIVE_BUFFER (1024)

//----------------Thread data structure------------------------------------------
typedef struct thread_data
{
    pthread_t threadID;
    int conn_fd;
    int thread_complete_flag;
    SLIST_ENTRY(thread_data)
    entries;
} thread_data;
//--------------------Globals-Struct---------------------------------------------
struct globals_aesdsocket
{
    int sockfd;
    int filefd;
    int file_writehead;
    int exit_flag;
    char *send_buffer;
    thread_data head;
#ifndef USE_AESD_CHAR_DEVICE 
    int write_flag;
    pthread_mutex_t file_mutex;
#endif

} aesdsocket;

// Defining a struct with name slisthead
SLIST_HEAD(slisthead, thread_data);

//--------------------Function Declarations---------------------------------------
static void register_signal_handlers();
static void bind_to_port(char *port);
static void open_temp_file(char *file);
static void daemonify(int argc, char *argv[]);
static void accept_connections_loop(struct slisthead *head);
static void write_to_file();
static void send_file();
static void setup_send_buffer();
static void cleanup(struct slisthead *head);
static void join_threads(struct slisthead *head);
void *thread_function(void *threadparams);

#ifndef USE_AESD_CHAR_DEVICE 
static void write_timestamp();
static void start_timer();
#endif

// Function Code
//  ---------------------------------main------------------------------------------
int main(int argc, char *argv[])
{
    // Init Logging
    openlog(NULL, 0, LOG_USER);

    register_signal_handlers();

    bind_to_port("9000");

    setup_send_buffer();

#ifndef USE_AESD_CHAR_DEVICE 
    pthread_mutex_init(&aesdsocket.file_mutex, NULL);
#endif

    daemonify(argc, argv);

#ifndef USE_AESD_CHAR_DEVICE 
    open_temp_file("/var/tmp/aesdsocketdata");
#endif

#ifndef USE_AESD_CHAR_DEVICE 
    start_timer();
#endif

    struct slisthead head;

    SLIST_INIT(&head);

    while (1)
    {
#ifndef USE_AESD_CHAR_DEVICE 
        if (aesdsocket.write_flag)
        {
            write_timestamp();
            aesdsocket.write_flag = 0;
        }
#endif

        if (aesdsocket.exit_flag)
        {
            cleanup(&head);
        }

        join_threads(&head);
        accept_connections_loop(&head);
    }
    return 0;
}
// ---------------------------------accept_connections_loop-----------------------
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
            // nothing to be done and go back to accept connections loop
        }
        else
        {
            perror("accept():");
            aesdsocket.exit_flag = 1;
        }
    }
    else
    {
        char addrstr[INET6_ADDRSTRLEN];
        struct sockaddr_in *p = (struct sockaddr_in *)&test_addr;
        syslog(LOG_DEBUG, "Accepted connection from %s",
               inet_ntop(AF_INET, &p->sin_addr, addrstr, sizeof(addrstr)));

        // Creating a new LL node
        thread_data *new_node = NULL;
        new_node = malloc(sizeof(thread_data));
        new_node->conn_fd = conn_fd;
        new_node->thread_complete_flag = 0;

        // Inserting the element at the head
        SLIST_INSERT_HEAD(head, new_node, entries);

        if (pthread_create(&(new_node->threadID), NULL, thread_function, new_node) != 0)
        {
            perror("pthread_create():");
        }
    }
}
// ---------------------------------thread_function-------------------------------
void *thread_function(void *threadparams)
{
    thread_data *data = (thread_data *)threadparams;

    int bytes_received, newlineflag = 0;
    char *receive_buffer;

    // The below two variables track the total size of receive buffer as it gets reallocated
    // writehead also tracks the current capacity/where to write next
    int receive_buffer_writehead, receive_buffer_size;

    // Allocating and Initializing the receive buffer
    receive_buffer = (char *)calloc((size_t)RECEIVE_BUFFER, sizeof(char));
    if (receive_buffer == NULL)
    {
        printf("Error allocating memeroy for receive buffer \n");
        exit(EXIT_FAILURE);
    }

    // Initializing the receive buffer trackers
    receive_buffer_writehead = 0;
    receive_buffer_size = RECEIVE_BUFFER;

    while (1)
    {

        bytes_received = recv(data->conn_fd, &(receive_buffer[receive_buffer_writehead]), RECEIVE_BUFFER, 0);

        if (bytes_received == -1)
        {
            perror("recv():");
            goto end;
        }

        if (bytes_received == 0)
        {
            close(data->conn_fd);
            break;
        }

        receive_buffer_writehead += bytes_received;

        // Find the location of the newline character in the buffer
        int i;
        for (i = 0; i < receive_buffer_writehead; i++)
        {
            if (receive_buffer[i] == '\n')
            {
                newlineflag = 1;
                break;
            }
        }

        if (newlineflag)
        {
            // Flushing contents of receive buffer to file
            write_to_file(receive_buffer, receive_buffer_writehead);
            // Resetting the writehead
            receive_buffer_writehead = 0;

            // Send the contents of the file
            send_file(data->conn_fd);
            newlineflag = 0;
        }
        else
        {
            // When there's no new line, increasing the size of receive buffer
            receive_buffer = (char *)realloc(receive_buffer,
                                             ((size_t)(receive_buffer_size + RECEIVE_BUFFER)) *
                                                 sizeof(char));

            // Updating the size of receive buffer
            receive_buffer_size += RECEIVE_BUFFER;
        }
    }

end:
    // Clean up, set flag and exit
    free(receive_buffer);
    close(data->conn_fd);
    data->thread_complete_flag = 1;
    return NULL;
}
// ---------------------------------setup_send_buffer-----------------------------
static void setup_send_buffer()
{
    // Setup the send buffer which is used to send data back
    aesdsocket.send_buffer = (char *)calloc((size_t)SEND_BUFFER, sizeof(char));
    if (aesdsocket.send_buffer == NULL)
    {
        printf("Error allocating memeroy for send buffer \n");
        exit(EXIT_FAILURE);
    }
}
// ---------------------------------join_threads----------------------------------
static void join_threads(struct slisthead *head)
{
    thread_data *thread, *temp;

    // Going through all the threads and joining the ones which have finished
    SLIST_FOREACH_SAFE(thread, head, entries, temp)
    {
        if (thread->thread_complete_flag)
        {
            pthread_join(thread->threadID, NULL);
            SLIST_REMOVE(head, thread, thread_data, entries);
            free(thread);
        }
    }
}
// ---------------------------------cleanup---------------------------------------
static void cleanup(struct slisthead *head)
{
    // closing socket, removing file and freeing buffer

#ifndef USE_AESD_CHAR_DEVICE 
    close(aesdsocket.filefd);
    unlink("/var/tmp/aesdsocketdata");
#endif

    free(aesdsocket.send_buffer);
    exit(EXIT_SUCCESS);
}
// ---------------------------------write_timestamp-------------------------------
#ifndef USE_AESD_CHAR_DEVICE 
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
#endif
// ---------------------------------send_file-------------------------------------
static void send_file(int conn_fd)
{
#ifdef USE_AESD_CHAR_DEVICE 
    // open_temp_file("/dev/aesdchar");
#endif

// Go to the beginning of the file
#ifndef USE_AESD_CHAR_DEVICE 
    if (lseek(aesdsocket.filefd, 0, SEEK_SET) == -1)
    {
        perror("lseek():");
    }
#endif

    // Setting the chunk size to default send buffer size
    int chunk = SEND_BUFFER;
    // File writehead tracks total data on the file
    int current_filehead = aesdsocket.file_writehead;
    int read_data;

    // Send data in max size of SEND_BUFFER Chunks
    while (current_filehead > 0)
    {
        printf("current writehead is %d \n", current_filehead);
        if (current_filehead < SEND_BUFFER)
            chunk = current_filehead;

            // Locking the file_mutex

#ifndef USE_AESD_CHAR_DEVICE 
        pthread_mutex_lock(&aesdsocket.file_mutex);
#endif
        // Read only the chunk size, and file descriptor updates
        read_data = read(aesdsocket.filefd, aesdsocket.send_buffer, chunk);
        if (read_data == -1)
        {
            perror("read():");
        }
        printf("data read is %d \n", read_data);
#ifndef USE_AESD_CHAR_DEVICE 
        pthread_mutex_unlock(&aesdsocket.file_mutex);
#endif

        // Send data which was read into the buffer
        if (send(conn_fd, aesdsocket.send_buffer, read_data, 0) == -1)
        {
            perror("send():");
        }

        // Updating remaining data to be sent
        current_filehead -= read_data;
    }
#ifdef USE_AESD_CHAR_DEVICE 
    close(aesdsocket.filefd);
#endif
}
// ---------------------------------write_to_file---------------------------------
static void write_to_file(char *buffer, int buffer_size)
{
#ifdef USE_AESD_CHAR_DEVICE 
    open_temp_file("/dev/aesdchar");
#else
    pthread_mutex_lock(&aesdsocket.file_mutex);
#endif

    // Write to file with mutex protection
    int data_written = write(aesdsocket.filefd,
                             buffer,
                             (size_t)buffer_size);

    if (data_written != buffer_size)
    {
        printf("ERR! write didn't write everything %d\n", data_written);
        perror("write():");
        aesdsocket.exit_flag = 1;
    }

    aesdsocket.file_writehead += buffer_size;

#ifdef USE_AESD_CHAR_DEVICE 
    // close(aesdsocket.filefd);
#else
    pthread_mutex_unlock(&aesdsocket.file_mutex);
#endif
}
// ---------------------------------open_temp_file--------------------------------
static void open_temp_file(char *file)
{
    printf("Opening file:%s:", file);

    aesdsocket.filefd = open(file, O_RDWR | O_CREAT | O_TRUNC,
                             S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);

    if (aesdsocket.filefd == -1)
    {
        syslog(LOG_ERR, "Error opening file with error: %d", errno);
        aesdsocket.exit_flag = 1;
    }

    // aesdsocket.file_writehead = 0;
    printf("SUCCESS fd-> %d\n", aesdsocket.filefd);
}
// ---------------------------------sig_handler-----------------------------------
void sig_handler(int signo)
{
#ifndef USE_AESD_CHAR_DEVICE 
    if (signo == SIGALRM)
        aesdsocket.write_flag = 1;
#endif

    if (signo == SIGINT || signo == SIGTERM)
        aesdsocket.exit_flag = 1;
}
// -----------------------------------daemonify-----------------------------------
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
                aesdsocket.exit_flag = 1;
            else if (pid != 0)
                exit(EXIT_SUCCESS);

            if (setsid() == -1)
                aesdsocket.exit_flag = 1;

            if (chdir("/") == -1)
                aesdsocket.exit_flag = 1;

            open("/dev/null", O_RDWR);
            dup(0);
            dup(0);
        }
        printf("Attempting to become a Daemon:SUCCESS\n");
    }
}
// -----------------------------bind_to_port--------------------------------------
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

    // Tells the kernel to make the socket nonblocking
    fcntl(aesdsocket.sockfd, F_SETFL, O_NONBLOCK);

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
// --------------------------register_signal_handlers-----------------------------
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
        aesdsocket.exit_flag = 1;
    }
    printf("SUCCESS\n");

    printf("Registering signal handler SIGTERM:");
    if (sigaction(SIGTERM, &act, NULL) == -1)
    {
        perror("sigaction");
        aesdsocket.exit_flag = 1;
    }
    printf("SUCCESS\n");

    printf("Registering signal handler SIGALRM:");
#ifndef USE_AESD_CHAR_DEVICE
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        aesdsocket.exit_flag = 1;
    }
#endif
    printf("SUCCESS\n");
}
// --------------------------start_timer------------------------------------------
#ifndef USE_AESD_CHAR_DEVICE
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
#endif
// ---------------------------------End-------------------------------------------