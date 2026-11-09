#define _POSIX_C_SOURCE 200809L
#include "executor.h"
#include <errno.h>
#include <limits.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "tasks.h"
#include "vars.h"
#include "process.h"


//*** Helpers  ***//

static void now(struct timespec *time) { clock_gettime(CLOCK_MONOTONIC, time); }

// mkdir -p
static bool make_dirs(const char *path) {
    char partial[PATH_MAX];
    snprintf(partial, sizeof partial, "%s", path);
    for (char *slash = partial + 1; *slash != '\0'; slash++) {
        if (*slash != '/') continue;
        *slash = '\0';
        if (mkdir(partial, 0755) != 0 && errno != EEXIST) return false;
        *slash = '/';
    }
    return mkdir(partial, 0755) == 0 || errno == EEXIST;
}

static const char *stage_name(const run_t *run, const job_exec_t *job) {
    const stage_t *stage = run->def->stages->items[job->node->stage];
    return stage->name;
}

static void job_workdir(const run_t *run, const job_exec_t *job, char *path, size_t size) {
    snprintf(path, size, "%s/jobs/%s.%s", run->workdir, stage_name(run, job), job->node->def->name);
}

// Variables a job starts with
static array_t *initial_vars(const run_t *run, const job_exec_t *job) {
    array_t *vars = array_init(8);
    if (vars == NULL) return NULL;
    // First pipeline vars
    for (size_t i = 0; i < run->def->variables->num_items; i++) {
        const par_var_t *var = run->def->variables->items[i];
        vars_set(vars, var->name, var->value);
    }
    // Then its predecessors vars
    for (int i = 0; i < job->node->n_pred; i++) {
        const array_t *pred_vars = run->jobs[job->node->pred[i]].node->out_vars;
        for (size_t k = 0; pred_vars != NULL && k < pred_vars->num_items; k++) {
            const par_var_t *var = pred_vars->items[k];
            vars_set(vars, var->name, var->value);
        }
    }
    return vars;
}


//*** Artifacts  ***//

static bool artifact_publish(run_t *run, const char *workdir, const char *path,
                             const char *name, job_log_t *log) {
    char folder[PATH_MAX], source[PATH_MAX];
    snprintf(folder, sizeof folder, "%s/artifacts", run->workdir);
    make_dirs(folder);
    snprintf(folder, sizeof folder, "%s/artifacts/%s", run->workdir, name);
    if (mkdir(folder, 0755) != 0) {
        joblog_append(log, "error: el artefacto ya existe o no se puede crear");
        return false;
    }
    snprintf(source, sizeof source, "%s/%s", workdir, path);
    char *argv[] = { "cp", "-r", source, folder, NULL };
    return process_run(argv);
}

static bool artifact_download(run_t *run, const char *workdir, const char *name,
                              const char *path, job_log_t *log) {
    char source[PATH_MAX], target[PATH_MAX];
    snprintf(source, sizeof source, "%s/artifacts/%s/.", run->workdir, name);
    snprintf(target, sizeof target, "%s/%s", workdir, path);
    struct stat info;
    if (stat(source, &info) != 0) {
        joblog_append(log, "error: el artefacto no existe");
        return false;
    }
    make_dirs(target);
    char *argv[] = { "cp", "-r", source, target, NULL };
    return process_run(argv);
}

//*** Steps  ***//

static task_result_t run_step(run_t *run, const step_t *step,
                              task_context_t *context) {
    task_result_t result = TASK_FAILED;
    if (step->type == STEP_SCRIPT) {
        char *script = vars_substitute(context->vars, step->script);
        if (script != NULL) result = process_run_script(script, context);
        free(script);
    } else if (step->type == STEP_TASK) {
        array_t *inputs = array_init(8);  // inputs with $(VAR) replaced
        for (size_t i = 0; inputs != NULL && i < step->inputs->num_items; i++) {
            const par_var_t *input = step->inputs->items[i];
            char *value = vars_substitute(context->vars, input->value);
            if (value != NULL) vars_set(inputs, input->name, value);
            free(value);
        }
        if (inputs != NULL) result = task_run(step->task, inputs, context);
        vars_free(inputs);
    } else if (step->type == STEP_PUBLISH) {
        result = artifact_publish(run, context->workdir, step->publish_path,
                                  step->artifact, context->log) ? TASK_OK : TASK_FAILED;
    } else if (step->type == STEP_DOWNLOAD) {
        result = artifact_download(run, context->workdir, step->artifact,
                                   step->download_path, context->log) ? TASK_OK : TASK_FAILED;
    }
    return result;
}

