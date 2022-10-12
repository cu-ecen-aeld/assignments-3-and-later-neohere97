CROSS_COMPILE ?= 
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror
TARGET ?= aesdsocket
LDFLAGS ?= -pthread

all:
		make clean
		$(CC) $(CFLAGS) $(LDFLAGS) aesdsocket.c -o $(TARGET)


default:
		$(CC) $(CFLAGS) $(LDFLAGS) aesdsocket.c -o $(TARGET)


# Clean to remove binaries 
clean:
	rm -rf *.o *.out
	rm -rf aesdsocket