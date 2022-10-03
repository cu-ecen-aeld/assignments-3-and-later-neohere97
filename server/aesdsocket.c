#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
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

#define BACKLOG 10

const char *file = "/var/tmp/aesdsocketdata";
int sockfd, new_conn_fd;
char *buf;
struct addrinfo *servinfo;
struct sockaddr_storage new_conn;
int fd;

void sig_handler(int signum)
{
    freeaddrinfo(servinfo);
    close(sockfd);
    free(buf);
    unlink(file);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{

    // Register Signal Handlers
    if (signal(SIGINT, sig_handler) == SIG_ERR)
    {
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, sig_handler) == SIG_ERR)
    {
        exit(EXIT_FAILURE);
    }

    // Init Logging
    openlog(NULL, 0, LOG_USER);
    // -----------------------------------------Variables-declarations---------------------------------------------------------------------
    // Buffer handling variables
    int starter_buf_size = 512; // Buffer size to hold network packets
    char buffer[starter_buf_size];
    buf = malloc(starter_buf_size);
    int buf_size = starter_buf_size;
    int bytes_from_client;
    int buf_size_count = 0;
    int bytes_pending = 0;
    int fsize = 0;
    char addrstr[INET6_ADDRSTRLEN];
    int newlineflag = 0;

    // Socket Variables & Declaration
    int new_conn;
    int status;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    struct sockaddr_storage test_addr;
    socklen_t addr_size;

    // -------------------------GetAddrInfo-Socket-Bind-----------------------------------------------------------------------------------
    if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1)
        return -1;

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
    {
        perror("setsockopt");
        exit(1);
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
        return -1;
    // -------------------------------Create-Daemon--after-bind-------------------------------------------------------------------------
    if (argc > 1)
    {
        char *daemon = "-d";
        if (strcmp(argv[argc - 1], daemon) == 0)
        {
            pid_t pid;
            pid = fork();

            if (pid == -1)
                return -1;
            else if (pid != 0)
                exit(EXIT_SUCCESS);

            if (setsid() == -1)
                return -1;

            if (chdir("/") == -1)
                return -1;

            open("/dev/null", O_RDWR);
            dup(0);
            dup(0);
        }
    }

    // -------------------------------Listen--------------------------------------------------------------------------------------------
    if (listen(sockfd, 5) == -1)
        return -1;

    fd = open(file, O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);

    // In case of an error opening the file
    if (fd == -1)
    {
        syslog(LOG_ERR, "Error opening file with error: %d", errno);
    }

    while (1)
    {
        addr_size = sizeof test_addr;
        new_conn = accept(sockfd, (struct sockaddr *)&test_addr, &addr_size);

        struct sockaddr_in *p = (struct sockaddr_in *)&test_addr;
        syslog(LOG_DEBUG, "Accepted connection from %s", inet_ntop(AF_INET, &p->sin_addr, addrstr, sizeof(addrstr)));

        while (1)
        {
            bytes_from_client = recv(new_conn, buffer, sizeof(buffer), 0);

            // When no bytes are sent, connection is closed
            if (bytes_from_client == 0)
                break;

            // Check if the received packet ends with newline
            if (buffer[bytes_from_client - 1] == '\n')
                newlineflag = 1;

            memcpy(buf + (buf_size_count ) * starter_buf_size, buffer, bytes_from_client);

            if (newlineflag == 1)
            {
                bytes_pending = (buf_size_count ) * starter_buf_size + bytes_from_client;
                break;
            }
            else
            {
                // When there is no newline increase the buffer by reallocation
                buf = realloc(buf, (buf_size + starter_buf_size));
                buf_size += starter_buf_size;
                buf_size_count += 1;
            }
        }
        if (newlineflag == 1)
        {
            // computing total file size
            fsize += write(fd, buf, bytes_pending);

            // Get back to the beginning of the file
            lseek(fd, 0, SEEK_SET);

            char *read_buffer = malloc(fsize);

            if (read(fd, read_buffer, fsize) == -1)
                printf("Error reading !\n");

            if (send(new_conn, read_buffer, fsize, 0) == 0)
                printf("Error sending !\n");

            free(read_buffer);
            memcpy(buf, buf + bytes_from_client, buf_size - bytes_pending);
            buf = realloc(buf, starter_buf_size);
            buf_size_count = 0;
            newlineflag = 0;
        }
        close(new_conn);
    }
    return 0;
}