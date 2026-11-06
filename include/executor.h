#pragma once

// Executor: runs one job. Called by each agent.

#include "scheduler.h"

// Runs the steps of a job and enqueues its ready successors.
void executor_run_job(scheduler_t *scheduler, void *job);