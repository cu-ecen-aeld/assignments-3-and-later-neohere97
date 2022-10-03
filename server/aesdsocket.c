// #include <stdio.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <sys/stat.h>
// #include <unistd.h>
// #include <netdb.h>
// #include <string.h>
// #include <stdlib.h>
// #include <fcntl.h>
// #include <syslog.h>
// #include <linux/fs.h>
// #include <signal.h>

// const char *file = "/var/tmp/aesdsocketdata";
// int sockfd, new_conn_fd;
// char *buf;
// struct addrinfo *servinfo;
// struct sockaddr_storage new_conn;

// void sig_handler(int signum)
// {
//     freeaddrinfo(servinfo);
//     close(sockfd);
//     free(buf);
//     unlink(file);
//     exit(EXIT_SUCCESS);
// }

// int main(int argc, char *argv[])
// {

//     if (signal(SIGINT, sig_handler) == SIG_ERR)
//     {
//         exit(EXIT_FAILURE);
//     }

//     if (signal(SIGTERM, sig_handler) == SIG_ERR)
//     {
//         exit(EXIT_FAILURE);
//     }

//     // Initializing logging
//     openlog(NULL, 0, LOG_USER);

//     int status;
//     struct addrinfo hints;

//     memset(&hints, 0, sizeof(hints));

//     hints.ai_family = AF_INET;
//     hints.ai_socktype = SOCK_STREAM;
//     hints.ai_flags = AI_PASSIVE;

//     if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0)
//     {
//         fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
//         exit(1);
//     }

//     sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
//     if (sockfd == -1)
//         return -1;

//     int yes = 1;

//     if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
//     {
//         perror("setsockopt");
//         exit(1);
//     }

//     if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
//         return -1;

//     if (argc > 1)
//     {
//         char *daemon = "-d";
//         if (strcmp(argv[argc - 1], daemon) == 0)
//         {
//             pid_t pid;
//             pid = fork();

//             if (pid == -1)
//                 return -1;
//             else if (pid != 0)
//                 exit(EXIT_SUCCESS);

//             if (setsid() == -1)
//                 return -1;

//             if (chdir("/") == -1)
//                 return -1;

//             open("/dev/null", O_RDWR);
//             dup(0);
//             dup(0);
//         }
//     }

//     // TODO check for listen error
//     if (listen(sockfd, 5) == -1)
//         return -1;

//     // TODO free sockaddr
//     int buffer_size = 4096;
//     buf = malloc(sizeof(char) * buffer_size);

//     while (1)
//     {
//         // TODO setup File creation/open file descriptor

//         int fd;
//         socklen_t addr_size;
//         addr_size = sizeof(new_conn);

//         // ssize_t nr;

//         // TODO log connected client IP address
//         new_conn_fd = accept(sockfd, (struct sockaddr *)&new_conn, &addr_size);
//         if (new_conn_fd == -1)
//             return -1;

//         // struct sockaddr client_info;
//         // socklen_t addrlen;
//         // addrlen = sizeof(struct sockaddr);

//         // TODO handle error for gerpeername
//         // getpeername(new_conn_fd, &client_info, &addrlen);

//         int recv_return;
//         char *read_buf;
        
//         int pending_write_buffer = 0;
        
//         while (1)
//         {
//             recv_return = recv(new_conn_fd, &buf[pending_write_buffer], buffer_size - pending_write_buffer, 0);
//             if (recv_return == -1)
//                 return -1;

//             if (buf[(pending_write_buffer + recv_return) - 1] == '\n')
//             {
//                 // open file with extra permissins and file mode
//                 // TODO handle open errors
//                 fd = open(file, O_CREAT | O_APPEND | O_RDWR, S_IRWXU | S_IRWXO);
//                 if (fd == -1)
//                     return -1;

//                 // printf("Writing to file %ld bytes \n", strlen(buf));
//                 if (write(fd, buf, strlen(buf)) == -1)
//                     return -1;

