#pragma once
#include <omp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    char **lines;     
    size_t count;     
    size_t capacity;  
    bool closed;    // true cuando el poceso termino
    FILE *file;
    omp_lock_t  lock;      
} job_log_t;

void joblog_init(job_log_t *log);
void joblog_destroy(job_log_t *log);
void joblog_append(job_log_t *log, const char *text);
void joblog_close(job_log_t *log);
size_t joblog_read(job_log_t *log, size_t from, char *buf, size_t cap, bool *closed);
void joblog_open_file(job_log_t* log, const char* path);