#define _POSIX_C_SOURCE 200809L
#include "run_registry.h"
#include "executor.h"
#include "run.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_RUNS 1000

static run_t* runs[MAX_RUNS];
int total_runs = 0;
int next_id = 1;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


static const char* state_text(exec_state_t state){

  switch(state){
     case STATE_QUEUED: return "queued";
        case STATE_RUNNING: return "running";
        case STATE_SUCCEEDED: return "succeeded";
        case STATE_PARTIALLY_SUCCEEDED: return "partiallySucceeded";
        case STATE_FAILED: return "failed";
        case STATE_CANCELED: return "canceled";
        case STATE_SKIPPED: return "skipped";
  }

  return "Estado desconocido";
}

//busca si un id existe
run_t* search_Run(int id) {
    for (int i = 0; i < total_runs; i++) {
        if (runs[i]->id == id)
            return runs[i];
    }
    return NULL;
}

//guarda una run
int run_add(run_t* run) {
    pthread_mutex_lock(&mutex);

    if (total_runs == MAX_RUNS) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    int id = next_id++;
    run->id = id;
    runs[total_runs] = run;     
    total_runs++;
    pthread_mutex_unlock(&mutex);
    return id;
}

//imprime el estado de una run
int run_status(int id, char* buf, size_t cap) {
    pthread_mutex_lock(&mutex);
    run_t* run = search_Run(id);
    if (run == NULL) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    omp_set_lock(&run->lock);
    exec_state_t state = run->state;
    omp_unset_lock(&run->lock);

    snprintf(buf, cap, "OK %d %s\n", run->id, state_text(state));
    pthread_mutex_unlock(&mutex);
    return 0;
}

int run_list(char* buf, size_t cap){
    pthread_mutex_lock(&mutex);
    if (total_runs == 0){

        snprintf(buf, cap, "Sin ejecuciones registradas\n");
        pthread_mutex_unlock(&mutex);
        return 0;

    }
    else{

        size_t regs_printed = 0;

        for(int i=1; i<=total_runs; i++){

           run_t* actual_run = search_Run(i);

           if(actual_run != NULL && regs_printed < cap){

            omp_set_lock(&actual_run->lock);
            exec_state_t state = actual_run->state;
            omp_unset_lock(&actual_run->lock);

            int n = snprintf(buf + regs_printed, cap - regs_printed, "OK %d %s\n", actual_run->id, state_text(state));

             if (n > 0){
                 regs_printed +=  (size_t)n;
             }     
           }

        }
        pthread_mutex_unlock(&mutex);
        return 0;
    }    
    
}

int run_cancel(int id) {
    pthread_mutex_lock(&mutex);
    run_t* run = search_Run(id);
    if (run == NULL) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }
   
    executor_cancel_run(run);
    pthread_mutex_unlock(&mutex);
    return 0;
}

int run_logs(int id, const char* job, char* buf, size_t cap) {

    pthread_mutex_lock(&mutex);
    run_t* run = search_Run(id);
    if (run == NULL) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    size_t used = 0;
    int found = 0;
    buf[0] = '\0';

    for (int i = 0; i < run->n_jobs; i++) {
        job_exec_t* actual_job = &run->jobs[i];

        const char* job_name = actual_job->node->def->name;
        stage_t* stage = run->def->stages->items[actual_job->node->stage];
        const char* stage_name = stage->name;

        if (job != NULL && strcmp(job, job_name) != 0) {
            continue;
        }
        found = 1;

        if (used < cap) {
            int n = snprintf(buf + used, cap - used, "%s.%s: \n", stage_name, job_name);
            if (n > 0) {
                used = used + (size_t)n;
            }
        }

        if (used < cap) {
            joblog_read(&actual_job->log, 0, buf + used, cap - used, NULL);
            used = used + strlen(buf + used);
        }
    }

    pthread_mutex_unlock(&mutex);

    if (!found) {
        return -2;
    }
    return 0;
}


void run_registry_destroy(void) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < total_runs; i++) {
        run_destroy(runs[i]);
    }
    total_runs = 0;
    pthread_mutex_unlock(&mutex);
}

// cuantos trabajos tiene la corrida
int run_job_count(int id) {
    pthread_mutex_lock(&mutex);
    run_t* run = search_Run(id);
    int count = -1;
    if (run != NULL) {
        count = run->n_jobs;
    }
    pthread_mutex_unlock(&mutex);
    return count;
}


int run_logs_follow(int id, const char* job, size_t* positions,char* buf, size_t cap, bool* finished){
    
    pthread_mutex_lock(&mutex);

    run_t* run = search_Run(id);
    if (run == NULL) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    size_t used = 0;
    int found = 0;
    bool all_closed = true;
    buf[0] = '\0';

    for (int i = 0; i < run->n_jobs; i++) {

        job_exec_t* actual_job = &run->jobs[i];
        const char* job_name = actual_job->node->def->name;
        stage_t* stage = run->def->stages->items[actual_job->node->stage];

        if (job != NULL && strcmp(job, job_name) != 0) {
            continue;
        }
        found = 1;

        char lines[4096];
        bool closed = false;
        size_t next = joblog_read(&actual_job->log, positions[i], lines, sizeof lines, &closed);
        if (!closed) {
            all_closed = false;
        }

        if (next > positions[i]) {
            char header[256];
            snprintf(header, sizeof header, "%s.%s:\n", stage->name, job_name);

            if (used + strlen(header) + strlen(lines) + 1 > cap) {
                all_closed = false;
                break;
            }
            strcpy(buf + used, header);
            used = used + strlen(header);
            strcpy(buf + used, lines);
            used = used + strlen(lines);

            positions[i] = next;
        }
    }

    pthread_mutex_unlock(&mutex);

    *finished = all_closed;

    if (!found) {
        return -2;
    }
    return 0;
}