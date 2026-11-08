#include "pipeline.h"
#include "array.h"

void pairs_destroy(array_t* a) {
    if (!a) return;
    for (int i = 0; i < a->num_items; i++) {
        par_var_t* v = a->items[i];
        free(v->name);
        free(v->value);
    }
    array_destroy(a);
}

void step_clean(step_t* s) {
    free(s->script);
    free(s->task);
    free(s->artifact);
    free(s->publish_path);
    free(s->download_path);
    pairs_destroy(s->inputs);
}

void job_clean(job_t* j) {
    free(j->name);
    array_destroy(j->depends_on);
    if (j->steps) {
        for (int i = 0; i < j->steps->num_items; i++) {
            step_clean(j->steps->items[i]);
        } 
        array_destroy(j->steps);
    }
}

void stage_clean(stage_t* s) {
    free(s->name);
    array_destroy(s->depends_on);
    if (s->jobs) {
        for (int i = 0; i < s->jobs->num_items; i++) {
            job_clean(s->jobs->items[i]);
        } 
        array_destroy(s->jobs);
    }
}

void pipeline_destroy(pipeline_t* p) {
    if (!p) return;
    free(p->name);
    pairs_destroy(p->variables);
    if (p->stages) {
        for (int i = 0; i < p->stages->num_items; i++) stage_clean(p->stages->items[i]);
        array_destroy(p->stages);
    }
    free(p);
}
