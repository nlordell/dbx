// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 Nicholas Rodrigues Lordello <n@lordello.net>

#ifndef _DBX_H_
#define _DBX_H_

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#define COUNTOF(x) ((sizeof(x)) / (sizeof(*x)))
#define HOSTNAME_MAX _POSIX_HOST_NAME_MAX

int dbx_writefile(const char *file, const char *format, ...);
int dbx_readfile(const char *file, char **contents);

void dbx_printerr(const char *format, ...);
void dbx_perror(const char *s, int e);
int dbx_fpath(char path[PATH_MAX], const char *format, ...);

enum dbx_proc_fds {
  DBXFD_NONE = 0,
  DBXFD_STDIN = 1,
  DBXFD_STDOUT = 2,
  DBXFD_STDERR = 4,
  DBXFD_ALL = 7,
};
bool dbx_proc_find(const char *name, char path[PATH_MAX]);
int dbx_proc_run(const char *const command[], enum dbx_proc_fds fds);
int dbx_proc_output(const char *const command[], char *out, int outlen);
bool dbx_proc_exec(const char *const command[]);
#define DBX_CMD(...) ((const char *const[]){__VA_ARGS__, NULL})

bool dbx_ssh_init(const char *name, char ssh_pubkey[PATH_MAX]);
bool dbx_ssh_config(const char *name, const char *hostname, uint16_t port);
bool dbx_ssh_cp(const char *name, const char *from, const char *to);
bool dbx_ssh_sys(const char *name, const char *command);

struct dbx_engine;
struct dbx_engine *dbx_engine_init(void);
bool dbx_engine_create(struct dbx_engine *engine, const char *image,
                       const char *name, const char *ssh_pubkey,
                       uint16_t sshd_port);
bool dbx_engine_start(struct dbx_engine *engine, const char *name,
                      char hostname[HOSTNAME_MAX], uint16_t *sshd_port);
bool dbx_engine_stop(struct dbx_engine *engine, const char *name);
void dbx_engine_destroy(struct dbx_engine *engine);

struct dbx_options {
  char *command;
  char *name;
  char *image;
  struct dbx_options_ports {
    uint16_t container;
    uint16_t host;
  } ports;
  char *post_install;
};
int dbx_create(struct dbx_engine *engine, struct dbx_options const *options);

#endif
