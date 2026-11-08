#define _POSIX_C_SOURCE 200809L
#include "tasks.h"
#include "vars.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

//*** HELPERS ***//

// Writes a line to the job log.
// TODO: need joblog unit
static void log_line(task_context_t *context, const char *format, ...) {
    (void)context;
    (void)format;
}

// Value of an input, or NULL if missing.
static const char *input_get(const array_t *inputs, const char *key) {
    if (inputs == NULL) return NULL;
    for (size_t i = 0; i < inputs->num_items; i++) {
        const par_var_t *input = inputs->items[i];
        if (strcmp(input->name, key) == 0) return input->value;
    }
    return NULL;
}

// Integer input in min, max. False if missing or invalid.
static bool input_long(task_context_t *context, const array_t *inputs,
                       const char *key, long min, long max, long *result) {
    const char *text = input_get(inputs, key);
    if (text == NULL) {
        log_line(context, "error: falta el input '%s'", key);
        return false;
    }
    errno = 0;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < min || value > max) {
        log_line(context, "error: input inválido '%s'", key);
        return false;
    }
    *result = value;
    return true;
}

// Requested threads, capped at cores / agents (avoids oversubscription).
static int effective_threads(long requested) {
    int limit = omp_get_num_procs() / omp_get_num_threads();
    if (limit < 1) limit = 1;
    long threads = requested < limit ? requested : limit;
    return threads < 1 ? 1 : (int)threads;
}

bool check_task_stop(const task_context_t *context, task_result_t *why) {
    int canceled = 0;
    if (context->cancel != NULL) {
        #pragma omp atomic read
        canceled = *context->cancel;
    }
    if (canceled) {
        if (why) *why = TASK_CANCELED;
        return true;
    }
    if (context->deadline.tv_sec != 0 || context->deadline.tv_nsec != 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > context->deadline.tv_sec ||
            (now.tv_sec == context->deadline.tv_sec &&
             now.tv_nsec >= context->deadline.tv_nsec)) {
            if (why) *why = TASK_TIMEOUT;
            return true;
        }
    }
    return false;
}

// Builds "<workdir>/<relative>".
static bool workpath(task_context_t *context, const char *relative,
                     char *path, size_t size) {
    int written = snprintf(path, size, "%s/%s", context->workdir, relative);
    if (written < 0 || (size_t)written >= size) {
        log_line(context, "error: ruta demasiado larga '%s'", relative);
        return false;
    }
    return true;
}

// Loads a whole file in memory; the caller frees *data.
static bool load_file(task_context_t *context, const char *path,
                      unsigned char **data, size_t *size) {
    *data = NULL;
    *size = 0;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        log_line(context, "error: no se puede abrir '%s'", path);
        return false;
    }
    struct stat info;
    if (fstat(fileno(file), &info) != 0) {
        fclose(file);
        return false;
    }
    size_t expected = (size_t)info.st_size;
    *data = malloc(expected > 0 ? expected : 1);
    if (*data == NULL) {
        fclose(file);
        return false;
    }
    *size = fread(*data, 1, expected, file);
    fclose(file);
    if (*size != expected) {
        log_line(context, "error: no se puede leer '%s'", path);
        free(*data);
        *data = NULL;
        return false;
    }
    return true;
}

//*** Sleep@1  ***//

static task_result_t task_sleep(const array_t *inputs, task_context_t *context) {
    long seconds;
    if (!input_long(context, inputs, "seconds", 0, 86400, &seconds)) return TASK_FAILED;

    // 100 ms slices, to notice cancel and timeout.
    for (long ms = 0; ms < seconds * 1000; ms += 100) {
        task_result_t why;
        if (check_task_stop(context, &why)) return why;
        struct timespec pause = { 0, 100L * 1000 * 1000 };
        nanosleep(&pause, NULL);
    }
    return TASK_OK;
}

//*** Cpu@1  ***//

