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
#include <sys/fcntl.h>
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

static char **dup_argv(const char *const command[]) {
  if (command == NULL || command[0] == NULL) {
    dbx_perror("dup_argv", EINVAL);
    return NULL;
  }

  size_t count = 0;
  size_t size = 0;
  for (; command[count] != NULL; count++) {
    size += strlen(command[count]) + 1;
  }

  // Allocate a single buffer for the `argv` array and its data. Include space
  // for the extra NULL pointer at the end of the array.
  char **argv = malloc((count + 1) * sizeof(char *) + size);
  if (argv == NULL) {
    dbx_perror("malloc", ENOMEM);
    return NULL;
  }

  char *buf = (char *)(argv + count + 1);
  for (size_t i = 0; i < count; i++) {
    argv[i] = buf;
    buf = stpcpy(buf, command[i]) + 1;
  }

  argv[count] = NULL;
  return argv;
}

int dbx_proc_run(const char *const command[], enum dbx_proc_fds fds) {
  int exit_code = -1;

  char **argv = dup_argv(command);
  if (argv == NULL) {
    goto exit;
  }

  posix_spawn_file_actions_t file_actions;
  int result = posix_spawn_file_actions_init(&file_actions);
  if (result != 0) {
    dbx_perror("spawn_file_actions_init", result);
    goto cleanup_argv;
  }

  if ((fds & DBXFD_STDIN) == 0) {
    result = posix_spawn_file_actions_addopen(&file_actions, STDIN_FILENO,
                                              _PATH_DEVNULL, O_RDONLY, 0);
    if (result != 0) {
      dbx_perror("posix_spawn_file_actions_addopen(stdin)", result);
      goto cleanup_fa;
    }
  }

  if ((fds & DBXFD_STDOUT) == 0) {
    result = posix_spawn_file_actions_addopen(&file_actions, STDOUT_FILENO,
                                              _PATH_DEVNULL, O_WRONLY, 0);
    if (result != 0) {
      dbx_perror("posix_spawn_file_actions_addopen(stdout)", result);
      goto cleanup_fa;
    }
  }

  if ((fds & DBXFD_STDERR) == 0) {
    result = posix_spawn_file_actions_addopen(&file_actions, STDERR_FILENO,
                                              _PATH_DEVNULL, O_WRONLY, 0);
    if (result != 0) {
      dbx_perror("posix_spawn_file_actions_addopen(stderr)", result);
      goto cleanup_fa;
    }
  }

  pid_t pid;
  result = posix_spawnp(&pid, argv[0], &file_actions, NULL, argv, environ);
  if (result != 0) {
    dbx_perror("posix_spawnp", result);
    goto cleanup_fa;
  }

  int status;
  while (waitpid(pid, &status, 0) != pid) {
    if (errno == EINTR) {
      continue;
    }

    dbx_perror("waitpid", errno);
    goto cleanup_fa;
  }

  if (WIFEXITED(status)) {
    exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    exit_code = 128 + WTERMSIG(status);
  }

cleanup_fa:
  result = posix_spawn_file_actions_destroy(&file_actions);
  if (result != 0) {
    dbx_perror("posix_spawn_file_actions_destroy", result);
  }

cleanup_argv:
  free(argv);

exit:
  return exit_code;
}

int dbx_proc_output(const char *const command[], char *out, int outlen) {
  if (out == NULL || outlen < 1) {
    return -1;
  }

  size_t total = 0;
  int exit_code = -1;

  char **argv = dup_argv(command);
  if (argv == NULL) {
    goto exit;
  }

  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) {
    dbx_perror("pipe", errno);
    goto cleanup_argv;
  }
  int read_fd = pipe_fds[0];
  int write_fd = pipe_fds[1];

  if (write_fd == STDOUT_FILENO) {
    dbx_printerr("unexpected pipe file descriptor values");
    goto cleanup_fds;
  }

  posix_spawn_file_actions_t file_actions;
  int result = posix_spawn_file_actions_init(&file_actions);
  if (result != 0) {
    dbx_perror("spawn_file_actions_init", result);
    goto cleanup_fds;
  }

  result = posix_spawn_file_actions_addclose(&file_actions, read_fd);
  if (result != 0) {
    dbx_perror("posix_spawn_file_actions_addclose(read_fd)", result);
    goto cleanup_fa;
  }

  result =
      posix_spawn_file_actions_adddup2(&file_actions, write_fd, STDOUT_FILENO);
  if (result != 0) {
    dbx_perror("posix_spawn_file_actions_adddup2(stdout)", result);
    goto cleanup_fa;
  }

  result = posix_spawn_file_actions_addclose(&file_actions, write_fd);
  if (result != 0) {
    dbx_perror("posix_spawn_file_actions_addclose(write_fd)", result);
    goto cleanup_fa;
  }

  pid_t pid;
  result = posix_spawnp(&pid, command[0], &file_actions, NULL, argv, environ);
  if (result != 0) {
    dbx_perror("posix_spawnp", result);
    goto cleanup_fa;
  }

  if (close(write_fd) != 0) {
    dbx_perror("close", errno);
  }
  write_fd = -1;

  size_t limit = outlen - 1;
  while (total < limit) {
    ssize_t n = read(read_fd, out + total, limit - total);
    if (n == 0) {
      break;
    } else if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      dbx_perror("read", errno);
      result = errno;
      goto cleanup_pid;
    }
    total += (size_t)n;
  }
  out[total] = '\0';

cleanup_pid:
  if (close(read_fd) != 0) {
    dbx_perror("close", errno);
  }
  read_fd = -1;

  int status;
  while (waitpid(pid, &status, 0) != pid) {
    if (errno == EINTR) {
      continue;
    }

    dbx_perror("waitpid", errno);
    goto cleanup_fa;
  }

  if (result == 0) {
    if (WIFEXITED(status)) {
      exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      exit_code = 128 + WTERMSIG(status);
    }
  }

cleanup_fa:
  result = posix_spawn_file_actions_destroy(&file_actions);
  if (result != 0) {
    dbx_perror("posix_spawn_file_actions_destroy", result);
  }

cleanup_fds:
  if (write_fd >= 0 && close(write_fd) != 0) {
    dbx_perror("close", errno);
  }
  if (read_fd >= 0 && close(read_fd) != 0) {
    dbx_perror("close", errno);
  }

cleanup_argv:
  free(argv);

exit:
  out[total] = '\0';
  return exit_code;
}

bool dbx_proc_exec(const char *const command[]) {
  char **argv = dup_argv(command);
  if (argv == NULL) {
    return false;
  }

  execvp(argv[0], argv);

  // execvp only ever returns if there was an error.
  dbx_perror("execv", errno);

  free(argv);
  return false;
}
