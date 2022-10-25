#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fs.h>
#include <string.h>
#include <fcntl.h>

int main(){
    int fd;
    char a[50];
    char *p = "/dev/aesdchar";
    
    fd = open(p, O_RDWR | O_CREAT | O_TRUNC,
                             S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
    
    printf("fd is %d\n", fd);
    
    read(fd,&a, 5);

    printf("contents is %s\n", a);

    close(fd);
}