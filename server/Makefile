CROSS_COMPILE =
CC = gcc

all:
		make clean
		$(CROSS_COMPILE)$(CC) -Wall -Werror aesdsocket.c -o aesdsocket


default:
		$(CROSS_COMPILE)$(CC) -Wall -Werror aesdsocket.c -o aesdsocket


# Clean to remove binaries 
clean:
	rm -rf *.o *.out
	rm -rf aesdsocket