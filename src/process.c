#define _GNU_SOURCE  // pipe2
#include "process.h"
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "vars.h"

extern char **environ;

//Processes an output line from the script: saves it to the log, if is ##setvariable defines the variable.
static void handle_line(const char *line, task_context_t *context) {
    const char *prefix = "##setvariable ";
    if (strncmp(line, prefix, strlen(prefix)) == 0) {
        char name[128];
        const char *start = line + strlen(prefix);
        const char *equal = strchr(start, '=');
        size_t length = equal != NULL ? (size_t)(equal - start) : 0;
        if (length > 0 && length < sizeof name) {
            memcpy(name, start, length);
            name[length] = '\0';
            vars_set(context->vars, name, equal + 1);
        }
    }
    if (context->log != NULL) joblog_append(context->log, line);
}

task_result_t process_run_script(const char *script, task_context_t *context) {
    // "cd <workdir> && <script>", so the script runs in the job workspace.
    size_t size = strlen(context->workdir) + strlen(script) + 16;
    char *command = malloc(size);
    if (command == NULL) return TASK_FAILED;
    snprintf(command, size, "cd '%s' && %s", context->workdir, script);

    // stdout and stderr of the child go to a pipe. 
    int pipe_fds[2];
    if (pipe2(pipe_fds, O_CLOEXEC) != 0) {// O_CLOEXEC: other children started at the same time do not inherit the pipe
        free(command);
        return TASK_FAILED;
    }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDERR_FILENO);

    // New process group, so a kill also stops script's own children.
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes, 0);

    char *argv[] = { "sh", "-c", command, NULL };
    pid_t pid;
    int spawned = posix_spawn(&pid, "/bin/sh", &actions, &attributes, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    free(command);
    close(pipe_fds[1]);
    if (spawned != 0) {
        close(pipe_fds[0]);
        return TASK_FAILED;
    }

    // Read the output in lines; every 100 ms check cancel and timeout.
    task_result_t stop = TASK_OK;
    char line[1024];
    size_t length = 0;
    struct pollfd wait_for = { .fd = pipe_fds[0], .events = POLLIN };
    while (true) {
        if (check_task_stop(context, &stop)) {
            kill(-pid, SIGKILL);
            break;
        }
        if (poll(&wait_for, 1, 100) <= 0) continue;  // nothing new yet
        char chunk[512];
        ssize_t got = read(pipe_fds[0], chunk, sizeof chunk);
        if (got <= 0) break;  // the child closed its output
        for (ssize_t i = 0; i < got; i++) {
            if (chunk[i] == '\n' || length == sizeof line - 1) {
                line[length] = '\0';
                handle_line(line, context);
                length = 0;
                if (chunk[i] == '\n') continue;
            }
            line[length++] = chunk[i];
        }
    }
    if (length > 0) { 
        line[length] = '\0';
        handle_line(line, context);
    }
    close(pipe_fds[0]);

    int status;
    waitpid(pid, &status, 0); 
    if (stop != TASK_OK) return stop;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? TASK_OK : TASK_FAILED;
}

bool process_run(char *const argv[]) {
    pid_t pid;
    if (posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ) != 0) return false;
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}