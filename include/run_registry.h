#pragma once
#include <stddef.h>
#include "run.h"
#include "executor.h"
#include <stdbool.h>

int run_add(run_t* run);
int run_status(int id, char* buf, size_t cap);
int run_list(char* buf, size_t cap);
int run_cancel(int id);
int run_logs(int id, const char* job, char* buf, size_t cap);
void run_registry_destroy(void);
int run_job_count(int id);

//si el id no existe retorna -1, si el job no exite retorna -2
int run_logs_follow(int id, const char* job, size_t* positions, char* buf, size_t cap, bool* finished);
