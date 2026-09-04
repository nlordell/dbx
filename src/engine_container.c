#include <errno.h>
#include <stdlib.h>

#include "dbx.h"

struct dbx_engine {
  char container[PATH_MAX];
};

struct dbx_engine *dbx_engine_init(void) {
  struct dbx_engine *engine = calloc(1, sizeof(struct dbx_engine));
  if (engine == NULL) {
    dbx_perror("calloc", errno);
    return NULL;
  }

  if (!dbx_proc_find("container", engine->container)) {
    goto error;
  }

  const char *const status[] = {engine->container, "system", "status", NULL};
  if (dbx_proc_run(status, DBXP_NONE) != 0) {
    dbx_printerr("container engine must started with `container system start`");
    goto error;
  }

  return engine;

error:
  free(engine);
  return NULL;
}

bool dbx_engine_create(struct dbx_engine *engine, const char *image,
                       const char *name, uint16_t sshd_port) {
  const char *const create[] = {
      engine->container, "machine",           "create", "--name", name,
      "--no-boot",       "--home-mount=none", image,    NULL};
  if (dbx_proc_run(create, DBXP_ALL) != 0) {
    return false;
  }

  // TODO(nlordell): Set the SSHD port
  // TODO(nlordell): Copy over the SSH key.

  return true;
}

void dbx_engine_destroy(struct dbx_engine *engine) { free(engine); }
