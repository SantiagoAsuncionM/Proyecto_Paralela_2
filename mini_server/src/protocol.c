#include "protocol.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <stddef.h>

#define LINEA_MAX 512

typedef struct {
    const char* nombre;
    cmd_type_t tipo;
    int aridad_min;
    int aridad_max;
} cmd_def_t;

static const cmd_def_t TABLA_CMD[] = {
    {"SUBMIT", CMD_SUBMIT, 1, 1},
    {"STATUS", CMD_STATUS, 1, 1},
    {"SHUTDOWN", CMD_SHUTDOWN, 0, 0},
    {"LOGS", CMD_LOGS, 1, 3},
    {"CANCEL", CMD_CANCEL, 1, 1},
    {"LIST", CMD_LIST, 0, 0},
};
#define NUM_CMD (sizeof TABLA_CMD / sizeof TABLA_CMD[0])

static int write_fallo(cmd_t* out, const char* motivo) {
    out->tipo = CMD_INVALIDO;
    snprintf(out->error, sizeof out->error, "%s", motivo);
    return -1;
}

int parse_command(const char* linea, cmd_t* out) {
    if (out == NULL)
        return -1;

    memset(out, 0, sizeof *out);

    size_t len = strlen(linea);

    char copia[LINEA_MAX];
    memcpy(copia, linea, len + 1);

    while (len > 0 && (copia[len - 1] == '\n' || copia[len - 1] == '\r'))
        copia[--len] = '\0';

    char* resto = NULL;
    const char* delim = " \t";
    char* tok = strtok_r(copia, delim, &resto);

    if (tok == NULL)
        return write_fallo(out, "linea vacia");

    const cmd_def_t* def = NULL;
    for (size_t i = 0; i < NUM_CMD; i++) {
        if (strcmp(tok, TABLA_CMD[i].nombre) == 0) {
            def = &TABLA_CMD[i];
            break;
        }
    }
    if (def == NULL)
        return write_fallo(out, "comando desconocido");

    int n = 0;
    while ((tok = strtok_r(NULL, delim, &resto)) != NULL) {
        if (n >= def->aridad_max) {
            return write_fallo(out, "sobran parametros");
        }
        snprintf(out->args[n], sizeof out->args[n], "%s", tok);
        n++;
    }

    if (n < def->aridad_min)
        return write_fallo(out, "faltan parametros");

    out->tipo = def->tipo;
    out->argc = n;
    return 0;
}
