/* Test unit for ready queue (src/ready_queue.c).

Tests:
1. First in, first out.
2. Closed quere handling.
3. Concurrent stress test.

Build (from the repo root, one command):
  gcc -std=c2x -Wall -Wextra -pedantic -O2 -pthread -fopenmp -Iinclude
      tests/test_ready_queue.c src/ready_queue.c -o bin/test_ready_queue

Run: ./bin/test_ready_queue (exit code 0 only if all pass)
*/

#include "ready_queue.h"
#include <omp.h>
#include <stdio.h>

static int passed = 0, total = 0;

static void check(bool ok, const char *name) {
    total++;
    if (ok) passed++;
    printf("%s  %s\n", ok ? "PASA" : "FALLA", name);
}

static void test_fifo_order(void) {
    ready_queue_t queue;
    ready_queue_init(&queue);
    int jobs[3] = { 10, 20, 30 };
    for (int i = 0; i < 3; i++) ready_queue_push(&queue, &jobs[i]);

    bool ok = true;
    for (int i = 0; i < 3; i++) {
        int *job = ready_queue_pop(&queue);
        if (job != &jobs[i]) ok = false;
    }
    ready_queue_destroy(&queue);
    check(ok, "Los jobs salen en orden FIFO");
}

static void test_close(void) {
    ready_queue_t queue;
    ready_queue_init(&queue);
    int job = 1;
    ready_queue_push(&queue, &job);
    ready_queue_close(&queue);

    bool ok = ready_queue_pop(&queue) == NULL && !ready_queue_push(&queue, &job);
    ready_queue_destroy(&queue);
    check(ok, "Al cerrar, pop devuelve NULL y push falla");
}

#define NUM_JOBS 10000

static void test_concurrent(void) {
    ready_queue_t queue;
    ready_queue_init(&queue);
    static int jobs[NUM_JOBS];
    static int times_popped[NUM_JOBS];
    for (int i = 0; i < NUM_JOBS; i++) {
        jobs[i] = i;
        times_popped[i] = 0;
    }

    // 2 threads push, 2 threads pop
    #pragma omp parallel num_threads(4)
    {
        int id = omp_get_thread_num();
        if (id < 2) {
            for (int i = id; i < NUM_JOBS; i += 2) ready_queue_push(&queue, &jobs[i]);
        } else {
            for (int count = 0; count < NUM_JOBS / 2; count++) {
                int *job = ready_queue_pop(&queue);
                #pragma omp atomic update
                times_popped[*job]++;
            }
        }
    }

    bool ok = true;
    for (int i = 0; i < NUM_JOBS; i++)
        if (times_popped[i] != 1) ok = false;
    ready_queue_destroy(&queue);
    check(ok, "Push y pop concurrentes: ningún job se pierde o repite");
}

int main(void) {
    test_fifo_order();
    test_close();
    test_concurrent();
    printf("%d de %d pruebas pasaron\n", passed, total);
    return passed == total ? 0 : 1;
}