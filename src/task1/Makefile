CC = gcc
CFLAGS = -Wall -Wextra -D_GNU_SOURCE

all: backend frontend

backend: backend.c common.h
	$(CC) $(CFLAGS) backend.c -o backend

frontend: frontend.c common.h
	$(CC) $(CFLAGS) frontend.c -o frontend

clean:
	rm -f backend frontend auth.log
	rm -f /tmp/auth_socket
