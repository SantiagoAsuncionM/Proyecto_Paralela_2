#pragma once

// Run: one execution of a pipeline (one submit).

#include <omp.h>
#include <sys/types.h>
#include <time.h>

#include "array.h"
#include "pipeline.h"
#include "graph.h"
#include "joblog.h" 

//Possible states for steps, jobs, stages and runs
typedef enum {
    STATE_QUEUED,
    STATE_RUNNING,
    STATE_SUCCEEDED,
    STATE_PARTIALLY_SUCCEEDED,
    STATE_FAILED,
    STATE_CANCELED,
    STATE_SKIPPED
} exec_state_t;

//Step execution info (state and duration)
typedef struct {
    exec_state_t    state;
    struct timespec t_start, t_end;
} step_exec_t;

// Execution data of one job.
typedef struct {
    struct run   *run;    // job run
    job_node_t   *node;   // job graph node
    exec_state_t  state;
    step_exec_t  *steps;  // one per step
    job_log_t     log;  // joblog
} job_exec_t;

typedef struct run {
    int              id;               
    char            *workdir;          // work dir path
    pipeline_t      *def;              // parsed pipeline
    dep_graph_t     *graph;            // dependencies graph 

    job_exec_t      *jobs;             // jobs exec data
    int              n_jobs;

    int              remaining;        // jobs not finished yet
    int              cancel_requested; // 1 = canceled (omp atomic)
    exec_state_t     state;
    struct timespec  t_start, t_end;

    omp_lock_t       lock;             // protects states and times (read by status)
} run_t;

// Creates the run from a pipeline and its graph. NULL if out of memory.
run_t *run_create(pipeline_t *pipeline, dep_graph_t *graph);

// Frees the run and its graph.
void run_destroy(run_t *run);