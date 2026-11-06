#include "array.h"

#include <stdlib.h>

array_t* array_init(int initial_capacity) {
    array_t* array = malloc(sizeof(array_t));
    if (array == NULL) {
        return NULL;
    }
    array->items = malloc(sizeof(void*) * initial_capacity);
    if (array->items == NULL) {
        free(array);
        return NULL;
    }
    array->num_items = 0;
    array->capacity = initial_capacity;
    array->next_id = 1;
    
    for (int i = 0; i < initial_capacity; ++i) {
        array->items[i] = NULL;
    }
    return array;
}

void array_destroy(array_t* jobs) {
    if (jobs == NULL) return;
    for (int i = 0; i < jobs->num_items; ++i) {
        free(jobs->items[i]);
    }
    free(jobs);
}

int array_add(array_t* array, void* new_item) {
    if (array->num_items == array->capacity) {
        int new_cap = array->capacity * 2;
        void** p = realloc(array->items, (size_t)new_cap * sizeof *p);
        if (p == NULL) {
            return -1;
        }
        array->items = p;
        array->capacity = new_cap;
    }
    array->items[array->num_items++] = new_item;
    return array->num_items;
}

// job_t* jobs_search(jobs_array* jobs, long id) {
//     job_t* j = NULL;
//     pthread_mutex_lock(&jobs->mutex);
//     if (id >= 1 && id <= jobs->num_jobs) {
//         j = jobs->jobs[id - 1];
//     }
//     pthread_mutex_unlock(&jobs->mutex);
//     return j;
// }
