#pragma once
#include <stdlib.h>
#include <pthread.h>

typedef struct {
    void** items;
    size_t num_items;
    size_t capacity;
    // pthread_mutex_t mutex;
    int next_id;
} array_t;

array_t* array_init(int initial_capacity);

void array_destroy(array_t* array);

int array_add(array_t* array, void* new_item);

void* array_search(array_t* array, long id);
