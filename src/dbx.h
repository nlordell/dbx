// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 Nicholas Rodrigues Lordello <n@lordello.net>

#ifndef _DBX_H_
#define _DBX_H_

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct dbx_options {
  char *command;
  char *name;
  char *image;
  struct dbx_options_ports {
    uint16_t container;
    uint16_t host;
  } ports;
};

#define COUNTOF(x) ((sizeof(x)) / (sizeof(*x)))

void dbx_printerr(const char *format, ...);
void dbx_perror(const char *s, int e);

bool dbx_proc_find(const char *name, char path[PATH_MAX]);
int dbx_proc_run(char *const command[]);
bool dbx_proc_exec(char *const command[]);

#endif
