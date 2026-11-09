#pragma once

// Run variables: array_t of par_var_t* (name, value).

#include <stdbool.h>

#include "array.h"
#include "pipeline.h"

// Value of a variable, else NULL.
const char *vars_get(const array_t *vars, const char *name);

// Sets a variable (replace if exists). False if out of memory.
bool vars_set(array_t *vars, const char *name, const char *value);

// Frees the variables and the array.
void vars_free(array_t *vars);

// Copy of text with each $(NAME) replaced by its value.
char *vars_substitute(const array_t *vars, const char *text);