#pragma once

// Scheduler: a pthread that opens the OpenMP region of agents.
// Each agent takes jobs from the ready queue and runs them.

#include <pthread.h>
#include <stdbool.h>

#include "ready_queue.h"

typedef struct {
    int           num_agents;
    ready_queue_t queue;
    pthread_t     thread;
} scheduler_t;

// Starts the scheduler thread and its agents. False if it cannot start.
bool scheduler_start(scheduler_t *scheduler, int num_agents);

// Adds a ready job. False if the scheduler is shut down.
bool scheduler_enqueue(scheduler_t *scheduler, void *job);

// Closes the queue, waits for the agents to finish their current job and frees the queue.
void scheduler_shutdown(scheduler_t *scheduler);

// TODO: scheduler_submit(): enqueue the jobs of a run with pending = 0 (needs run.h and the graph).
// TODO: mark the jobs left in the queue at shutdown as canceled.