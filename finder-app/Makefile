#References : ECEN5813

# Define commands
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror

all: writer

writer: writer.c
	$(CC) $(CFLAGS) -o writer writer.c

clean:
	rm -f writer
