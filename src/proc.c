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

int dbx_proc_find(const char *name, char file[PATH_MAX]) {
  if (name == NULL || name[0] == '\0' || strchr(name, '/') != NULL ||
      file == NULL) {
    return EINVAL;
  }

  char *path = strdup(get_path());
  if (path == NULL) {
    return ENOMEM;
  }

  char *pos = path;
  bool found = false;
  for (const char *dir = next_path_segment(&pos); dir != NULL;
       dir = next_path_segment(&pos)) {
    char candidate[PATH_MAX];
    if (dbx_formatpath(candidate, "%s/%s", dir, name)) {
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
  size_t count = 0;
  size_t size = 0;
  for (; command[count] != NULL; count++) {
    size += strlen(command[count]) + 1;
  }

  // Allocate a single buffer for the `argv` array and its data. Include space
  // for the extra NULL pointer at the end of the array.
  char **argv = malloc((count + 1) * sizeof(char *) + size);
  if (argv == NULL) {
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

int dbx_proc_run(const char *const command[], enum dbx_proc_fds fds,
                 int *exit_code) {
  if (command == NULL || command[0] == NULL || exit_code == NULL) {
    return EINVAL;
  }

  int result = 0;
  char **argv = dup_argv(command);
  if (argv == NULL) {
    result = ENOMEM;
    goto exit;
  }

  posix_spawn_file_actions_t file_actions;
  result = posix_spawn_file_actions_init(&file_actions);
  if (result != 0) {
    goto cleanup_argv;
  }

  if ((fds & DBXFD_STDIN) == 0) {
    result = posix_spawn_file_actions_addopen(&file_actions, STDIN_FILENO,
                                              _PATH_DEVNULL, O_RDONLY, 0);
    if (result != 0) {
      goto cleanup_fa;
    }
  }

  if ((fds & DBXFD_STDOUT) == 0) {
    result = posix_spawn_file_actions_addopen(&file_actions, STDOUT_FILENO,
                                              _PATH_DEVNULL, O_WRONLY, 0);
    if (result != 0) {
      goto cleanup_fa;
    }
  }

  if ((fds & DBXFD_STDERR) == 0) {
    result = posix_spawn_file_actions_addopen(&file_actions, STDERR_FILENO,
                                              _PATH_DEVNULL, O_WRONLY, 0);
    if (result != 0) {
      goto cleanup_fa;
    }
  }

  pid_t pid;
  result = posix_spawnp(&pid, argv[0], &file_actions, NULL, argv, environ);
  if (result != 0) {
    goto cleanup_fa;
  }

  int status;
  while (waitpid(pid, &status, 0) != pid) {
    if (errno == EINTR) {
      continue;
    }

    result = errno;
    goto cleanup_fa;
  }

  if (WIFEXITED(status)) {
    *exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    *exit_code = 128 + WTERMSIG(status);
  } else {
    *exit_code = -1;
  }

cleanup_fa:;
  int destroy_result = posix_spawn_file_actions_destroy(&file_actions);
  if (result == 0) {
    result = destroy_result;
  }

cleanup_argv:
  free(argv);

exit:
  return result;
}

static int read_all(int fd, char **out, size_t *outlen) {
  size_t cap = 4096;
  size_t len = 0;
  char *buf = malloc(cap);
  if (buf == NULL) {
    return ENOMEM;
  }

  for (;;) {
    if (len == cap) {
      size_t new_cap = cap * 2;
      if (new_cap >= SSIZE_MAX || new_cap <= cap) {
        free(buf);
        return ENOMEM;
      }

      char *new_buf = realloc(buf, new_cap);
      if (new_buf == NULL) {
        free(buf);
        return ENOMEM;
      }

      buf = new_buf;
      cap = new_cap;
    }

    ssize_t n = read(fd, buf + len, cap - len);
    if (n == 0) {
      break;
    } else if (n < 0) {
      if (errno == EINTR) {
        continue;
      }

      int result = errno;
      free(buf);
      return result;
    }

    len += (size_t)n;
  }

  char *final = realloc(buf, len + 1);
  if (!final) {
    free(buf);
    return ENOMEM;
  }
  final[len] = '\0';

  *out = final;
  *outlen = len;
  return 0;
}

int dbx_proc_output(const char *const command[], int *exit_code, char **out,
                    size_t *outlen) {
  if (command == NULL || command[0] == NULL || exit_code == NULL ||
      out == NULL || outlen == NULL) {
    return EINVAL;
  }

  int result = 0;
  char *buf = NULL;

  char **argv = dup_argv(command);
  if (argv == NULL) {
    result = ENOMEM;
    goto exit;
  }

  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) {
    result = errno;
    goto cleanup_argv;
  }
  int read_fd = pipe_fds[0];
  int write_fd = pipe_fds[1];

  if (write_fd == STDOUT_FILENO) {
    result = EINVAL;
    goto cleanup_fds;
  }

  posix_spawn_file_actions_t file_actions;
  result = posix_spawn_file_actions_init(&file_actions);
  if (result != 0) {
    goto cleanup_fds;
  }

  result = posix_spawn_file_actions_addclose(&file_actions, read_fd);
  if (result != 0) {
    goto cleanup_fa;
  }

  result =
      posix_spawn_file_actions_adddup2(&file_actions, write_fd, STDOUT_FILENO);
  if (result != 0) {
    goto cleanup_fa;
  }

  result = posix_spawn_file_actions_addclose(&file_actions, write_fd);
  if (result != 0) {
    goto cleanup_fa;
  }

  pid_t pid;
  result = posix_spawnp(&pid, command[0], &file_actions, NULL, argv, environ);
  if (result != 0) {
    goto cleanup_fa;
  }

  if (close(write_fd) != 0) {
    result = errno;
  }
  write_fd = -1;
  if (result != 0) {
    goto cleanup_pid;
  }

  size_t buflen;
  result = read_all(read_fd, &buf, &buflen);

cleanup_pid:
  if (close(read_fd) != 0 && result == 0) {
    result = errno;
  }
  read_fd = -1;

  int status;
  while (waitpid(pid, &status, 0) != pid) {
    if (errno == EINTR) {
      continue;
    }

    if (result == 0) {
      result = errno;
    }
    goto cleanup_fa;
  }

cleanup_fa:;
  int destroy_result = posix_spawn_file_actions_destroy(&file_actions);
  if (result == 0) {
    result = destroy_result;
  }

cleanup_fds:
  if (write_fd >= 0 && close(write_fd) != 0 && result == 0) {
    result = errno;
  }
  if (read_fd >= 0 && close(read_fd) != 0 && result == 0) {
    result = errno;
  }

cleanup_argv:
  free(argv);

exit:
  if (result == 0) {
    if (WIFEXITED(status)) {
      *exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      *exit_code = 128 + WTERMSIG(status);
    } else {
      *exit_code = -1;
    }
    *out = buf;
    *outlen = buflen;
  } else {
    free(buf);
  }

  return result;
}

int dbx_proc_exec(const char *const command[]) {
  if (command == NULL || command[0] == NULL) {
    return EINVAL;
  }

  char **argv = dup_argv(command);
  if (argv == NULL) {
    return ENOMEM;
  }

  execvp(argv[0], argv);

  // execvp only ever returns if there was an error.
  free(argv);
  return errno;
}
