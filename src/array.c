#include "array.h"

#include <stdlib.h>

array_t* array_init(int initial_capacity) {
    array_t* array = calloc(1, sizeof *array);
    if (!array) return NULL;

    array->items = calloc((size_t)initial_capacity, sizeof(void*));
    if (!array->items) {
        free(array);
        return NULL;
    }
    array->capacity = initial_capacity;
    array->next_id = 1;
    return array;
}

void array_destroy(array_t* array) {
    if (array == NULL) {
        return;
    }
    for (int i = 0; i < array->num_items; ++i) {
        free(array->items[i]);
    }
    free(array->items);
    free(array);
}

int array_add(array_t* array, void* new_item) {
    if (!array) return -1;
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
