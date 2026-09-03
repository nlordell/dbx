// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 Nicholas Rodrigues Lordello <n@lordello.net>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "dbx.h"

static const char *progname = "dbx";
static void usage(FILE *output) {
  fprintf(
      output,
      "usage: %s [COMMAND] [OPTIONS...] [ARGS...]\n"
      "commands:\n"
      "    create      create a new devbox\n"
      "    enter       opens a shell in the devbox (default)\n"
      "    proxy       proxies a port to the host\n"
      "    start       starts a devbox\n"
      "    stop        stops a devbox\n"
      "options:\n"
      "    -n NAME     name of the devbox container (default: dbx)\n"
      "create options:\n"
      "    -i IMAGE    image for devbox container (default: dbx-next:latest)\n"
      "    -p PORT     host port `sshd` binds to (default: 4222)\n"
      "proxy arguments:\n"
      "    PORT[:HOST] the devbox port to proxy on the host; optionally\n"
      "                specify an alternate host port to bind to\n",
      progname);
}

static int usage_error(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "%s: ", progname);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  return EX_USAGE;
}

static bool parse_port(const char *str, uint16_t *port) {
  char *end = NULL;
  errno = 0;
  long result = strtol(str, &end, 10);

  if (str == end || end[0] != '\0') {
    // not a valid string.
    return false;
  }
  if (errno == ERANGE || result <= 0 || result > UINT16_MAX) {
    // out of valid port range; note that we consider the `0` port as invalid as
    // well, since it does not make sense for the CLI
    return false;
  }

  *port = (uint16_t)result;
  return true;
}

bool proxy_ports_arg(struct dbx_options *options, const char *arg) {
  // TRICK: We make a buffer that fits the largest possible value for the ports
  // and add a sentinel 'X', so that if the `arg` would be too long, we would
  // be left with a dangling 'X' (which is not a valid character for integer
  // parsing) in either `container` or `host`.
  char buffer[14] = "65535:65535\0X";
  strncpy(buffer, arg, sizeof(buffer) - 2);

  char *container = buffer;
  char *host = strchr(buffer, ':');
  if (host != NULL) {
    *(host++) = '\0';
  }

  if (!parse_port(container, &options->ports.container) ||
      (host != NULL && !parse_port(host, &options->ports.host))) {
    return false;
  }

  if (host == NULL) {
    options->ports.host = options->ports.container;
  }

  return true;
}

int dummy_run(struct dbx_options *options) {
  printf("Hello, DBX!\n");
  printf("- command: %s\n", options->command);
  printf("- name:    %s\n", options->name);
  printf("- image:   %s\n", options->image);
  printf("- ports:   %d:%d\n", options->ports.container, options->ports.host);

  char *const ls[] = {"ls", "-la", ".", NULL};
  int exit_code = dbx_proc_run(ls);
  printf("- ls:   %d\n", exit_code);
  dbx_proc_exec(ls);

  return EXIT_SUCCESS;
}

struct command {
  const char *name;
  const char *optstr;
  struct argument {
    const char *name;
    bool (*parse)(struct dbx_options *options, const char *arg);
  } *args;
  int (*run)(struct dbx_options *options);
};

static struct command commands[] = {
    {
        .name = "create",
        .optstr = "i:p:",
        .run = dummy_run,
    },
    {
        .name = "enter",
        .run = dummy_run,
    },
    {
        .name = "proxy",
        .args =
            (struct argument[]){
                {
                    .name = "PORT",
                    .parse = proxy_ports_arg,
                },
                {0},
            },
        .run = dummy_run,
    },
    {
        .name = "start",
        .run = dummy_run,
    },
    {
        .name = "stop",
        .run = dummy_run,
    },
};

int main(int argc, char **argv) {
  struct dbx_options options = {
      .command = "enter",
      .name = "dbx",
      .image = "dbx-next:latest",
      .ports =
          {
              .container = 22,
              .host = 4222,
          },
  };

  if (argc > 0) {
    progname = argv[0];
  }
  if (argc > 1 && *argv[1] != '-') {
    options.command = argv[1];
    optind = 2;
  }

  struct command *command = NULL;
  for (int i = 0; i < (int)COUNTOF(commands); i++) {
    if (strcmp(options.command, commands[i].name) == 0) {
      command = &commands[i];
      break;
    }
  }
  if (command == NULL) {
    return usage_error("unknown command '%s'", options.command);
  }

  char optstr[32] = ":hn:";
  if (command->optstr != NULL) {
    int len = snprintf(optstr, sizeof(optstr), ":hn:%s", command->optstr);
    assert(len < (int)sizeof(optstr));
  }

  int opt;
  while ((opt = getopt(argc, argv, optstr)) != -1) {
    switch (opt) {
    case 'h':
      usage(stdout);
      return EXIT_SUCCESS;
    case 'n':
      options.name = optarg;
      break;
    case 'i':
      options.image = optarg;
      break;
    case 'p':
      if (!parse_port(optarg, &options.ports.host)) {
        return usage_error("invalid port '%s'", optarg);
      }
      break;
    case ':':
      return usage_error("missing value for -%c", optopt);
    case '?':
    default:
      return usage_error("unknown option -%c", optopt);
    }
  }

  int nargs = argc - optind;
  char **args = argv + optind;
  for (int i = 0; i < nargs; i++) {
    const char *value = args[i];
    struct argument *arg = command->args ? &command->args[i] : NULL;
    if (arg == NULL || arg->parse == NULL) {
      return usage_error("unexpected argument '%s'", value);
    }
    if (!arg->parse(&options, value)) {
      return usage_error("invalid %s value '%s'", arg->name, value);
    }
  }
  struct argument *endarg = command->args ? &command->args[nargs] : NULL;
  if (endarg != NULL && endarg->name != NULL) {
    return usage_error("missing argument %s", endarg->name);
  }

  return command->run(&options);
}
