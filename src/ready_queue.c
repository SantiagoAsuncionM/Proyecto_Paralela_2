#define _POSIX_C_SOURCE 200809L
#include "ready_queue.h"

#include <stdlib.h>
#include <time.h>

void ready_queue_init(ready_queue_t *queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->closed = false;
    omp_init_lock(&queue->lock);
}

void ready_queue_destroy(ready_queue_t *queue) {
    ready_node_t *node = queue->head;
    while (node != NULL) {
        ready_node_t *next = node->next;
        free(node);
        node = next;
    }
    queue->head = NULL;
    queue->tail = NULL;
    omp_destroy_lock(&queue->lock);
}

bool ready_queue_push(ready_queue_t *queue, void *job) {
    ready_node_t *node = malloc(sizeof *node);
    if (node == NULL) return false;
    node->job = job;
    node->next = NULL;

    omp_set_lock(&queue->lock);
    if (queue->closed) {
        omp_unset_lock(&queue->lock);
        free(node);
        return false;
    }
    if (queue->tail != NULL) queue->tail->next = node;
    else queue->head = node;
    queue->tail = node;
    omp_unset_lock(&queue->lock);
    return true;
}

// An empty queue is polled every 1 ms (OpenMP has no condition variables).
void *ready_queue_pop(ready_queue_t *queue) {
    while (true) {
        omp_set_lock(&queue->lock);
        if (queue->closed) {
            omp_unset_lock(&queue->lock);
            return NULL;
        }
        ready_node_t *node = queue->head;
        if (node != NULL) {
            queue->head = node->next;
            if (queue->head == NULL) queue->tail = NULL;
        }
        omp_unset_lock(&queue->lock);

        if (node != NULL) {
            void *job = node->job;
            free(node);
            return job;
        }
        struct timespec pause = { 0, 1000 * 1000 };  // 1 ms
        nanosleep(&pause, NULL);
    }
}

void ready_queue_close(ready_queue_t *queue) {
    omp_set_lock(&queue->lock);
    queue->closed = true;
    omp_unset_lock(&queue->lock);
}