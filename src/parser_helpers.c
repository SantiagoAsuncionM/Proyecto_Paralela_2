#include <yaml.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_helpers.h"
#include "pipeline.h"
#include "array.h"
#include "parser.h"

void err(ctx_t* c, const yaml_node_t* n, const char* fmt, ...) {
    char msg[200], full[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    snprintf(full, sizeof full, "línea %zu: %s", n ? n->start_mark.line + 1 : 1, msg);
    array_add(c->errors, strdup(full));
}

yaml_node_t* N(ctx_t* ctx, int idx) { 
    return yaml_document_get_node(ctx->doc, idx);
}

char* copy(const char* string) { 
    return string ? strdup(string) : NULL;
}

const char* str(const yaml_node_t* n) {
    return n && n->type == YAML_SCALAR_NODE ? (const char*)n->data.scalar.value : NULL;
}

yaml_node_t* get(ctx_t* ctx, yaml_node_t* m, const char* key) {
    for (yaml_node_pair_t* p = m->data.mapping.pairs.start; p < m->data.mapping.pairs.top; p++) {
        const char* k = str(N(ctx, p->key));
        if (k && strcmp(k, key) == 0) {
            return N(ctx, p->value);
        } 
    }
    return NULL;
}

bool is_map(ctx_t* ctx, yaml_node_t* n, const char* allowed) {
    if (!n || n->type != YAML_MAPPING_NODE) { 
        err(ctx, n, "se esperaba un mapa"); 
        return false;
    }
    for (yaml_node_pair_t* p = n->data.mapping.pairs.start; p < n->data.mapping.pairs.top; p++) {
        yaml_node_t* k = N(ctx, p->key);
        char pat[64];
        snprintf(pat, sizeof pat, "|%s|", str(k) ? str(k) : "");
        if (!strstr(allowed, pat)) {
            err(ctx, k, "clave desconocida '%s'", str(k) ? str(k) : "?");
        } 
    }
    return true;
}

void pairs(ctx_t* ctx, yaml_node_t* m, array_t* out) {
    if (!m) return;
    if (m->type != YAML_MAPPING_NODE) { 
        err(ctx, m, "se esperaba un mapa");
         return; 
    }
    for (yaml_node_pair_t* p = m->data.mapping.pairs.start; p < m->data.mapping.pairs.top; p++) {
        const char *k = str(N(ctx, p->key)), *v = str(N(ctx, p->value));
        if (!k || !v) { 
            err(ctx, N(ctx, p->key), "par clave/valor inválido");
            continue;
        }
        par_var_t* pv = calloc(1, sizeof *pv);
        if (!pv) continue;
        pv->name = strdup(k);
        pv->value = strdup(v);
        array_add(out, pv);
    }
}

array_t* depends(ctx_t* ctx, yaml_node_t* n) {
    array_t* a = array_init(ARR_INITIAL_CAPACITY);
    if (str(n)) {
        array_add(a, strdup(str(n)));
    } else if (n->type == YAML_SEQUENCE_NODE) {
        EACH(ctx, n, it) {
            if (str(it)) array_add(a, strdup(str(it)));
            else err(ctx, it, "dependsOn debe contener nombres");
        }
    } else {
        err(ctx, n, "dependsOn debe ser un nombre o una lista");
    }
    return a;
}

condition_t cond(ctx_t* ctx, yaml_node_t* n) {
    const char* s = str(n);
    if (!n) {
        return COND_SUCCEEDED;
    }
    if (s && !strcmp(s, "failed()")) {
        return COND_FAILED;
    } 
    if (s && !strcmp(s, "always()")) {
        return COND_ALWAYS;
    }   
    if (!s || strcmp(s, "succeeded()")) {
        err(ctx, n, "condition debe ser succeeded(), failed() o always()");
    } 
    return COND_SUCCEEDED;
}
