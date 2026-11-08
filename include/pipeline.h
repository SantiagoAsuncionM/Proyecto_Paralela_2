#pragma once
#include <stdbool.h>
#include "array.h"

typedef enum {
    COND_SUCCEEDED,
    COND_FAILED,
    COND_ALWAYS,
} condition_t;

typedef enum { 
    STEP_SCRIPT,
    STEP_TASK,
    STEP_PUBLISH,
    STEP_DOWNLOAD 
} step_type_t;

typedef struct {
    char* name;
    char* value;
} par_var_t;

typedef struct {
    char* script;
    char* task;
    array_t* inputs;
    bool continue_on_error;
    step_type_t type;
    char* artifact;
    char* publish_path;
    char* download_path;
    int line;
} step_t;

typedef struct {
    char* name;
    array_t* depends_on;
    condition_t condition;
    double timeout_in_seconds;
    array_t* steps;
    int line;
} job_t;

typedef struct {
    char* name;
    array_t* jobs;
    array_t* depends_on;
    condition_t condition;
    bool has_depends_on; 
    int line;
} stage_t;

typedef struct {
    char* name;
    array_t* variables;
    array_t* stages;
} pipeline_t;

void pairs_destroy(array_t* a);
void step_clean(step_t* s);
void job_clean(job_t* j);
void stage_clean(stage_t* s);
void pipeline_destroy(pipeline_t* p);