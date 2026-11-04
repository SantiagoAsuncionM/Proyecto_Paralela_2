#pragma once

typedef enum { CMD_SUBMIT, CMD_STATUS, CMD_LOGS, CMD_CANCEL, CMD_LIST, CMD_SHUTDOWN, CMD_INVALIDO } cmd_type_t;

typedef struct {
    cmd_type_t tipo;
    char args[3][128];
    int  argc;
    char error[64];
} cmd_t;

int parse_command(const char *linea, cmd_t *out);
