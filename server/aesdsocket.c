#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <syslog.h>
#include <linux/fs.h>
#include <signal.h>

const char *file = "/var/tmp/aesdsocketdata";
int sockfd, new_conn_fd;
char *buf;
struct addrinfo *servinfo;
struct sockaddr_storage new_conn;

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

    if (signal(SIGINT, sig_handler) == SIG_ERR)
    {
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, sig_handler) == SIG_ERR)
    {
        exit(EXIT_FAILURE);
    }

    // Initializing logging
    openlog(NULL, 0, LOG_USER);

    int status;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

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

    // TODO check for listen error
    if (listen(sockfd, 5) == -1)
        return -1;

    // TODO free sockaddr
    int buffer_size = 4096;
    buf = malloc(sizeof(char) * buffer_size);

    while (1)
    {
        // TODO setup File creation/open file descriptor

        int fd;
        socklen_t addr_size;
        addr_size = sizeof(new_conn);

        // ssize_t nr;

        // TODO log connected client IP address
        new_conn_fd = accept(sockfd, (struct sockaddr *)&new_conn, &addr_size);
        if (new_conn_fd == -1)
            return -1;

        // struct sockaddr client_info;
        // socklen_t addrlen;
        // addrlen = sizeof(struct sockaddr);

        // TODO handle error for gerpeername
        // getpeername(new_conn_fd, &client_info, &addrlen);

        int recv_return;
        char *read_buf;
        
        int pending_write_buffer = 0;
        
        while (1)
        {
            recv_return = recv(new_conn_fd, &buf[pending_write_buffer], buffer_size - pending_write_buffer, 0);
            if (recv_return == -1)
                return -1;

            if (buf[(pending_write_buffer + recv_return) - 1] == '\n')
            {
                // open file with extra permissins and file mode
                // TODO handle open errors
                fd = open(file, O_CREAT | O_APPEND | O_RDWR, S_IRWXU | S_IRWXO);
                if (fd == -1)
                    return -1;

                // printf("Writing to file %ld bytes \n", strlen(buf));
                if (write(fd, buf, strlen(buf)) == -1)
                    return -1;

                if (lseek(fd, 0, SEEK_END) == -1)

                    return -1;

                long filesize = lseek(fd, 0, SEEK_CUR);
                if (filesize == -1)
                    return -1;

                if (lseek(fd, 0, SEEK_SET) == -1)
                    return -1;

                read_buf = malloc(filesize+1);
                read(fd, read_buf, filesize);

                send(new_conn_fd, read_buf, strlen(read_buf), 0);
                free(read_buf);

                if (close(fd) == -1)
                {
                    perror("close");
                }

                pending_write_buffer = 0;
            }
            else
            {
                pending_write_buffer = pending_write_buffer + recv_return;
                buffer_size = buffer_size + recv_return;
                buf = realloc(buf, sizeof(char) * (buffer_size+recv_return));
            }

            // TODO Log Connection close
            if (recv_return == 0)
            {
                close(new_conn_fd);
                break;
            }
        }
        buffer_size = 4096;
        buf = realloc(buf, sizeof(char) * (buffer_size));

    }

    return 0;
}