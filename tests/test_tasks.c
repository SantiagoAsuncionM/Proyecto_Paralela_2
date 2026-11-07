/* Unit tests for the built-in tasks (src/tasks.c).

Tests:
  1. WordCount: same output with 1 and 4 threads (determinism).
  2. Histogram: same output with 1 and 4 threads (determinism).
  3. Cpu: same seed gives same CPU_RESULT.
  4. Invalid input fails.
  5. Unknown task fails.
  6. Sleep stops when canceled.
  7. Sleep stops if timeout.

Build (from the repo root, one command):
  gcc -std=c2x -Wall -Wextra -pedantic -O2 -pthread -fopenmp -Iinclude
      tests/test_tasks.c src/tasks.c src/vars.c src/array.c -o bin/test_tasks

Run: ./bin/test_tasks
  Prints PASS/FAIL per test; exit code 0 only if all pass.
  Uses a temporary folder /tmp/test_tasks_XXXXXX, deleted at the end.

Memory check: valgrind --leak-check=full ./bin/test_tasks
*/

#define _POSIX_C_SOURCE 200809L
#include "tasks.h"
#include "vars.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int passed = 0, total = 0;
static char workdir[] = "/tmp/test_tasks_XXXXXX";

//*** Helpers  ***//

static void check(int ok, const char *name) {
    total++;
    if (ok) passed++;
    printf("%s  %s\n", ok ? "PASS" : "FAIL", name);
}

// array_t of par_var_t*, as the parser delivers a step's inputs.
static array_t make_inputs(par_var_t *vars, size_t count, void **slots) {
    for (size_t i = 0; i < count; i++) slots[i] = &vars[i];
    array_t inputs = { .items = slots, .num_items = count, .capacity = (int)count };
    return inputs;
}

static task_context_t make_context(const int *cancel) {
    task_context_t context = { .workdir = workdir, .cancel = cancel };
    return context;
}

// True if both files exist and are identical.
static int same_file(const char *first, const char *second) {
    char first_path[512], second_path[512];
    snprintf(first_path, sizeof first_path, "%s/%s", workdir, first);
    snprintf(second_path, sizeof second_path, "%s/%s", workdir, second);
    FILE *first_file = fopen(first_path, "rb");
    FILE *second_file = fopen(second_path, "rb");
    int same = first_file != NULL && second_file != NULL;
    while (same) {
        int first_char = fgetc(first_file), second_char = fgetc(second_file);
        if (first_char != second_char) same = 0;
        if (first_char == EOF || second_char == EOF) break;
    }
    if (first_file) fclose(first_file);
    if (second_file) fclose(second_file);
    return same;
}

// ~2 MB test text with repeated words.
static void make_corpus(void) {
    static const char *words[] = { "SanJose", "Alajuela", "Cartago", "Heredia", "Limon",
                                   "Puntarenas", "Guanacaste" };
    char path[512];
    snprintf(path, sizeof path, "%s/corpus.txt", workdir);
    FILE *file = fopen(path, "w");
    for (int i = 0; i < 300000; i++)
        fprintf(file, "%s%c", words[i % 7], i % 12 == 11 ? '\n' : ' ');
    fclose(file);
}

static double seconds_since(struct timespec start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)(now.tv_sec - start.tv_sec) + (double)(now.tv_nsec - start.tv_nsec) / 1e9;
}

//*** Tests  ***//

static void test_wordcount_deterministic(void) {
    task_context_t context = make_context(NULL);
    void *slots[4];
    par_var_t one[]  = { {"file", "corpus.txt"}, {"threads", "1"},
                         {"output", "wc1.txt"}, {"top", "5"} };
    par_var_t four[] = { {"file", "corpus.txt"}, {"threads", "4"},
                         {"output", "wc4.txt"}, {"top", "5"} };
    array_t inputs_one = make_inputs(one, 4, slots);
    int ok = task_run("WordCount@1", &inputs_one, &context) == TASK_OK;
    array_t inputs_four = make_inputs(four, 4, slots);
    ok = ok && task_run("WordCount@1", &inputs_four, &context) == TASK_OK;
    check(ok && same_file("wc1.txt", "wc4.txt"),
          "WordCount: same result with 1 and 4 threads");
}

