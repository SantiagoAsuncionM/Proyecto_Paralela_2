/* Integration tests of the executor with the real parser, graph and agents.

Tests:
  1. Full pipeline: two stages with scripts, artifacts, variables,
     WordCount, continueOnError and a failed() condition.
  2. Cancel stops a running Sleep@1 and the run ends canceled.
  3. timeoutInSeconds stops a script and the job fails.
  4. Fan-in: 50 jobs feed 1 final job, 10 times; the final job runs once.

Build (from the repo root, one command):
   gcc -std=c2x -Wall -Wextra -pedantic -O2 -pthread -fopenmp -Iinclude
       tests/test_executor.c src/executor.c src/scheduler.c src/ready_queue.c
       src/process.c src/joblog.c src/vars.c src/tasks.c src/run.c src/graph.c
       src/array.c src/parser.c src/parser_helpers.c src/validate_pipeline.c
       -lyaml -o bin/test_executor

Run: ./bin/test_executor (exit code 0 only if all pass)
*/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "executor.h"
#include "graph.h"
#include "parser.h"
#include "validate_pipeline.h"

static int passed = 0, total = 0;
static char workdir[] = "/tmp/test_executor_XXXXXX";

static void check(bool ok, const char *name) {
    total++;
    if (ok) passed++;
    printf("%s  %s\n", ok ? "PASA" : "FALLA", name);
}

// Writes the YAML text to a file, parses it and builds the run.
static run_t *load_run(const char *yaml) {
    char path[512];
    snprintf(path, sizeof path, "%s/pipeline.yaml", workdir);
    FILE *file = fopen(path, "w");
    fputs(yaml, file);
    fclose(file);

    array_t *errors = NULL;
    pipeline_t *pipeline = load_pipeline(path, &errors);
    if (pipeline != NULL) validate_names_and_refs(pipeline, errors);
    if (pipeline == NULL || errors->num_items > 0) {
        printf("      errores del YAML: %zu\n", errors != NULL ? errors->num_items : 0);
        pipeline_destroy(pipeline);
        array_destroy(errors);
        return NULL;
    }
    dep_graph_t *graph = graph_build(pipeline);
    if (graph == NULL || graph_check_cycles(graph, errors)) {
        printf("      error del grafo\n");
        graph_destroy(graph);
        pipeline_destroy(pipeline);
        array_destroy(errors);
        return NULL;
    }
    array_destroy(errors);
    return run_create(pipeline, graph);
}


static void run_and_wait(scheduler_t *scheduler, run_t *run, const char *name) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", workdir, name);
    executor_submit_run(scheduler, run, path);
    for (int i = 0; i < 3000 && !executor_run_finished(run); i++) {
        struct timespec pause = { 0, 10 * 1000 * 1000 };
        nanosleep(&pause, NULL);
    }
}


static int times_logged(job_log_t *log, const char *text) {
    int count = 0;
    for (size_t i = 0; i < log->count; i++)
        if (strcmp(log->lines[i], text) == 0) count++;
    return count;
}

static exec_state_t job_state(run_t *run, const char *name) {
    for (int i = 0; i < run->n_jobs; i++)
        if (strcmp(run->jobs[i].node->def->name, name) == 0) return run->jobs[i].state;
    return -1;
}

static const char *FIGURE1 =
    "name: figure1\n"
    "variables:\n"
    "  TOP: 3\n"
    "stages:\n"
    "- stage: prepare\n"
    "  jobs:\n"
    "  - job: make\n"
    "    steps:\n"
    "    - script: |\n"
    "        echo hola mundo hola omp > corpus.txt\n"
    "        echo \"##setvariable WORDS=4\"\n"
    "    - publish: corpus.txt\n"
    "      artifact: corpus\n"
    "  - job: wait\n"
    "    steps:\n"
    "    - task: Sleep@1\n"
    "      inputs:\n"
    "        seconds: 1\n"
    "- stage: analyze\n"
    "  jobs:\n"
    "  - job: count\n"
    "    steps:\n"
    "    - download: corpus\n"
    "      path: data\n"
    "    - task: WordCount@1\n"
    "      inputs:\n"
    "        file: data/corpus.txt\n"
    "        threads: 2\n"
    "        output: count.txt\n"
    "        top: $(TOP)\n"
    "    - script: test \"$(WORDS)\" = 4 && grep -q total,4 count.txt\n"
    "  - job: flaky\n"
    "    steps:\n"
    "    - script: exit 1\n"
    "      continueOnError: true\n"
    "  - job: alarm\n"
    "    dependsOn: count\n"
    "    condition: failed()\n"
    "    steps:\n"
    "    - script: echo should not run\n";

