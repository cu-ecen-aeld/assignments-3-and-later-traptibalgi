#References : ECEN5813

# Define commands
CC = gcc
CFLAGS = -Wall -Werror

# The executable
EXEC = writer

all: $(EXEC)

$(EXEC): writer.o
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $(EXEC) writer.o

writer.o: writer.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c writer.c

clean:
	rm -f $(EXEC) *.o
