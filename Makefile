CC = gcc
CFLAGS = -std=c2x -Wall -Wextra -pedantic -O2 -pthread -fopenmp -Iinclude

# make DSIN_PROTECCION), compila sin el mutex de run_add
ifdef SIN_PROTECCION
CFLAGS += -DSIN_PROTECCION
endif


LDLIBS = -lyaml

ASAN_FLAGS = -std=c2x -Wall -Wextra -pedantic -O1 -g -pthread -fopenmp -Iinclude \
	-fsanitize=address -fno-omit-frame-pointer

HEADERS = $(wildcard include/*.h)

LIB_SRC = src/net_util.c src/protocol.c src/run_registry.c src/array.c \
src/parser.c src/parser_helpers.c src/validate_pipeline.c src/graph.c \
src/run.c src/vars.c src/scheduler.c src/ready_queue.c src/executor.c \
src/tasks.c src/joblog.c src/pipeline.c src/process.c

TEST_SRC   = $(wildcard tests/test_*.c)
TEST_BINS  = $(patsubst tests/%.c,build/tests/%,$(TEST_SRC))
TEST_FLAGS = $(ASAN_FLAGS)

all: bin/pipelined bin/pipectl

bin/pipelined: src/pipelined.c $(LIB_SRC) $(HEADERS)
	mkdir -p bin
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@ $(LDLIBS)

bin/pipectl: src/pipectl.c $(HEADERS)
	mkdir -p bin
	$(CC) $(CFLAGS) $(filter %.c,$^) -o $@

build/tests/%: tests/%.c $(LIB_SRC) $(HEADERS)
	mkdir -p build/tests
	$(CC) $(TEST_FLAGS) $(filter %.c,$^) -o $@ $(LDLIBS)

test: $(TEST_BINS)
	@failed=0; \
	for t in $(TEST_BINS); do \
		echo "=== $$t"; \
		./$$t || failed=1; \
	done; \
	exit $$failed

asan: clean
	$(MAKE) all CFLAGS="$(ASAN_FLAGS)"

DSIN_PROTECCION: clean
	$(MAKE) SIN_PROTECCION=1

install:
	sudo apt install -y libyaml-dev

clean:
	rm -rf bin build/tests

.PHONY: all test asan install clean