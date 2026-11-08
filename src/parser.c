
#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline.h"
#include "array.h"
#include "parser.h"
#include "parser_helpers.h"

step_t* step(ctx_t* c, yaml_node_t* n) {
    if (!is_map(c, n, "|script|task|inputs|publish|artifact|download|path|continueOnError|"))
        return NULL;
    step_t* s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->line = (int)n->start_mark.line + 1;

    yaml_node_t *sc = get(c, n, "script"), *tk = get(c, n, "task"),
                *pb = get(c, n, "publish"), *dl = get(c, n, "download");
    if ((!!sc) + (!!tk) + (!!pb) + (!!dl) != 1) {
        err(c, n, "el step debe tener al menos un script, task, publish o download");
        return s;
    }
    const char* art = str(get(c, n, "artifact"));
    const char* path = str(get(c, n, "path"));

    if (sc) {
        s->type = STEP_SCRIPT;
        s->script = copy(str(sc));
    } else if (tk) {
        s->type = STEP_TASK;
        s->task = copy(str(tk));
        s->inputs = array_init(ARR_INITIAL_CAPACITY);
        pairs(c, get(c, n, "inputs"), s->inputs);
    } else if (pb) {
        s->type = STEP_PUBLISH;
        s->publish_path = copy(str(pb));
        s->artifact = copy(art);
        if (!art) err(c, n, "publish requiere 'artifact'");
    } else {
        s->type = STEP_DOWNLOAD;
        s->artifact = copy(str(dl));
        s->download_path = copy(path);
        if (!path) err(c, n, "download requiere 'path'");
    }
    const char* coe = str(get(c, n, "continueOnError"));
    s->continue_on_error = coe && !strcmp(coe, "true");
    return s;
}

job_t* job(ctx_t* c, yaml_node_t* n) {
    if (!is_map(c, n, "|job|dependsOn|condition|timeoutInSeconds|steps|")) return NULL;
    job_t* j = calloc(1, sizeof *j);
    if (!j) return NULL;
    j->line = (int)n->start_mark.line + 1;

    j->name = copy(str(get(c, n, "job")));
    if (!j->name) err(c, n, "falta 'job'");

    yaml_node_t* dep = get(c, n, "dependsOn");
    j->depends_on = dep ? depends(c, dep) : array_init(ARR_INITIAL_CAPACITY);
    j->condition = cond(c, get(c, n, "condition"));

    const char* t = str(get(c, n, "timeoutInSeconds"));
    j->timeout_in_seconds = t ? atof(t) : 0;

    j->steps = array_init(ARR_INITIAL_CAPACITY);
    yaml_node_t* st = get(c, n, "steps");
    if (!st || st->type != YAML_SEQUENCE_NODE) {
        err(c, st ? st : n, "faltan los 'steps'");
    } else {
        EACH(c, st, it) { 
            step_t* s = step(c, it);
            if (s) array_add(j->steps, s); 
        }
    }
    return j;
}

stage_t* stage(ctx_t* c, yaml_node_t* n) {
    if (!is_map(c, n, "|stage|jobs|dependsOn|condition|")) return NULL;
    stage_t* s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->line = (int)n->start_mark.line + 1;

    s->name = copy(str(get(c, n, "stage")));
    if (!s->name) err(c, n, "falta 'stage'");

    yaml_node_t* dep = get(c, n, "dependsOn");
    s->has_depends_on = dep != NULL;
    if (dep) s->depends_on = depends(c, dep);
    s->condition = cond(c, get(c, n, "condition"));

    s->jobs = array_init(ARR_INITIAL_CAPACITY);
    yaml_node_t* jobs = get(c, n, "jobs");
    if (!jobs || jobs->type != YAML_SEQUENCE_NODE) {
        err(c, jobs ? jobs : n, "faltn los jobs");
    } else {
        EACH(c, jobs, it) { job_t* j = job(c, it); if (j) array_add(s->jobs, j); }
    }
    return s;
}

pipeline_t* pipeline(ctx_t* c, yaml_node_t* r) {
    if (!is_map(c, r, "|name|variables|stages|")) return NULL;
    pipeline_t* pipe = calloc(1, sizeof *pipe);
    if (!pipe) return NULL;

    pipe->name = copy(str(get(c, r, "name")));
    if (!pipe->name) err(c, r, "falta 'name'");

    pipe->variables = array_init(ARR_INITIAL_CAPACITY);
    pairs(c, get(c, r, "variables"), pipe->variables);

    pipe->stages = array_init(ARR_INITIAL_CAPACITY);
    yaml_node_t* st = get(c, r, "stages");
    if (!st || st->type != YAML_SEQUENCE_NODE) {
        err(c, st ? st : r, "faltan los stages");
    } else {
        EACH(c, st, it) { stage_t* s = stage(c, it); if (s) array_add(pipe->stages, s); }
    }
    return pipe;
}
pipeline_t* load_pipeline(const char* filename, array_t** errors) {
    ctx_t ctx = { 
        .doc = NULL,
        .errors = array_init(ARR_INITIAL_CAPACITY)
    };
    *errors = ctx.errors;
    pipeline_t* result = NULL;

    yaml_parser_t parser;
    yaml_document_t doc;
    if (!yaml_parser_initialize(&parser)) return NULL;
    FILE* file = fopen(filename, "rb");
    
    yaml_parser_set_input_file(&parser, file);

    if (!yaml_parser_load(&parser, &doc)) {
        yaml_node_t fake = { .start_mark = parser.problem_mark };
        err(&ctx, &fake, "YAML inválido: %s", parser.problem ? parser.problem : "sintaxis");
    } else {
        ctx.doc = &doc;
        result = pipeline(&ctx, yaml_document_get_root_node(&doc));
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);

    fclose(file);
    if (ctx.errors->num_items > 0 && result) { 
        pipeline_destroy(result); 
        result = NULL; 
    }
    return result;
}