// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 Nicholas Rodrigues Lordello <n@lordello.net>

#ifndef _DBX_H_
#define _DBX_H_

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COUNTOF(x) ((sizeof(x)) / (sizeof(*x)))

void dbx_printerr(const char *format, ...);
void dbx_perror(const char *s, int e);

enum dbx_proc_output {
  DBXP_NONE = 0,
  DBXP_STDOUT = 1,
  DBXP_STDERR = 2,
  DBXP_ALL = 3,
};
bool dbx_proc_find(const char *name, char path[PATH_MAX]);
int dbx_proc_run(const char *const command[], enum dbx_proc_output out);
bool dbx_proc_exec(const char *const command[]);

bool dbx_ssh_init(const char *name);
bool dbx_ssh_set_hostname(const char *name, const char *hostname);

struct dbx_engine;
struct dbx_engine *dbx_engine_init(void);
bool dbx_engine_create(struct dbx_engine *engine, const char *image,
                       const char *name, uint16_t sshd_port);
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
