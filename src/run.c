#include "run.h"
#include <stdlib.h>
#include "vars.h"

run_t *run_create(pipeline_t *pipeline, dep_graph_t *graph) {
    run_t *run = calloc(1, sizeof *run);
    if (run == NULL) return NULL;
    run->n_jobs = (int)graph->n_nodes;
    run->jobs = calloc((size_t)(run->n_jobs > 0 ? run->n_jobs : 1), sizeof *run->jobs);
    if (run->jobs == NULL) {
        free(run);
        return NULL;
    }
    run->def = pipeline;
    run->graph = graph;
    run->remaining = run->n_jobs;
    run->state = STATE_QUEUED;
    omp_init_lock(&run->lock);


    for (int i = 0; i < run->n_jobs; i++) {
        joblog_init(&run->jobs[i].log);
    }

    for (int i = 0; i < run->n_jobs; i++) {
        job_exec_t *job = &run->jobs[i];
        job->run = run;
        job->node = &graph->nodes[i];
        job->node->pending = job->node->n_pred;
        job->state = STATE_QUEUED;
        job->steps = calloc(job->node->def->steps->num_items + 1, sizeof *job->steps);
        if (job->steps == NULL) {
            run_destroy(run);
            return NULL;
        }
    }
    return run;
}

void run_destroy(run_t *run) {
    if (run == NULL) return;
    for (int i = 0; i < run->n_jobs; i++) {
        job_exec_t *job = &run->jobs[i];
        free(job->steps);
        if (job->node != NULL) vars_free(job->node->out_vars);
        joblog_destroy(&job->log);
    }
    free(run->jobs);
    free(run->workdir);
    graph_destroy(run->graph);
    omp_destroy_lock(&run->lock);
    free(run);
}