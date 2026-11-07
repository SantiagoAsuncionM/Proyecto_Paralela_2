#define _POSIX_C_SOURCE 200809L
#include "vars.h"

#include <stdlib.h>
#include <string.h>

const char *vars_get(const array_t *vars, const char *name) {
    for (size_t i = 0; i < vars->num_items; i++) {
        const par_var_t *var = vars->items[i];
        if (strcmp(var->name, name) == 0) return var->value;
    }
    return NULL;
}

bool vars_set(array_t *vars, const char *name, const char *value) {
    char *copy = strdup(value);
    if (copy == NULL) return false;
    for (size_t i = 0; i < vars->num_items; i++) {
        par_var_t *var = vars->items[i];
        if (strcmp(var->name, name) == 0) {
            free(var->value);
            var->value = copy;
            return true;
        }
    }
    par_var_t *var = malloc(sizeof *var);
    if (var == NULL) {
        free(copy);
        return false;
    }
    var->name = strdup(name);
    var->value = copy;
    if (var->name == NULL || array_add(vars, var) < 0) {
        free(var->name);
        free(copy);
        free(var);
        return false;
    }
    return true;
}

void vars_free(array_t *vars) {
    if (vars == NULL) return;
    for (size_t i = 0; i < vars->num_items; i++) {
        par_var_t *var = vars->items[i];
        free(var->name);
        free(var->value);
        free(var);
    }
    free(vars->items);
    free(vars);
}

//checks if starts with known ($NAME), return value if so.
static const char *find_var(const array_t *vars, const char *text, size_t *skip) {
    if (text[0] != '$' || text[1] != '(') return NULL;
    const char *close = strchr(text + 2, ')');
    if (close == NULL) return NULL;
    char name[128];
    size_t name_length = (size_t)(close - (text + 2));
    if (name_length >= sizeof name) return NULL;
    memcpy(name, text + 2, name_length);
    name[name_length] = '\0';
    *skip = name_length + 3;  // "$(" + NAME + ")"
    return vars_get(vars, name);
}

char *vars_substitute(const array_t *vars, const char *text) {
    // 1. Calculate length
    size_t length = 0;
    for (size_t i = 0; text[i] != '\0';) {
        size_t skip;
        const char *value = find_var(vars, text + i, &skip);
        if (value != NULL) {
            length += strlen(value);
            i += skip;
        } else {
            length++;
            i++;
        }
    }

    char *result = malloc(length + 1);
    if (result == NULL) return NULL;

    // 2. Build the string
    size_t out = 0;
    for (size_t i = 0; text[i] != '\0';) {
        size_t skip;
        const char *value = find_var(vars, text + i, &skip);
        if (value != NULL) {
            memcpy(result + out, value, strlen(value));
            out += strlen(value);
            i += skip;
        } else {
            result[out++] = text[i++];
        }
    }
    result[out] = '\0';
    return result;
}