// Deterministic load (linear congruential generator); defines CPU_RESULT.
static task_result_t task_cpu(const array_t *inputs, task_context_t *context) {
    long iterations, seed;
    if (!input_long(context, inputs, "iterations", 0, LONG_MAX, &iterations) ||
        !input_long(context, inputs, "seed", 0, LONG_MAX, &seed))
        return TASK_FAILED;

    uint64_t value = (uint64_t)seed;
    for (long i = 0; i < iterations; i++) {
        if (i % 1000000 == 0) {  // check cancel every million iterations
            task_result_t why;
            if (check_task_stop(context, &why)) return why;
        }
        value = value * 6364136223846793005ULL + 1442695040888963407ULL;  // LCG Constants (Knuth)
    }

    char text[32];
    snprintf(text, sizeof text, "%" PRIu64, value);
    log_line(context, "CPU_RESULT=%s", text);

    if (context->vars != NULL) vars_set(context->vars, "CPU_RESULT", text);

    return TASK_OK;
}

//*** Histogram@1  ***//

static task_result_t task_histogram(const array_t *inputs, task_context_t *context) {
    const char *file_name = input_get(inputs, "file");
    const char *output_name = input_get(inputs, "output");
    long bins, threads;
    if (file_name == NULL || output_name == NULL) {
        log_line(context, "error: Histogram@1 requiere 'file' y 'output'");
        return TASK_FAILED;
    }
    if (!input_long(context, inputs, "bins", 1, 256, &bins) ||
        !input_long(context, inputs, "threads", 1, 1024, &threads))
        return TASK_FAILED;

    char input_path[PATH_MAX], output_path[PATH_MAX];
    if (!workpath(context, file_name, input_path, sizeof input_path) ||
        !workpath(context, output_name, output_path, sizeof output_path))
        return TASK_FAILED;

    unsigned char *data;
    size_t size;
    if (!load_file(context, input_path, &data, &size)) return TASK_FAILED;

    const int num_bins = (int)bins;
    const int num_threads = effective_threads(threads);
    uint64_t *histogram = calloc((size_t)num_bins, sizeof *histogram);
    if (histogram == NULL) {
        free(data);
        return TASK_FAILED;
    }

    //Reduction to consolidate thread's histograms
    task_result_t result = TASK_OK;
    if (!check_task_stop(context, &result)) {
        #pragma omp parallel for num_threads(num_threads) reduction(+ : histogram[:num_bins])
        for (size_t i = 0; i < size; i++)
            histogram[(size_t)data[i] * (size_t)num_bins / 256u]++;
    }
    free(data);

    if (result == TASK_OK) {
        FILE *output = fopen(output_path, "w");
        if (output == NULL) {
            log_line(context, "error: no se puede crear '%s'", output_name);
            result = TASK_FAILED;
        } else {
            fprintf(output, "bin,count\n");
            for (int bin = 0; bin < num_bins; bin++)
                fprintf(output, "%d,%" PRIu64 "\n", bin, histogram[bin]);
            fclose(output);
        }
    }
    free(histogram);
    return result;
}

//*** WordCount@1  ***//

// Word: ASCII letters and digits, compared in lowercase.
static bool is_word_char(unsigned char c) { return c < 128 && isalnum(c) != 0; }

typedef struct {
    char    *word;
    uint64_t count;
} word_count_t;

// List of distinct words with their counts (linear search).
typedef struct {
    word_count_t *items;
    size_t        count, capacity;
} word_list_t;

// Adds the word to the list if is a new word. If word exists adds amount to its count. False if out of memory.
static bool word_list_add(word_list_t *list, const char *word, uint64_t amount) {
    for (size_t i = 0; i < list->count; i++)
        if (strcmp(list->items[i].word, word) == 0) {
            list->items[i].count += amount;
            return true;
        }
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 64;
        word_count_t *items = realloc(list->items, capacity * sizeof *items);
        if (items == NULL) return false;
        list->items = items;
        list->capacity = capacity;
    }
    char *copy = strdup(word);
    if (copy == NULL) return false;
    list->items[list->count].word = copy;
    list->items[list->count].count = amount;
    list->count++;
    return true;
}

static void word_list_free(word_list_t *list) {
    for (size_t i = 0; i < list->count; i++) free(list->items[i].word);
    free(list->items);
}

//Compare function for qsort.
static int compare_words(const void *a, const void *b) {
    const word_count_t *first = a, *second = b;
    if (first->count != second->count) return first->count > second->count ? -1 : 1;
    return strcmp(first->word, second->word);
}

