#pragma once

// Built-in tasks: Sleep@1, Cpu@1, WordCount@1, Histogram@1

#include <stdbool.h>
#include <time.h>

#include "array.h"
#include "pipeline.h"
#include "joblog.h"

// Task context passed to each task by the job executor
typedef struct {
    const char     *workdir;   // job workspace
    const int      *cancel;    // 1 = canceled (read with omp atomic read)
    struct timespec deadline;  // CLOCK_MONOTONIC, {0,0} = no limit
    array_t        *vars;      // job variables
    job_log_t *log;          // job log
} task_context_t;

typedef enum {
    TASK_OK = 0,
    TASK_FAILED,    // bad inputs, file error, out of memory
    TASK_CANCELED,  // Cancelation was requested
    TASK_TIMEOUT    // deadline expired
} task_result_t;

// Runs a task. inputs: array_t of par_var_t*.
task_result_t task_run(const char *name, const array_t *inputs,
                       task_context_t *context);

// True if the task need to stop (TASK_CANCELED or TASK_TIMEOUT)
bool check_task_stop(const task_context_t *context, task_result_t *why);