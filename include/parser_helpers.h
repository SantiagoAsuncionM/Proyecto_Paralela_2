#pragma once
#include <yaml.h>
#include "pipeline.h"
#include "array.h"

typedef struct {
    yaml_document_t* doc;
    array_t* errors;
} ctx_t;

// para recorrer varios elementos
#define EACH(c, seq, it) \
    for (yaml_node_item_t* _i = (seq)->data.sequence.items.start; \
         _i < (seq)->data.sequence.items.top; _i++) \
        for (yaml_node_t* it = N(c, *_i); it; it = NULL)


void err(ctx_t* c, const yaml_node_t* n, const char* fmt, ...);
yaml_node_t* N(ctx_t* ctx, int idx);
char* copy(const char* string);
const char* str(const yaml_node_t* n);
yaml_node_t* get(ctx_t* ctx, yaml_node_t* m, const char* key);
bool is_map(ctx_t* ctx, yaml_node_t* n, const char* allowed);
void pairs(ctx_t* ctx, yaml_node_t* m, array_t* out);
array_t* depends(ctx_t* ctx, yaml_node_t* n);
condition_t cond(ctx_t* ctx, yaml_node_t* n);
