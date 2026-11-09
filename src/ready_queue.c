#include "ready_queue.h"

#include <stdlib.h>


void ready_queue_init(ready_queue_t *queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->closed = false;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
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
    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->mutex);
}

bool ready_queue_push(ready_queue_t *queue, void *job) {
    ready_node_t *node = malloc(sizeof *node);
    if (node == NULL) return false;
    node->job = job;
    node->next = NULL;

    pthread_mutex_lock(&queue->mutex);
    if (queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        free(node);
        return false;
    }
    if (queue->tail != NULL) queue->tail->next = node;
    else queue->head = node;
    queue->tail = node;
    pthread_cond_signal(&queue->not_empty);  // wake one waiting agent
    pthread_mutex_unlock(&queue->mutex);
    return true;
}

void *ready_queue_pop(ready_queue_t *queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->head == NULL && !queue->closed)
        pthread_cond_wait(&queue->not_empty, &queue->mutex);  // sleeps until signal

    if (queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    ready_node_t *node = queue->head;
    queue->head = node->next;
    if (queue->head == NULL) queue->tail = NULL;
    pthread_mutex_unlock(&queue->mutex);

    void *job = node->job;
    free(node);
    return job;
}

void ready_queue_close(ready_queue_t *queue) {
    pthread_mutex_lock(&queue->mutex);
    queue->closed = true;
    pthread_cond_broadcast(&queue->not_empty);  // wake all waiting agents
    pthread_mutex_unlock(&queue->mutex);
}