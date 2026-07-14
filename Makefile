CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lpthread

all: init server client

init: init_data.c
	$(CC) $(CFLAGS) -o init init_data.c

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client init
	rm -rf data

.PHONY: all clean