static void set_step_state(run_t *run, step_exec_t *step, exec_state_t state, bool start) {
    omp_set_lock(&run->lock);
    step->state = state;
    if (start) now(&step->t_start);
    else now(&step->t_end);
    omp_unset_lock(&run->lock);
}

//*** Graph progress  ***//

static void finish_job(scheduler_t *scheduler, job_exec_t *job, exec_state_t state);

// True if the condition holds for the predecessors final states.
static bool condition_holds(condition_t condition, const run_t *run, const job_exec_t *job) {
    bool all_ok = true, any_failed = false;
    for (int i = 0; i < job->node->n_pred; i++) {
        exec_state_t state = run->jobs[job->node->pred[i]].state;
        if (state != STATE_SUCCEEDED && state != STATE_PARTIALLY_SUCCEEDED) all_ok = false;
        if (state == STATE_FAILED) any_failed = true;
    }
    if (condition == COND_ALWAYS) return true;
    if (condition == COND_FAILED) return any_failed;
    return all_ok;
}

// Job with no predecessors processing
static void release_job(scheduler_t *scheduler, job_exec_t *job) {
    run_t *run = job->run;
    int canceled;
    #pragma omp atomic read
    canceled = run->cancel_requested;

    omp_set_lock(&run->lock);  // Predecessor states read under lock
    const stage_t *stage = run->def->stages->items[job->node->stage];
    bool holds = condition_holds(stage->condition, run, job) &&
                 condition_holds(job->node->def->condition, run, job);
    omp_unset_lock(&run->lock);

    if (canceled) finish_job(scheduler, job, STATE_CANCELED);
    else if (!holds) finish_job(scheduler, job, STATE_SKIPPED);
    else if (!scheduler_enqueue(scheduler, job)) finish_job(scheduler, job, STATE_CANCELED);
}

// Final state of the run
static void finish_run(run_t *run) {
    omp_set_lock(&run->lock);
    exec_state_t state = STATE_SUCCEEDED;
    for (int i = 0; i < run->n_jobs; i++) {
        if (run->jobs[i].state == STATE_FAILED) state = STATE_FAILED;
        if (run->jobs[i].state == STATE_PARTIALLY_SUCCEEDED && state == STATE_SUCCEEDED)
            state = STATE_PARTIALLY_SUCCEEDED;
    }
    if (run->cancel_requested) state = STATE_CANCELED;
    run->state = state;
    now(&run->t_end);
    omp_unset_lock(&run->lock);
}

// Saves the final state, releases the successors with no pendings.
static void finish_job(scheduler_t *scheduler, job_exec_t *job, exec_state_t state) {
    run_t *run = job->run;
    if (job->node->out_vars == NULL) job->node->out_vars = initial_vars(run, job);  // skipped jobs pass variables on
    joblog_close(&job->log);

    omp_set_lock(&run->lock);
    job->state = state;
    now(&job->node->t_end);
    omp_unset_lock(&run->lock);

    for (int i = 0; i < job->node->n_succ; i++) {
        job_exec_t *next = &run->jobs[job->node->succ[i]];
        int left;
        #pragma omp atomic capture
        left = --next->node->pending;
        if (left == 0) release_job(scheduler, next);
    }

    int remaining;
    #pragma omp atomic capture
    remaining = --run->remaining;
    if (remaining == 0) finish_run(run);
}

//*** Public functions  ***//

