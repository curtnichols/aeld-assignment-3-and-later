CROSS_COMPILE ?=
# Suggested command-line variable: aarch64-none-linux-gnu-

CC  = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
LD  = $(CROSS_COMPILE)ld

CFLAGS ?=

writer : writer.o
	$(CC) $(CFLAGS) -o writer writer.o
	chmod 755 writer

writer.o : writer.c
	$(CC) $(CFLAGS) -c writer.c

clean : 
	rm -f writer writer.o

