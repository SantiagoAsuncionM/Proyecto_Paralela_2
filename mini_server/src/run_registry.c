#include "../includes/run_registry.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RUNS 100

run_t runs[MAX_RUNS];
int total_runs = 0;
int next_id = 1;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


//busca si un id existe
run_t* search_Run(int id) {
    for (int i = 0; i < total_runs; i++) {
        if (runs[i].id == id)
            return &runs[i];
    }
    return NULL;
}

//guarda una run
int run_add(char* yaml) {
    pthread_mutex_lock(&mutex);
    if (total_runs == MAX_RUNS) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    int id = next_id++;
    runs[total_runs].id = id;
    runs[total_runs].status = "Queued";
    runs[total_runs].yaml = yaml;     
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
    snprintf(buf, cap, "OK %d %s\n", run->id, run->status);
    pthread_mutex_unlock(&mutex);
    return 0;
}

int run_list(char* buf, size_t cap) {
    if (total_runs == 0)
        snprintf(buf, cap, "sin ejecuciones\n");
    return 0;
}

int run_cancel(int id) {
    pthread_mutex_lock(&mutex);
    run_t* run = search_Run(id);
    if (run == NULL) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    run->status = "Canceled";
    pthread_mutex_unlock(&mutex);
    return 0;
}

/*int logs() {
    return 0;
}
*/    

void run_registry_destroy(void) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < total_runs; i++) {
        free(runs[i].yaml);
    }
    total_runs = 0;
    pthread_mutex_unlock(&mutex);
}