// Start of chunk number "chunk" of "chunks", moved so it never splits a word.
static size_t chunk_start(const unsigned char *data, size_t size, int chunk, int chunks) {
    if (chunk == 0) return 0;
    if (chunk >= chunks) return size;
    size_t start = size / (size_t)chunks * (size_t)chunk;
    while (start < size && is_word_char(data[start - 1]) && is_word_char(data[start]))
        start++;
    return start;
}

static task_result_t task_wordcount(const array_t *inputs, task_context_t *context) {
    const char *file_name = input_get(inputs, "file");
    const char *output_name = input_get(inputs, "output");
    long threads, top = 10;
    if (file_name == NULL || output_name == NULL) {
        log_line(context, "error: WordCount@1 requiere 'file' y 'output'");
        return TASK_FAILED;
    }
    if (!input_long(context, inputs, "threads", 1, 1024, &threads)) return TASK_FAILED;
    if (input_get(inputs, "top") != NULL &&
        !input_long(context, inputs, "top", 0, 1000000, &top))
        return TASK_FAILED;

    char input_path[PATH_MAX], output_path[PATH_MAX];
    if (!workpath(context, file_name, input_path, sizeof input_path) ||
        !workpath(context, output_name, output_path, sizeof output_path))
        return TASK_FAILED;

    unsigned char *data;
    size_t size;
    if (!load_file(context, input_path, &data, &size)) return TASK_FAILED;

    // One chunk per thread; each chunk counts in its own list.
    const int num_threads = effective_threads(threads);
    word_list_t *lists = calloc((size_t)num_threads, sizeof *lists);
    if (lists == NULL) {
        free(data);
        return TASK_FAILED;
    }

    int failed = 0;  // 1 => out of memory in some chunk
    #pragma omp parallel for num_threads(num_threads)
    for (int chunk = 0; chunk < num_threads; chunk++) {
        if (check_task_stop(context, NULL)) continue;
        size_t pos = chunk_start(data, size, chunk, num_threads);
        size_t end = chunk_start(data, size, chunk + 1, num_threads);
        while (pos < end) {
            while (pos < end && !is_word_char(data[pos])) pos++;
            char word[256];
            size_t length = 0;
            while (pos < end && is_word_char(data[pos])) {
                if (length < sizeof word - 1) word[length++] = (char)tolower(data[pos]);
                pos++;
            }
            if (length == 0) continue;
            word[length] = '\0';
            if (!word_list_add(&lists[chunk], word, 1)) {
                #pragma omp atomic write
                failed = 1;
                break;
            }
        }
    }
    free(data);

    task_result_t result = TASK_OK;
    if (failed) result = TASK_FAILED;
    else check_task_stop(context, &result);  // canceled or timed out

    // Serial merge
    word_list_t all = {0};
    uint64_t total = 0;
    for (int chunk = 0; chunk < num_threads; chunk++) {
        for (size_t i = 0; i < lists[chunk].count && result == TASK_OK; i++) {
            total += lists[chunk].items[i].count;
            if (!word_list_add(&all, lists[chunk].items[i].word,
                               lists[chunk].items[i].count))
                result = TASK_FAILED;
        }
        word_list_free(&lists[chunk]);
    }
    free(lists);

    if (result == TASK_OK) {
        qsort(all.items, all.count, sizeof *all.items, compare_words);
        FILE *output = fopen(output_path, "w");
        if (output == NULL) {
            log_line(context, "error: no se puede crear '%s'", output_name);
            result = TASK_FAILED;
        } else {
            fprintf(output, "total,%" PRIu64 "\n", total);
            fprintf(output, "distinct,%zu\n", all.count);
            for (size_t i = 0; i < all.count && i < (size_t)top; i++)
                fprintf(output, "%s,%" PRIu64 "\n", all.items[i].word, all.items[i].count);
            fclose(output);
        }
    }
    word_list_free(&all);
    return result;
}

//*** Dispatch  ***//

task_result_t task_run(const char *name, const array_t *inputs,
                       task_context_t *context) {
    if (strcmp(name, "Sleep@1") == 0)     return task_sleep(inputs, context);
    if (strcmp(name, "Cpu@1") == 0)       return task_cpu(inputs, context);
    if (strcmp(name, "WordCount@1") == 0) return task_wordcount(inputs, context);
    if (strcmp(name, "Histogram@1") == 0) return task_histogram(inputs, context);
    log_line(context, "error: tarea desconocida '%s'", name);
    return TASK_FAILED;
}