//                 if (lseek(fd, 0, SEEK_END) == -1)

//                     return -1;

//                 long filesize = lseek(fd, 0, SEEK_CUR);
//                 if (filesize == -1)
//                     return -1;

//                 if (lseek(fd, 0, SEEK_SET) == -1)
//                     return -1;

//                 read_buf = malloc(filesize+1);
//                 read(fd, read_buf, filesize);

//                 send(new_conn_fd, read_buf, strlen(read_buf), 0);
//                 free(read_buf);

//                 if (close(fd) == -1)
//                 {
//                     perror("close");
//                 }

//                 pending_write_buffer = 0;
//             }
//             else
//             {
//                 pending_write_buffer = pending_write_buffer + recv_return;
//                 buffer_size = buffer_size + recv_return;
//                 buf = realloc(buf, sizeof(char) * (buffer_size+recv_return));
//             }

//             // TODO Log Connection close
//             if (recv_return == 0)
//             {
//                 close(new_conn_fd);
//                 break;
//             }
//         }
//         buffer_size = 4096;
//         buf = realloc(buf, sizeof(char) * (buffer_size));

//     }

//     return 0;
// }




#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <syslog.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/fs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define BACKLOG 10
#define INITIAL_BUFFER_SIZE 1024

// Global Variables
int socketfd; // Socket File Descriptor
int testfile_fd; // Testfile File Descriptor
char *storage_buffer;


// Signal handler
void sig_handler(int signum)
{
    printf("Inside handler function\n");
    if (signum == SIGINT)
        syslog(LOG_ERR, "Caught signal SIGINT, exiting");
    else if (signum == SIGTERM)
        syslog(LOG_ERR, "Caught signal SIGTERM, exiting");
        
    free(storage_buffer);
    close(socketfd);
    close(testfile_fd);
    exit(EXIT_SUCCESS);
}

static int daemon_create()
{
    pid_t pid;

    /* create new process */
    pid = fork ();

    if (pid == -1)
        return -1;

    else if (pid != 0)
        exit (EXIT_SUCCESS);
    /* create new session and process group */
    if (setsid () == -1)
        return -1;
    /* set the working directory to the root directory */
    if (chdir ("/") == -1)
        return -1;

    /* redirect fd's 0,1,2 to /dev/null */
    open ("/dev/null", O_RDWR); /* stdin */
    dup (0); /* stdout */
    dup (0); /* stderror */

    return 0;
}

