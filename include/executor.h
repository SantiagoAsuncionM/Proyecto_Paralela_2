#pragma once

// Executor: runs one job. Called by each agent.

#include "run.h"
#include "scheduler.h"

// Runs the steps of a job and enqueues its ready successors.
void executor_run_job(scheduler_t *scheduler, void *job);

// Run submit: starts run, creates folders and logs and enqueues jobs with no pending dependencies. 
bool executor_submit_run(scheduler_t *scheduler, run_t *run, const char *workdir);

// Cancels a run: running steps stop and pending jobs never run.
void executor_cancel_run(run_t *run);

// True when the run is in a final state.
bool executor_run_finished(run_t *run);