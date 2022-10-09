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
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <linux/fs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>

//-----------------------Global--Defines-----------------------------
#define BACKLOG (10)
#define RECEIVE_BUFFER (1024)
#define SEND_BUFFER (1024)
#define TEMP_BUFFER (1024)

//--------------------Function Declarations--------------------------------
static void register_signal_handlers();
static void bind_to_port(char *port);
static void open_temp_file(char *file);
static void daemonify(int argc, char *argv[]);
static void accept_connections_loop();
static void write_to_file();
static void send_file();

//--------------------Globals-Struct---------------------------------------
struct globals_aesdsocket
{
    int sockfd;
    int filefd;
    int file_writehead;

} aesdsocket;

// ---------------------------------main--------------------------------------------
int main(int argc, char *argv[])
{
    // Init Logging
    openlog(NULL, 0, LOG_USER);

    register_signal_handlers();

    bind_to_port("9000");

    daemonify(argc, argv);

    open_temp_file("/var/tmp/aesdsocketdata");

    while (1)
    {
        printf("Accepting Connections...\n");
        accept_connections_loop();
    }
    return 0;
}

// ---------------------------------accept_connections_loop--------------------------------------------
static void accept_connections_loop()
{
    struct sockaddr_storage test_addr;
    socklen_t addr_size;
    addr_size = sizeof(test_addr);
    int conn_fd;

    conn_fd = accept(aesdsocket.sockfd, (struct sockaddr *)&test_addr, &addr_size);

    if (conn_fd == -1)
    {
        perror("accept():");
    }
    else
    {
        char addrstr[INET6_ADDRSTRLEN];
        
        struct sockaddr_in *p = (struct sockaddr_in *)&test_addr;
        syslog(LOG_DEBUG, "Accepted connection from %s",
               inet_ntop(AF_INET, &p->sin_addr, addrstr, sizeof(addrstr)));

        printf("Accepted Connection \n");
        int bytes_received, newlineflag = 0;

        char *receive_buffer, *temp_buffer;
        int temp_buffer_writehead, temp_buffer_size;

        printf("Allocating memory for buffers:");
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

            bytes_received = recv(conn_fd, receive_buffer, RECEIVE_BUFFER, 0);

            if (bytes_received == -1)
                perror("recv():");

            if (bytes_received == 0)
            {
                printf("Closing the connection \n");
                close(conn_fd);
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

                send_file(conn_fd);
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
        close(conn_fd);
    }
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

        if (read(aesdsocket.filefd, send_buffer, chunk) == -1)
        {
            perror("read():");
        }

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
    if (write(aesdsocket.filefd,
              buffer,
              (size_t)buffer_size) != buffer_size)
    {
        printf("ERR! write didn't write everything \n");
        exit(EXIT_FAILURE);
    }

    aesdsocket.file_writehead += buffer_size;
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
void sig_handler()
{
    close(aesdsocket.sockfd);
    unlink("/var/tmp/aesdsocketdata");
    exit(EXIT_SUCCESS);
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
}
// ---------------------------------End--------------------------------------------