#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "validate_pipeline.h"

void add_error(array_t* errors, int line, const char* fmt, ...) {
    char msg[200], full[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    snprintf(full, sizeof full, "línea %zu: %s", line, msg);
    array_add(errors, strdup(full));
}

stage_t* find_stage(pipeline_t* pipeline, const char* name) {
    for (int i = 0; i < pipeline->stages->num_items; i++) {
        stage_t* s = pipeline->stages->items[i];
        if (s->name && strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

job_t* find_job(stage_t* s, const char* name) {
    for (int i = 0; i < s->jobs->num_items; i++) {
        job_t* j = s->jobs->items[i];
        if (j->name && strcmp(j->name, name) == 0) return j;
    }
    return NULL;
}

void validate_names_and_refs(pipeline_t* pipeline, array_t* errors) {
    for (int i = 0; i < pipeline->stages->num_items; i++) {
        stage_t* stage = pipeline->stages->items[i];
        if (!stage->name) continue;

        stage_t* first = find_stage(pipeline, stage->name);
        if (first != stage) {
            add_error(errors, stage->line, "etapa '%s' duplicada (ya definida en la línea %d)",
                    stage->name, first->line);
        }

        if (stage->depends_on) {
            for (int d = 0; d < stage->depends_on->num_items; d++) {
                const char* dn = stage->depends_on->items[d];
                if (strcmp(dn, stage->name) == 0) {
                    add_error(errors, stage->line, "la etapa '%s' no puede depender de ella misma", dn);
                }
                else if (!find_stage(pipeline, dn)) {
                    add_error(errors, stage->line, "la etapa '%s' depende de '%s', que no existe",
                        stage->name, dn);
                }
            }
        }

        for (int k = 0; k < stage->jobs->num_items; k++) {
            job_t* job = stage->jobs->items[k];
            if (!job->name) continue;

            job_t* job_first = find_job(stage, job->name);
            if (job_first != job) {
                add_error(errors, job->line,
                    "trabajo '%s' duplicado en la etapa '%s' (ya definido en la línea %d)",
                    job->name, stage->name, job_first->line);
            }

            for (int d = 0; d < job->depends_on->num_items; d++) {
                const char* dn = job->depends_on->items[d];
                if (strcmp(dn, job->name) == 0) {
                    add_error(errors, job->line, "el trabajo '%s' no puede depender de %s", dn, job->name);
                }
                else if (!find_job(stage, dn)) {
                    add_error(errors, job->line,
                        "el trabajo '%s' depende de '%s', que no existe en la etapa '%s'",
                        job->name, dn, stage->name);
                }
            }
        }
    }
}
