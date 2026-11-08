#pragma once

// Child processes: script steps and simple commands.

#include "array.h"
#include "joblog.h"
#include "tasks.h"  // task_context_t, task_result_t

// Runs a script with "sh -c" in context workdir. Output lines go to the log.
// "##setvariable NAME=value" lines set a variable in context vars.
task_result_t process_run_script(const char *script, task_context_t *context);

// Runs a command and waits. True if it exits with 0.
bool process_run(char *const argv[]);