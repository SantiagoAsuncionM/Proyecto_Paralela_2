#pragma once

#include <omp.h>
#include <sys/types.h>
#include <time.h>

#include "array.h"
#include "pipeline.h"

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

//Job node been executed
typedef struct job_node {
    const job_t    *def;          //YAML definition
    int             stage;        //Stage index

    int            *pred;         //Predecessors: Indexes of jobs this job depends on
    int             n_pred;       //predecessor jobs count
    int            *succ;         //Succesors: Indexes of jobs that depends on this job
    int             n_succ;       //Succesor jobs count  

    int             pending;      //Predecessors pending count
    exec_state_t    state;        //Job current state  
    struct timespec t_start, t_end;

    step_exec_t    *steps;        //One per step
    job_log_t       log;          //Job log
    array_t        *out_vars;     //Variables set when job ended
    pid_t           current_pid;  //Child process ID running, 0 if none
} job_node_t;