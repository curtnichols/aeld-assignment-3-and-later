CROSS_COMPILE ?=
# Suggested command-line variable: aarch64-none-linux-gnu-

CC  = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
LD  = $(CROSS_COMPILE)ld

CFLAGS ?=

all : aesdsocket

aesdsocket : aesdsocket.o
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.o
	chmod 755 aesdsocket

aesdsocket.o : aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c

clean : 
	rm -f aesdsocket aesdsocket.o

