#pragma once
#include "pipeline.h"

stage_t* find_stage(pipeline_t* p, const char* name);
job_t* find_job(stage_t* s, const char* name);
void add_error(array_t* errors, int line, const char* fmt, ...);
void validate_names_and_refs(pipeline_t* p, array_t* errors);
