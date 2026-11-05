#pragma once
#include <stddef.h>

typedef struct {
    int id;
    const char* status;   
    char* yaml;           
} run_t;


int run_add(char* yaml);
int run_status(int id, char* buf, size_t cap);
int run_list(char* buf, size_t cap);
int run_cancel(int id);
//int logs();
void run_registry_destroy(void);