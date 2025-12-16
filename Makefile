CC=gcc
CFLAGS=-Wall -Wextra -g

DEBUGGER=debugger
SRC=src/main.c src/debugger.c

all: $(DEBUGGER) hello

$(DEBUGGER): $(SRC)
	$(CC) $(CFLAGS) -o $(DEBUGGER) $(SRC)

hello: tests/hello.c
	$(CC) -g -no-pie -fno-pie -o tests/hello tests/hello.c

clean:
	rm -f $(DEBUGGER) tests/hello
