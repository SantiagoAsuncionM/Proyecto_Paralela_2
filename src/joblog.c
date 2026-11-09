#define _POSIX_C_SOURCE 200809L
#include "joblog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//init
void joblog_init(job_log_t *log) {
    log->lines = NULL;
    log->count = 0;
    log->capacity = 0;
    log->closed = false;
    omp_init_lock(&log->lock);
}

//destroy
void joblog_destroy(job_log_t *log) {
    for (size_t i = 0; i < log->count; i++) {
        free(log->lines[i]);
    }
    free(log->lines);

    log->lines = NULL;
    log->count = 0;
    log->capacity = 0;
    omp_destroy_lock(&log->lock);
}

// agregar una linea al log
void joblog_append(job_log_t *log, const char *text) {

    char *line = strdup(text);
    if (line == NULL) {
        return;
    }

    size_t length = strlen(line);
    while (length > 0) {
        char last = line[length - 1];
        if (last == '\n' || last == '\r') {
            line[length - 1] = '\0';
            length = length - 1;
        } else {
            break;
        }
    }

    omp_set_lock(&log->lock);

    if (log->count == log->capacity) {
        size_t new_capacity;
        if (log->capacity == 0) {
            new_capacity = 16;
        } else {
            new_capacity = log->capacity * 2;
        }

        char **new_lines = realloc(log->lines, new_capacity * sizeof(char *));
        if (new_lines == NULL) {
            omp_unset_lock(&log->lock);
            free(line);
            return;
        }
        log->lines = new_lines;
        log->capacity = new_capacity;
    }

    log->lines[log->count] = line;
    log->count = log->count + 1;

    //escribir tambien en el archivo
    if(log->file != NULL){
        fprintf(log->file, "%s\n", line);
        fflush(log->file);
    }
    omp_unset_lock(&log->lock);
}

//cierra el archivo del log
void joblog_close(job_log_t *log) {
    omp_set_lock(&log->lock);
    log->closed = true;
    if(log->file != NULL){
        fclose(log->file);
        log->file = NULL;
    }
    omp_unset_lock(&log->lock);
}

// Copia las lineas del log y devuelve la siguiente posicion por leer. Para enviar al cliente
size_t joblog_read(job_log_t *log, size_t from, char *buf, size_t cap, bool *closed) {
    size_t used = 0;    
    size_t i = from;

    omp_set_lock(&log->lock);

    while (i < log->count) {
        size_t length = strlen(log->lines[i]);

        if (used + length + 2 > cap) {
            break;
        }

        memcpy(buf + used, log->lines[i], length);
        used = used + length;
        buf[used] = '\n';
        used = used + 1;

        i = i + 1;
    }

    if (closed != NULL) {
        if (log->closed && i == log->count) {
            *closed = true;
        } else {
            *closed = false;
        }
    }

    omp_unset_lock(&log->lock);

    buf[used] = '\0';
    return i;
}


//abre el archivo del log
 void joblog_open_file(job_log_t *log, const char *path) {
      omp_set_lock(&log->lock);
      log->file = fopen(path, "w");
      omp_unset_lock(&log->lock);
  }