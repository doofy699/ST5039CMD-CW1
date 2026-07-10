CC = gcc
CFLAGS = -Wall -Wextra -O2
PTHREAD = -pthread

TEST_BINARIES = test_binary benign cpu_hog fork_bomb ignore_sigterm

all: sandbox $(TEST_BINARIES)

sandbox: sandbox.c
	$(CC) $(CFLAGS) $(PTHREAD) -o $@ $^

test_binary: test_binary.c
	$(CC) $(CFLAGS) -o $@ $^

benign: benign.c
	$(CC) $(CFLAGS) -o $@ $^

cpu_hog: cpu_hog.c
	$(CC) $(CFLAGS) -o $@ $^

fork_bomb: fork_bomb.c
	$(CC) $(CFLAGS) -o $@ $^

ignore_sigterm: ignore_sigterm.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f sandbox $(TEST_BINARIES) sandbox.log

.PHONY: all clean
