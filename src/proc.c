// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 Nicholas Rodrigues Lordello <n@lordello.net>

#include <assert.h>
#include <fcntl.h>
#include <paths.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dbx.h"

extern char **environ;

static const char *get_path(void) {
  const char *path = getenv("PATH");
  if (path == NULL || path[0] == '\0') {
    // In case `PATH` is not specified, use the default search path.
    return _PATH_DEFPATH;
  }
  return path;
}

static const char *next_path_segment(char **path) {
  char *segment = *path;
  if (segment != NULL) {
    *path = strchr(segment, ':');
    if (*path != NULL) {
      *((*path)++) = '\0';
    }

    if (segment[0] == '\0') {
      // POSIX defines an empty PATH segment to mean "current directory"
      segment = ".";
    }
  }

  return segment;
}

bool dbx_proc_find(const char *name, char file[PATH_MAX]) {
  assert(name != NULL && name[0] != '\0' && strchr(name, '/') == NULL &&
         file != NULL);

  char *path = strdup(get_path());
  if (path == NULL) {
    dbx_perror("strdup", ENOMEM);
    return false;
  }

  char *pos = path;
  bool found = false;
  for (const char *dir = next_path_segment(&pos); dir != NULL;
       dir = next_path_segment(&pos)) {
    char candidate[PATH_MAX];
    int n = snprintf(candidate, sizeof(candidate), "%s/%s", dir, name);
    if (n < 0 || n >= (int)sizeof(candidate)) {
      continue;
    }

    struct stat st;
    if (realpath(candidate, file) != NULL && access(file, X_OK) == 0 &&
        stat(file, &st) == 0 && S_ISREG(st.st_mode)) {
      found = true;
      break;
    }
  }

  free(path);
  return found;
}

int dbx_proc_run(const char *const command[], enum dbx_proc_output out) {
  assert(command != NULL && command[0] != NULL);

  int result;
  posix_spawn_file_actions_t file_actions;
  if ((result = posix_spawn_file_actions_init(&file_actions)) != 0) {
    dbx_perror("spawn_file_actions_init", result);
    return -1;
  }

  int exit_code = -1;
  result = posix_spawn_file_actions_addclose(&file_actions, STDIN_FILENO);
  if (result != 0) {
    dbx_perror("posix_spawn_file_actions_addclose(stdin)", result);
    goto cleanup;
  }

  if ((out & DBXP_STDOUT) == 0) {
    result = posix_spawn_file_actions_addopen(&file_actions, STDOUT_FILENO,
                                              _PATH_DEVNULL, O_WRONLY, 0);
    if (result != 0) {
      dbx_perror("posix_spawn_file_actions_addopen(stdout)", result);
      goto cleanup;
    }
  }

  if ((out & DBXP_STDERR) == 0) {
    result = posix_spawn_file_actions_addopen(&file_actions, STDERR_FILENO,
                                              _PATH_DEVNULL, O_WRONLY, 0);
    if (result != 0) {
      dbx_perror("posix_spawn_file_actions_addopen(stderr)", result);
      goto cleanup;
    }
  }

  // POSIX guarantees that the pointer is not modified, so the cast is safe.
  char *const *argv = (char *const *)command;

  pid_t pid;
  result = posix_spawnp(&pid, command[0], &file_actions, NULL, argv, environ);
  if (result != 0) {
    dbx_perror("posix_spawnp", result);
    goto cleanup;
  }

  int status;
  if (waitpid(pid, &status, 0) != pid) {
    dbx_perror("waitpid", errno);
    goto cleanup;
  }

  if (WIFEXITED(status)) {
    exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    exit_code = 128 + WTERMSIG(status);
  }

cleanup:
  result = posix_spawn_file_actions_destroy(&file_actions);
  if (result != 0) {
    dbx_perror("posix_spawn_file_actions_destroy", result);
    // Don't change the exit code on cleanup failure: issues cleaning up do not
    // change the status of the child process's execution.
  }
  return exit_code;
}

bool dbx_proc_exec(const char *const command[]) {
  assert(command != NULL && command[0] != NULL);

  // POSIX guarantees that the pointer is not modified, so the cast is safe.
  char *const *argv = (char *const *)command;

  execvp(command[0], argv);

  // execvp only ever returns if there was an error.
  dbx_perror("execv", errno);
  return false;
}
