#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>

typedef struct thread_data
{
    pthread_t threadID;
    int conn_fd;
    int thread_complete_flag;
    SLIST_ENTRY(thread_data) entries;
} thread_data;


void main() {
    int i=0;
    thread_data *datap=NULL;

    SLIST_HEAD(slisthead, thread_data) head;
    SLIST_INIT(&head);

    // Write.
    for (i=0; i<10; i++) {
        datap = malloc(sizeof(thread_data));
        datap->conn_fd = (int) (i * 1000);
        printf("Insert: %d\n", datap->conn_fd);
        SLIST_INSERT_HEAD(&head, datap, entries);
    }
    printf("\n");

    // Read1.
    SLIST_FOREACH(datap, &head, entries) {
        printf("Read1: %d\n", datap->conn_fd);
    }
    printf("\n");

    // Read2 (remove).
    while (!SLIST_EMPTY(&head)) {
        datap = SLIST_FIRST(&head);
        printf("Read2: %d\n", datap->conn_fd);
        SLIST_REMOVE_HEAD(&head, entries);
        free(datap);
    }
}