static void test_histogram_deterministic(void) {
    task_context_t context = make_context(NULL);
    void *slots[4];
    par_var_t one[]  = { {"file", "corpus.txt"}, {"bins", "16"},
                         {"threads", "1"}, {"output", "h1.csv"} };
    par_var_t four[] = { {"file", "corpus.txt"}, {"bins", "16"},
                         {"threads", "4"}, {"output", "h4.csv"} };
    array_t inputs_one = make_inputs(one, 4, slots);
    int ok = task_run("Histogram@1", &inputs_one, &context) == TASK_OK;
    array_t inputs_four = make_inputs(four, 4, slots);
    ok = ok && task_run("Histogram@1", &inputs_four, &context) == TASK_OK;
    check(ok && same_file("h1.csv", "h4.csv"),
          "Histogram: same result with 1 and 4 threads");
}

static void test_cpu_deterministic(void) {
    task_context_t context = make_context(NULL);
    context.vars = array_init(4);
    void *slots[2];
    par_var_t vars[] = { {"iterations", "2000000"}, {"seed", "42"} };
    array_t inputs = make_inputs(vars, 2, slots);

    bool ok = task_run("Cpu@1", &inputs, &context) == TASK_OK;
    char first[32] = "";
    if (vars_get(context.vars, "CPU_RESULT") != NULL)
        snprintf(first, sizeof first, "%s", vars_get(context.vars, "CPU_RESULT"));
    ok = ok && task_run("Cpu@1", &inputs, &context) == TASK_OK;
    const char *second = vars_get(context.vars, "CPU_RESULT");
    check(ok && first[0] != '\0' && second != NULL && strcmp(first, second) == 0,
          "Cpu: same seed gives same CPU_RESULT");
    vars_free(context.vars);
}

static void test_invalid_input(void) {
    task_context_t context = make_context(NULL);
    void *slots[1];
    par_var_t vars[] = { {"seconds", "abc"} };
    array_t inputs = make_inputs(vars, 1, slots);
    check(task_run("Sleep@1", &inputs, &context) == TASK_FAILED, "Invalid input fails");
}

static void test_unknown_task(void) {
    task_context_t context = make_context(NULL);
    void *slots[1];
    par_var_t vars[] = { {"seconds", "1"} };
    array_t inputs = make_inputs(vars, 1, slots);
    check(task_run("Foo@1", &inputs, &context) == TASK_FAILED, "Unknown task fails");
}

static void test_sleep_cancel(void) {
    int cancel = 1;  // already canceled
    task_context_t context = make_context(&cancel);
    void *slots[1];
    par_var_t vars[] = { {"seconds", "10"} };
    array_t inputs = make_inputs(vars, 1, slots);

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    task_result_t result = task_run("Sleep@1", &inputs, &context);
    check(result == TASK_CANCELED && seconds_since(start) < 1.0, "Sleep stops when canceled");
}

static void test_sleep_timeout(void) {
    task_context_t context = make_context(NULL);
    clock_gettime(CLOCK_MONOTONIC, &context.deadline);
    context.deadline.tv_sec += 1;  // 1 s limit for 10 s sleep to ensure timout
    void *slots[1];
    par_var_t vars[] = { {"seconds", "10"} };
    array_t inputs = make_inputs(vars, 1, slots);

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    task_result_t result = task_run("Sleep@1", &inputs, &context);
    check(result == TASK_TIMEOUT && seconds_since(start) < 2.0, "Sleep stops on timeout");
}

//*** Main  ***//

int main(void) {
    if (mkdtemp(workdir) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    make_corpus();

    test_wordcount_deterministic();
    test_histogram_deterministic();
    test_cpu_deterministic();
    test_invalid_input();
    test_unknown_task();
    test_sleep_cancel();
    test_sleep_timeout();

    printf("%d of %d tests passed\n", passed, total);

    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf %s", workdir);  // clean up
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", workdir);

    return passed == total ? 0 : 1;
}