CC = gcc
CFLAGS = -std=c2x -Wall -Wextra -pedantic -O2 -pthread -fopenmp -Iincludes

all: bin/pipelined bin/pipectl

bin/pipelined: src/pipelined.c src/net_util.c src/protocol.c src/run_registry.c
	mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@

bin/pipectl: src/pipectl.c
	mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf bin

.PHONY: all cleans