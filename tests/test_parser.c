#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "array.h"
#include "../include/pipeline.h"
#include "parser.h"
#include "validate_pipeline.h"
#include "graph.h"

#define YAML_DIR "yamls/tests"
#define MAX_EXPECTED 8

typedef struct {
    int line;
    const char* text;
} expected_t;

typedef struct {
    const char* file;
    bool valid;
    int n_errors;
    expected_t expected[MAX_EXPECTED];
} case_t;

static const case_t CASES[] = {
    { "valid.yaml", true, 0, { {0, NULL} } },

    // parser
    { "invalid_format.yaml", false, 6, {
        {2, "clave desconocida 'colorFavorito'" },
        {8, "al menos un" },
        {10, "publish requiere 'artifact'" },
        {11, "download requiere 'path'" },
        {12, "clave desconocida 'paso_raro'" },
        {12, "al menos un" } } },

    // validación
    { "invalid_names.yaml", false, 5, {
        { 8, "'build' duplicado" },
        {11, "'inexistente'" },
        {15, "no puede depender de" },
        {19, "etapa 'compilar' duplicada" },
        {24, "'desplegar'" } } },

    // grafo
    { "job_cycles.yaml",false, 1, { {0, "cíclica" } } },
    { "stages_cycle.yaml", false, 1,{ { 0, "cíclica" } } },
};

static bool run(const char* path, array_t** errors) {
    pipeline_t* pipeline = load_pipeline(path, errors);
    if (!pipeline) return false;

    validate_names_and_refs(pipeline, *errors);

    bool ok = (*errors)->num_items == 0;
    if (ok) {
        dep_graph_t* graph = graph_build(pipeline);
        ok = graph != NULL;
        if (graph) {
            graph_check_cycles(graph, *errors);
            ok = (*errors)->num_items == 0;
            graph_destroy(graph);
        }
    }
    pipeline_destroy(pipeline);
    return ok;
}

static bool has_error(const array_t* errors, const expected_t* expected) {
    char prefix[32];
    snprintf(prefix, sizeof prefix, "línea %d:", expected->line);
    for (size_t i = 0; i < errors->num_items; i++) {
        const char* message = errors->items[i];
        if (expected->line != 0 && strncmp(message, prefix, strlen(prefix)) != 0) continue;
        if (strstr(message, expected->text)) return true;
    }
    return false;
}

static void add_reason(char* reasons, size_t size, const char* fmt, ...) {
    size_t used = strlen(reasons);
    va_list args;
    va_start(args, fmt);
    vsnprintf(reasons + used, size - used, fmt, args);
    va_end(args);
}

static bool run_case(const case_t* test_case) {
    char path[256];
    snprintf(path, sizeof path, "%s/%s", YAML_DIR, test_case->file);

    array_t* errors = NULL;
    bool ok = run(path, &errors);
    int n_errors = errors ? errors->num_items : 0;

    char reasons[1024] = "";
    if (ok != test_case->valid) {
        add_reason(reasons, sizeof reasons, "  - se esperaba un pipeline %s\n",
            test_case->valid ? "válido" : "inválido");
    }
    if (test_case->n_errors >= 0 && n_errors != test_case->n_errors)
        add_reason(reasons, sizeof reasons, "  - errores: esperados %d, obtenidos %d\n",
                   test_case->n_errors, n_errors);
    for (int i = 0; i < MAX_EXPECTED && test_case->expected[i].text; i++) {
        if (!errors || !has_error(errors, &test_case->expected[i]))
            add_reason(reasons, sizeof reasons, "  - falta el error: línea %d, '%s'\n",
                    test_case->expected[i].line, test_case->expected[i].text);
    }

    bool passed = reasons[0] == '\0';
    printf("%s  %s\n", passed ? "OK  " : "FAIL", test_case->file);
    if (!passed) {
        fputs(reasons, stdout);
        for (int i = 0; i < n_errors; i++) {
            printf("      recibido: %s\n", (char*)errors->items[i]);
        }
    }
    array_destroy(errors);
    return passed;
}

#define CHECK(cond) do { if (!(cond)) { printf("  - falló: %s\n", #cond); failures++; } } while (0)

static bool check_valid_content(void) {
    array_t* errors = NULL;
    pipeline_t* pipeline = load_pipeline(YAML_DIR "/valid.yaml", &errors);

    int failures = 0;
    CHECK(pipeline != NULL);
    if (pipeline && pipeline->stages->num_items == 3) {
        stage_t* first  = pipeline->stages->items[0];
        stage_t* second = pipeline->stages->items[1];
        stage_t* third  = pipeline->stages->items[2];

        CHECK(strcmp(pipeline->name, "valido") == 0);
        CHECK(pipeline->variables->num_items == 1);
        CHECK(!first->has_depends_on);
        CHECK(second->has_depends_on && second->depends_on->num_items == 0);
        CHECK(third->has_depends_on && third->depends_on->num_items == 2);
        CHECK(first->jobs->num_items == 2);

        job_t* job = second->jobs->items[0];
        CHECK(strcmp(job->name, "c") == 0);
        CHECK(job->condition == COND_ALWAYS);
        CHECK(job->timeout_in_seconds == 10);
        step_t* step = job->steps->items[0];
        CHECK(step->type == STEP_SCRIPT && step->continue_on_error);
    } else if (pipeline) {
        printf("  - se esperaban 3 etapas y hay %zu\n", pipeline->stages->num_items);
        failures++;
    }

    printf("%s  contenido de valido.yaml\n", failures ? "FAIL" : "OK  ");
    pipeline_destroy(pipeline);
    array_destroy(errors);
    return failures == 0;
}

int main(void) {
    int passed = 0, failed = 0;

    for (size_t i = 0; i < sizeof CASES / sizeof CASES[0]; i++) {
        // printf("idx: %zu\n", i);
        if (run_case(&CASES[i])) passed++; else failed++;
    }
    if (check_valid_content()) passed++; else failed++;

    printf("\n%d pasaron, %d fallaron\n", passed, failed);
    return failed ? 1 : 0;
}