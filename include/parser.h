#pragma once
#include <yaml.h>

#include "pipeline.h"
#include "parser_helpers.h"

#define ARR_INITIAL_CAPACITY 10

typedef struct {
    array_t* errors;
    yaml_node_t* node;
    yaml_document_t* doc;
    pipeline_t* pipeline;
} parse_data_t;

pipeline_t* load_pipeline(const char* filename, array_t** errors);
step_t* step(ctx_t* c, yaml_node_t* n);
job_t* job(ctx_t* c, yaml_node_t* n);
stage_t* stage(ctx_t* c, yaml_node_t* n);
pipeline_t* pipeline(ctx_t* c, yaml_node_t* r);