bool executor_submit_run(scheduler_t *scheduler, run_t *run, const char *workdir) {
    run->workdir = strdup(workdir);
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/logs", workdir);
    if (run->workdir == NULL || !make_dirs(path)) return false;


    for(int i=0; i<run->n_jobs; i++){
       
        job_exec_t* job = &run->jobs[i];
        char log_path[PATH_MAX];
        snprintf(log_path, sizeof log_path,"%s/logs/%s.%s.log", workdir, stage_name(run,job), job->node->def->name);
        joblog_open_file(&job->log, log_path);
    }

    omp_set_lock(&run->lock);
    run->state = STATE_RUNNING;
    now(&run->t_start);
    omp_unset_lock(&run->lock);
    if (run->n_jobs == 0) {
        finish_run(run);
        return true;
    }

    int *ready = malloc((size_t)run->n_jobs * sizeof *ready);
    if (ready == NULL) return false;
    int count = 0;
    for (int i = 0; i < run->n_jobs; i++)
        if (run->jobs[i].node->pending == 0) ready[count++] = i;
    for (int i = 0; i < count; i++) release_job(scheduler, &run->jobs[ready[i]]);
    free(ready);
    return true;
}

void executor_run_job(scheduler_t *scheduler, void *job_pointer) {
    job_exec_t *job = job_pointer;
    run_t *run = job->run;

    int canceled;
    #pragma omp atomic read
    canceled = run->cancel_requested;
    if (canceled) {
        finish_job(scheduler, job, STATE_CANCELED);
        return;
    }

    omp_set_lock(&run->lock);
    job->state = STATE_RUNNING;
    now(&job->node->t_start);
    omp_unset_lock(&run->lock);

    char workdir[PATH_MAX];
    job_workdir(run, job, workdir, sizeof workdir);
    make_dirs(workdir);
    job->node->out_vars = initial_vars(run, job);

    task_context_t context = {
        .workdir = workdir,
        .cancel = &run->cancel_requested,
        .vars = job->node->out_vars,
        .log = &job->log,
    };
    if (job->node->def->timeout_in_seconds > 0) {
        now(&context.deadline);
        context.deadline.tv_sec += (time_t)job->node->def->timeout_in_seconds;
    }

    // Steps in order. Fail without continueOnError skips the rest.
    bool failed = false, partial = false;
    task_result_t stop = TASK_OK;  // canceled or timed out
    for (size_t i = 0; i < job->node->def->steps->num_items; i++) {
        const step_t *step = job->node->def->steps->items[i];
        if (failed || stop != TASK_OK) {
            set_step_state(run, &job->steps[i],
                           stop == TASK_CANCELED ? STATE_CANCELED : STATE_SKIPPED, false);
            continue;
        }
        set_step_state(run, &job->steps[i], STATE_RUNNING, true);

        char line[256];
        snprintf(line, sizeof line, "paso %zu: inicio", i+1);
        joblog_append(&job->log, line);

        task_result_t result = context.vars != NULL ? run_step(run, step, &context)
                                                    : TASK_FAILED;

        const char* text;
        if(result == TASK_OK){
            text = "ok";

        }
        else if(result == TASK_CANCELED){
            text = "cancelado";
        }
        else{
            text = "fallo";
        }                 
        snprintf(line, sizeof line, "paso %zu: %s", i+1, text);
        joblog_append(&job->log, line);
        
        
        if (result == TASK_CANCELED || result == TASK_TIMEOUT) stop = result;
        else if (result == TASK_FAILED && step->continue_on_error) partial = true;
        else if (result == TASK_FAILED) failed = true;
        set_step_state(run, &job->steps[i], result == TASK_OK ? STATE_SUCCEEDED
                       : result == TASK_CANCELED ? STATE_CANCELED : STATE_FAILED, false);
    }

    exec_state_t state = STATE_SUCCEEDED;
    if (stop == TASK_CANCELED) state = STATE_CANCELED;
    else if (failed || stop == TASK_TIMEOUT) state = STATE_FAILED;
    else if (partial) state = STATE_PARTIALLY_SUCCEEDED;
    finish_job(scheduler, job, state);
}

void executor_cancel_run(run_t *run) {
    #pragma omp atomic write
    run->cancel_requested = 1;
}

bool executor_run_finished(run_t *run) {
    omp_set_lock(&run->lock);
    bool finished = run->state != STATE_QUEUED && run->state != STATE_RUNNING;
    omp_unset_lock(&run->lock);
    return finished;
}