int main(int argc, char **argv)
{
    // Opening Log to use syslog
    openlog(NULL, 0,  LOG_USER);

    int daemon_flag = 0;

    if (argc == 2){
        char *daemon_argument = argv[1];
        if(strcmp(daemon_argument, "-d") != 0) {
            printf("Invalid argument to run as daemon! Expected \"-d\"\n");
        } else {
            daemon_flag = 1;
        }
    }

    int yes = 1;

    printf("Socket Programming 101!\n");

    signal(SIGINT, sig_handler); // Register signal handler for SIGINT
    signal(SIGTERM, sig_handler); // Register signal handler for SIGTERM

    // Getting a socketfd
    socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketfd == -1) {
        printf("Error creating socket! Error number is %d\n", errno);
        return -1;
    }

    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo; // Points to results

    memset(&hints, 0, sizeof(hints)); // Empty struct
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0) {
        printf("Error in getting addrinfo! Error number is %d\n", errno);
        return -1;
    }

    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
        perror("setsockopt");
        exit(1);
    } 

    status = bind(socketfd, servinfo->ai_addr, sizeof(struct addrinfo));
    if (status == -1) {
        printf("Error binding socket! Error number is %d\n", errno);
        return -1;
    }

    freeaddrinfo(servinfo);

    if (daemon_flag == 1) {
        printf("Daemon Created\n");
        int status = daemon_create();
        if (status == -1) {
            printf("Error Creating Daemon");
        } else if (status == 0) {
            syslog(LOG_DEBUG, "Created Daemon successfully!");
        }
    }

    status = listen(socketfd, BACKLOG);
    if (status == -1) {
        printf("Error listening to connections! Error number is %d\n", errno);
        return -1;
    }

    struct sockaddr_storage test_addr;

    //socklen_t addr_size = sizeof test_addr; //Store address of client
    socklen_t addr_size;

    int new_fd;

    // /var/tmp/aesdsocketdata operations
    // Opening the file specified by filepath with the following flags and allowing the permissions: u=rw,g=rw,o=r
    testfile_fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);

    // In case of an error opening the file
    if (testfile_fd == -1) {
        syslog(LOG_ERR, "Error opening file with error: %d", errno);
    } 
    
    char buffer[INITIAL_BUFFER_SIZE];

    storage_buffer = (char *)malloc(INITIAL_BUFFER_SIZE);

    int storage_buffer_size = INITIAL_BUFFER_SIZE;

    int bytes_received;

    int temp_iterator = 0;

    // Flag that is set when \n is received
    int packet_complete_flag = 0;

    int storage_buffer_size_count = 1;

    int bytes_to_write = 0;

    int file_size = 0;

    char address_string[INET6_ADDRSTRLEN];

    while(1) {

        addr_size = sizeof test_addr;

        new_fd = accept(socketfd, (struct sockaddr *)&test_addr, &addr_size);

        if (new_fd == -1) {
            perror("accept");
            continue;
        } 

        struct sockaddr_in *p = (struct sockaddr_in *)&test_addr;
        syslog(LOG_DEBUG, "Accepted connection from %s", inet_ntop(AF_INET, &p->sin_addr, address_string, sizeof(address_string)));

        while ((bytes_received = recv(new_fd, buffer, sizeof(buffer), 0)) > 0 ) {

            printf("Bytes = %d\n", bytes_received);

            for (int i = 0; i < bytes_received; i++) {
                if (buffer[i] == '\n') {
                    packet_complete_flag = 1;
                    temp_iterator = i+1;
                    break;
                }
            }

            memcpy(storage_buffer + (storage_buffer_size_count - 1)*INITIAL_BUFFER_SIZE, buffer, bytes_received);

            if (packet_complete_flag == 1) {
                bytes_to_write = (storage_buffer_size_count - 1)*INITIAL_BUFFER_SIZE + temp_iterator;
                // packet_complete_flag = 0;
                break;
            } else {
                storage_buffer = realloc(storage_buffer, (storage_buffer_size + INITIAL_BUFFER_SIZE));
                storage_buffer_size += INITIAL_BUFFER_SIZE;
                storage_buffer_size_count += 1;
            }
        }

        printf("Storage Buffer size = %d\n", bytes_to_write);
        if (packet_complete_flag == 1) {

            packet_complete_flag = 0;

            //Writing to file
            int nr = write(testfile_fd, storage_buffer, bytes_to_write); //(storage_buffer_size - 1)*INITIAL_BUFFER_SIZE + temp_iterator);
            file_size += nr; // Adding to current file length

            // TO ADD: lseek, malloc and read
            lseek(testfile_fd, 0, SEEK_SET);

            char *read_buffer = (char *)malloc(file_size);

            ssize_t bytes_read = read(testfile_fd, read_buffer, file_size);
            if (bytes_read == -1)
                perror("read");

            int bytes_sent = send(new_fd, read_buffer, file_size, 0);

            if (bytes_sent == 0) {
                printf("Error in sending to socket!\n");
            }

            free(read_buffer);

            // Cleanup
            memcpy(storage_buffer, storage_buffer + temp_iterator, storage_buffer_size - bytes_to_write);
            storage_buffer = realloc(storage_buffer, INITIAL_BUFFER_SIZE);
            storage_buffer_size_count = 1;
        }

        close(new_fd);
        syslog(LOG_DEBUG, "Closed connection from %s", address_string);
    }

    
    close(socketfd);
    
    return 0;
}
