#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "dbx.h"

int dbx_create(struct dbx_engine *engine, struct dbx_options const *options) {
  if (!dbx_ssh_init(options->name)) {
    dbx_printerr("failed to generate SSH key");
    return EX_CANTCREAT;
  }

  if (!dbx_engine_create(engine, options->image, options->name,
                         options->ports.host)) {
    dbx_printerr("failed to create devbox");
    return EX_CANTCREAT;
  }

  // TODO: setup and install SSH configuration.
  // TODO: copy over (SCP) and execute post-install script.

  if (options->post_install != NULL) {
    dbx_printerr("-x SCRIPT not yet supported");
    return EX_USAGE;
  }

  return EXIT_SUCCESS;
}