static void test_figure1(scheduler_t *scheduler) {
    run_t *run = load_run(FIGURE1);
    bool ok = run != NULL;
    if (ok) {
        run_and_wait(scheduler, run, "figure1");
        ok = job_state(run, "make") == STATE_SUCCEEDED &&
             job_state(run, "wait") == STATE_SUCCEEDED &&
             job_state(run, "count") == STATE_SUCCEEDED &&
             job_state(run, "flaky") == STATE_PARTIALLY_SUCCEEDED &&
             job_state(run, "alarm") == STATE_SKIPPED &&
             run->state == STATE_PARTIALLY_SUCCEEDED;
    }
    check(ok, "Pipeline completo: etapas, artefactos, variables y condiciones");
    run_destroy(run);
}

static void test_cancel(scheduler_t *scheduler) {
    run_t *run = load_run(
        "name: cancel\nstages:\n- stage: one\n  jobs:\n  - job: long\n    steps:\n"
        "    - task: Sleep@1\n      inputs:\n        seconds: 10\n");
    bool ok = run != NULL;
    if (ok) {
        char path[512];
        snprintf(path, sizeof path, "%s/cancel", workdir);
        executor_submit_run(scheduler, run, path);
        struct timespec pause = { 0, 300 * 1000 * 1000 };
        nanosleep(&pause, NULL);
        executor_cancel_run(run);
        for (int i = 0; i < 200 && !executor_run_finished(run); i++) {
            struct timespec tick = { 0, 10 * 1000 * 1000 };
            nanosleep(&tick, NULL);
        }
        ok = run->state == STATE_CANCELED && job_state(run, "long") == STATE_CANCELED;
    }
    check(ok, "Cancelar detiene un job en curso en menos de 2 s");
    run_destroy(run);
}

static void test_timeout(scheduler_t *scheduler) {
    run_t *run = load_run(
        "name: timeout\nstages:\n- stage: one\n  jobs:\n  - job: slow\n"
        "    timeoutInSeconds: 1\n    steps:\n    - script: sleep 10\n");
    bool ok = run != NULL;
    if (ok) {
        run_and_wait(scheduler, run, "timeout");
        ok = job_state(run, "slow") == STATE_FAILED && run->state == STATE_FAILED;
    }
    check(ok, "El timeout detiene el script y el job falla");
    run_destroy(run);
}

// 50 jobs in stage one, one job in stage two that depends on all of them.
static void test_fan_in(scheduler_t *scheduler) {
    char *yaml = malloc(20000);
    int length = sprintf(yaml, "name: fanin\nstages:\n- stage: many\n  jobs:\n");
    for (int i = 0; i < 50; i++)
        length += sprintf(yaml + length,
                          "  - job: j%d\n    steps:\n    - task: Cpu@1\n      inputs:\n"
                          "        iterations: 1000\n        seed: %d\n", i, i);
    sprintf(yaml + length, "- stage: last\n  jobs:\n  - job: end\n    steps:\n"
                           "    - script: echo done\n");

    bool ok = true;
    for (int round = 0; round < 10 && ok; round++) {
        run_t *run = load_run(yaml);
        if (run == NULL) {
            ok = false;
            break;
        }
        char name[32];
        snprintf(name, sizeof name, "fanin%d", round);
        run_and_wait(scheduler, run, name);
        job_exec_t *end = &run->jobs[run->n_jobs - 1];
        ok = run->state == STATE_SUCCEEDED && times_logged(&end->log, "done") == 1;
        run_destroy(run);
    }
    free(yaml);
    check(ok, "Fan-in 50 -> 1 (10 rondas): el último job corre una sola vez");
}

int main(void) {
    if (mkdtemp(workdir) == NULL) return 1;
    scheduler_t scheduler;
    if (!scheduler_start(&scheduler, 4)) return 1;

    test_figure1(&scheduler);
    test_cancel(&scheduler);
    test_timeout(&scheduler);
    test_fan_in(&scheduler);

    scheduler_shutdown(&scheduler);
    printf("%d de %d pruebas pasaron\n", passed, total);

    char command[600];
    snprintf(command, sizeof command, "rm -rf %s", workdir);
    if (system(command) != 0) fprintf(stderr, "no se pudo borrar %s\n", workdir);
    return passed == total ? 0 : 1;
}