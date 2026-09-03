// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 Nicholas Rodrigues Lordello <n@lordello.net>

#include <assert.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dbx.h"

extern char **environ;

int dbx_proc_run(char *const command[]) {
  assert(command != NULL && command[0] != NULL);

  posix_spawn_file_actions_t file_actions;
  if (posix_spawn_file_actions_init(&file_actions) != 0) {
    perror("spawn_file_actions_init");
    return -1;
  }

  int exit_code = -1;
  if (posix_spawn_file_actions_addclose(&file_actions, STDIN_FILENO) != 0) {
    perror("posix_spawn_file_actions_addclose(stdin)");
    goto cleanup;
  }
  if (posix_spawn_file_actions_addopen(&file_actions, STDOUT_FILENO,
                                       "/dev/null", O_WRONLY, 0) != 0) {
    perror("posix_spawn_file_actions_addopen(stdout)");
    goto cleanup;
  }
  if (posix_spawn_file_actions_addopen(&file_actions, STDERR_FILENO,
                                       "/dev/null", O_WRONLY, 0) != 0) {
    perror("posix_spawn_file_actions_addopen(stderr)");
    goto cleanup;
  }

  pid_t pid;
  if (posix_spawnp(&pid, command[0], &file_actions, NULL, command, environ) !=
      0) {
    perror("posix_spawnp");
    goto cleanup;
  }

  int status;
  if (waitpid(pid, &status, 0) != pid) {
    perror("waitpid");
    goto cleanup;
  }

  if (WIFEXITED(status)) {
    exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    exit_code = 128 + WTERMSIG(status);
  }

cleanup:
  if (posix_spawn_file_actions_destroy(&file_actions) != 0) {
    perror("posix_spawn_file_actions_destroy");
  }
  return exit_code;
}

bool dbx_proc_exec(char *const command[]) {
  assert(command != NULL && command[0] != NULL);
  execvp(command[0], command);

  // execvp only ever returns if there was an error.
  perror("execvp");
  return false;
}
