#include "scheduler.h"
#include <omp.h>
#include "executor.h"

// Agent loop: takes a job, runs it and repeats until the queue closes.
static void agent_loop(scheduler_t *scheduler) {
    void *job;
    while ((job = ready_queue_pop(&scheduler->queue)) != NULL)
        executor_run_job(scheduler, job);
}

static void *scheduler_main(void *arg) {
    scheduler_t *scheduler = arg;
    omp_set_dynamic(0);            // exactly num_agents threads
    omp_set_max_active_levels(2);  // agents + nested threads of the tasks

    // This thread becomes agent 0; OpenMP creates the others.
    #pragma omp parallel num_threads(scheduler->num_agents)
    agent_loop(scheduler);

    return NULL;  // all agents have finished
}

bool scheduler_start(scheduler_t *scheduler, int num_agents) {
    if (num_agents < 1) return false;
    scheduler->num_agents = num_agents;
    ready_queue_init(&scheduler->queue);
    if (pthread_create(&scheduler->thread, NULL, scheduler_main, scheduler) != 0) {
        ready_queue_destroy(&scheduler->queue);
        return false;
    }
    return true;
}

bool scheduler_enqueue(scheduler_t *scheduler, void *job) {
    return ready_queue_push(&scheduler->queue, job);
}

void scheduler_shutdown(scheduler_t *scheduler) {
    ready_queue_close(&scheduler->queue);
    pthread_join(scheduler->thread, NULL);
    ready_queue_destroy(&scheduler->queue);
}