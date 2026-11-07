#pragma once

// Ready Queue: Queue of jobs ready to run

#include <pthread.h>
#include <stdbool.h>

typedef struct ready_node {
    void              *job;
    struct ready_node *next;
} ready_node_t;

typedef struct {
    ready_node_t *head, *tail;
    bool          closed;  // true after shutdown
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;  // signaled when a job arrives or queue closes
} ready_queue_t;

void ready_queue_init(ready_queue_t *queue);
void ready_queue_destroy(ready_queue_t *queue);

// Adds a job at the end of the queue. False if out of memory or closed.
bool ready_queue_push(ready_queue_t *queue, void *job);

// Takes the next job from the queue. Waits if empty. NULL if closed.
void *ready_queue_pop(ready_queue_t *queue);

// Closes the queue: waiting agents get NULL.
void ready_queue_close(ready_queue_t *queue);

// TODO: remove the jobs of a canceled run (needed for cancel <id>).