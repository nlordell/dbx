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
      "    -x SCRIPT   post installation script\n"
      "proxy arguments:\n"
      "    PORT[:HOST] the devbox port to proxy on the host; optionally\n"
      "                specify an alternate host port to bind to\n",
      progname);
}

void dbx_printerr(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "%s: ", progname);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}

void dbx_perror(const char *s, int e) {
  dbx_printerr("%s: %s", s, strerror(e));
}

#define USAGE_ERROR(...) (dbx_printerr(__VA_ARGS__), EX_USAGE)

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

int dummy_run(struct dbx_engine *engine, struct dbx_options const *options) {
  (void)engine;

  printf("Hello, DBX!\n");
  printf("- command: %s\n", options->command);
  printf("- name:    %s\n", options->name);
  printf("- image:   %s\n", options->image);
  printf("- ports:   %d:%d\n", options->ports.container, options->ports.host);

  char container[PATH_MAX];
  if (!dbx_proc_find("container", container)) {
    fprintf(stderr, "dbx: can't find `container`.\n");
    return EXIT_FAILURE;
  }

  const char *const cmd[] = {container, "--version", NULL};
  int exit_code = dbx_proc_run(cmd, DBXP_NONE);
  printf("- cmd:     %s:%d\n", container, exit_code);
  dbx_proc_exec(cmd);

  return EXIT_SUCCESS;
}

struct command {
  const char *name;
  const char *optstr;
  struct argument {
    const char *name;
    bool (*parse)(struct dbx_options *options, const char *arg);
  } *args;
  int (*run)(struct dbx_engine *engine, struct dbx_options const *options);
};

static struct command commands[] = {
    {
        .name = "create",
        .optstr = "i:p:x:",
        .run = dbx_create,
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
      .image = "ghcr.io/nlordell/dbx:latest",
      .ports =
          {
              .container = 22,
              .host = 4222,
          },
      .post_install = NULL,
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
    return USAGE_ERROR("unknown command '%s'", options.command);
  }

  char optstr[32] = ":hvn:";
  if (command->optstr != NULL) {
    int len = snprintf(optstr, sizeof(optstr), ":hvn:%s", command->optstr);
    assert(len < (int)sizeof(optstr));
  }

  int opt;
  while ((opt = getopt(argc, argv, optstr)) != -1) {
    switch (opt) {
    case 'h':
      usage(stdout);
      return EXIT_SUCCESS;
    case 'v':
      printf("%s 0.0.1\n", progname);
      return EXIT_SUCCESS;
    case 'n':
      options.name = optarg;
      break;
    case 'i':
      options.image = optarg;
      break;
    case 'p':
      if (!parse_port(optarg, &options.ports.host)) {
        return USAGE_ERROR("invalid port '%s'", optarg);
      }
      break;
    case 'x':
      options.post_install = optarg;
      break;
    case ':':
      return USAGE_ERROR("missing value for -%c", optopt);
    case '?':
    default:
      return USAGE_ERROR("unknown option -%c", optopt);
    }
  }

  int nargs = argc - optind;
  char **args = argv + optind;
  for (int i = 0; i < nargs; i++) {
    const char *value = args[i];
    struct argument *arg = command->args ? &command->args[i] : NULL;
    if (arg == NULL || arg->parse == NULL) {
      return USAGE_ERROR("unexpected argument '%s'", value);
    }
    if (!arg->parse(&options, value)) {
      return USAGE_ERROR("invalid %s value '%s'", arg->name, value);
    }
  }
  struct argument *endarg = command->args ? &command->args[nargs] : NULL;
  if (endarg != NULL && endarg->name != NULL) {
    return USAGE_ERROR("missing argument %s", endarg->name);
  }

  struct dbx_engine *engine = dbx_engine_init();
  if (engine == NULL) {
    dbx_printerr("failed to initialize container engine");
    return EX_UNAVAILABLE;
  }
  int result = command->run(engine, &options);
  dbx_engine_destroy(engine);

  return result;
}
