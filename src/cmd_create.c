#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "dbx.h"

int dbx_create(struct dbx_engine *engine, struct dbx_options const *options) {
  char ssh_pubkey[PATH_MAX];
  if (!dbx_ssh_init(options->name, ssh_pubkey)) {
    dbx_printerr("failed to generate SSH key");
    return EX_CANTCREAT;
  }

  if (!dbx_engine_create(engine, options->image, options->name, ssh_pubkey,
                         options->ports.host)) {
    dbx_printerr("failed to create devbox");
    return EX_CANTCREAT;
  }

  int result = EXIT_SUCCESS;
  if (options->post_install != NULL) {
    char hostname[HOSTNAME_MAX];
    uint16_t sshd_port;
    if (!dbx_engine_start(engine, options->name, hostname, &sshd_port)) {
      return EX_CONFIG;
    }

    char tmp[PATH_MAX];
    if (dbx_formatpath(tmp, "%s:/tmp/post-install", options->name) ||
        !dbx_ssh_config(options->name, hostname, sshd_port) ||
        !dbx_ssh_cp(options->name, options->post_install, tmp) ||
        !dbx_ssh_sys(options->name,
                     "chmod +x /tmp/post-install && /tmp/post-install")) {
      result = EX_CONFIG;
    }

    if (!dbx_engine_stop(engine, options->name)) {
      result = EX_CONFIG;
    }
  }

  return result;
}
