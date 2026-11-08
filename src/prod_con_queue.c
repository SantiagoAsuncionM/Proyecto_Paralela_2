#include <stdlib.h>
#include <stdio.h>

#include "prod_con_queue.h"

int pc_queue_init(pc_queue_t* q, size_t capacity) {
    if (q == NULL || capacity == 0) {
        return PC_ERROR;
    }
    
    q->items = calloc(capacity, sizeof(*q->items));
    if (q->items == NULL) {
        return PC_ERROR;
    }

    q->capacity = capacity;
    q->head = q->tail = q->count = 0;
    q->closed = 0;
    
    pthread_mutex_init(&q->mutex, NULL);
    sem_init(&q->items_sem, 0, 0);
    sem_init(&q->spaces_sem, 0, (unsigned)capacity);

    return PC_OK;
}

int insert(pc_queue_t* q, void* item) {
    pthread_mutex_lock(&q->mutex);
    if (q->closed) {
        pthread_mutex_unlock(&q->mutex);
        sem_post(&q->spaces_sem);
        return PC_CLOSED;
    }
    q->items[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_mutex_unlock(&q->mutex);

    sem_post(&q->items_sem);
    return PC_OK;
}

// todo: probar el bloqueo despues del shutdown
int pc_queue_try_push(pc_queue_t* q, void* item) {
    if (sem_trywait(&q->spaces_sem) != 0) {
        return PC_FULL;
    }
    return insert(q, item);
}

int pc_queue_push(pc_queue_t* q, void* item) {
    while (sem_wait(&q->spaces_sem) != 0) {
    }
    return insert(q, item);
}

int pc_queue_item_idx(pc_queue_t* q, void* item) {
    pthread_mutex_lock(&q->mutex);
    for (size_t i = 0; i < q->capacity; ++i) {
        if (q->items[i] == item) {
            pthread_mutex_unlock(&q->mutex);
            return (int)i;
        }
    }
    pthread_mutex_unlock(&q->mutex);
    return -1;
}

void* pc_queue_pop(pc_queue_t* q) {
    while (sem_wait(&q->items_sem) != 0) {
    }
    pthread_mutex_lock(&q->mutex);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        sem_post(&q->items_sem);
        return NULL;
    }
    void *item = q->items[q->head];
    q->items[q->head] = NULL;
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_mutex_unlock(&q->mutex);

    sem_post(&q->spaces_sem);
    return item;
}

size_t pc_queue_count(pc_queue_t* queue) {
    pthread_mutex_lock(&queue->mutex);
    size_t n = queue->count;
    pthread_mutex_unlock(&queue->mutex);
    return n;
}

void pc_queue_close(pc_queue_t* q) {
    pthread_mutex_lock(&q->mutex);
    q->closed = 1;
    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->items_sem);
    sem_post(&q->spaces_sem);
}

void pc_queue_destroy(pc_queue_t* q) {
    sem_destroy(&q->spaces_sem);
    sem_destroy(&q->items_sem);
    pthread_mutex_destroy(&q->mutex);
    free(q->items);
    q->items = NULL;
}
