CC = gcc
CFLAGS = -std=c2x -Wall -Wextra -pedantic -O2 -pthread -fopenmp -Iinclude

LDLIBS = -lyaml

all: bin/pipelined bin/pipectl

bin/pipelined: src/pipelined.c src/net_util.c src/protocol.c src/run_registry.c src/array.c src/parser.c src/parser_helpers.c src/validate_pipeline.c src/graph.c src/run.c src/vars.c src/scheduler.c src/ready_queue.c src/executor.c src/tasks.c src/executor.c src/tasks.c src/joblog.c
	mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

bin/pipectl: src/pipectl.c
	mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@

install:
	sudo apt install -y libyaml-dev	

clean:
	rm -rf bin

.PHONY: all clean