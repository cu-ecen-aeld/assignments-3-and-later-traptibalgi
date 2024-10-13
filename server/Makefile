CC ?= $(CROSS_COMPILE)gcc

CFLAGS ?= -Werror -Wall

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200112L -o aesdsocket aesdsocket.c $(LDFLAGS)

clean:
	rm -f aesdsocket