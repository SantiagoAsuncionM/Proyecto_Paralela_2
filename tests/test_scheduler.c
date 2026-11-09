/* Test unit for scheduler (src/scheduler.c).

Tests:
  1. Starts with 4 agents, proper shutdown.
  2. Rejects start with 0 agents.
  3. Enqueue fails after shutdown.

Build (from the repo root, one command):
  gcc -std=c2x -Wall -Wextra -pedantic -O2 -pthread -fopenmp -Iinclude
      tests/test_scheduler.c src/scheduler.c src/ready_queue.c src/executor.c
      src/vars.c src/tasks.c src/array.c -o bin/test_scheduler

Run: ./bin/test_scheduler (exit code 0 only if all pass)
*/

#include "scheduler.h"
#include <stdio.h>

static int passed = 0, total = 0;

static void check(bool ok, const char *name) {
    total++;
    if (ok) passed++;
    printf("%s  %s\n", ok ? "PASA" : "FALLA", name);
}

static void test_start_and_shutdown(void) {
    scheduler_t scheduler;
    bool ok = scheduler_start(&scheduler, 4);
    if (ok) scheduler_shutdown(&scheduler);
    check(ok, "Arranca con 4 agentes y se apaga");
}

static void test_zero_agents(void) {
    scheduler_t scheduler;
    check(!scheduler_start(&scheduler, 0), "Rechaza 0 agentes");
}

static void test_enqueue_after_shutdown(void) {
    scheduler_t scheduler;
    int job = 1;
    bool ok = scheduler_start(&scheduler, 2);
    if (ok) {
        ready_queue_close(&scheduler.queue);  // same state as during shutdown
        ok = !scheduler_enqueue(&scheduler, &job);
        scheduler_shutdown(&scheduler);
    }
    check(ok, "Tras apagar, encolar falla");
}

int main(void) {
    test_start_and_shutdown();
    test_zero_agents();
    test_enqueue_after_shutdown();
    printf("%d de %d pruebas pasaron\n", passed, total);
    return passed == total ? 0 : 1;
}