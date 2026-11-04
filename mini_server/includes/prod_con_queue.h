#pragma once

#include <pthread.h>
#include <stddef.h>
#include <semaphore.h>

#include "net_util.h"

enum pc_queue_result { PC_FULL = 400, PC_OK, PC_CLOSED, PC_ERROR };

typedef struct {
    void** items;
    size_t capacity, head, tail, count;
    int closed;
    pthread_mutex_t mutex;
    sem_t items_sem;
    sem_t spaces_sem;
} pc_queue_t;

int pc_queue_init(pc_queue_t* q, size_t capacity);
int pc_queue_try_push(pc_queue_t* q, void* item);
int pc_queue_push(pc_queue_t* q, void* item);
int pc_queue_item_idx(pc_queue_t* q, void* item);
void* pc_queue_pop(pc_queue_t* q);
void pc_queue_close(pc_queue_t* q);
void pc_queue_destroy(pc_queue_t* q);
size_t pc_queue_count(pc_queue_t* q);
