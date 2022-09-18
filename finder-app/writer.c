//  /***************************************************************************
//   * AESD Assignment 2
//   * Author: Chinmay Shalawadi
//   * Institution: University of Colorado Boulder
//   * Mail id: chsh1552@colorado.edu
//   * References: Stack Overflow, Man Pages
//   ***************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>

// -----------------------------------main-----------------------------------------------------------
int main(int argc, char *argv[])
{
    // Initializing logging
    openlog(NULL, 0, LOG_USER);

    // check if there are two arguments
    if (argc == 3)
    {
        // check if string null
        if (!argv[2])
            return 1;

        int fd;
        ssize_t nr;

        // open file with extra permissins and file mode    
        printf("Opening the file \n");
        fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXO);

        if (fd > 0)
        {
            //syslog when writing
            syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
            nr = write(fd, argv[2], strlen(argv[2]));
            if (nr == -1 || nr < strlen(argv[2]))
            {
                printf("Write Failed or incomplete \n");
                syslog(LOG_ERR, "Write Failed or incomplete nr -> %ld", nr);
                return 1;
            }
        }
        else
        {
            printf("Error opening the file \n");
            syslog(LOG_ERR, "Error Opening the file %s", argv[1]);
            return 1;
        }
    }
    else
    {
        printf("Not enough arguments passed \n");
        return 1;
    }
    return 0;
}
// ----------------------------------------End-----------------------------------------------------