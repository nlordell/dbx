#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbx.h"

struct dbx_engine {
  char container[PATH_MAX];
};

struct dbx_engine *dbx_engine_init(void) {
  struct dbx_engine *engine = malloc(sizeof(struct dbx_engine));
  if (engine == NULL) {
    dbx_perror("calloc", errno);
    return NULL;
  }

  int result = dbx_proc_find("container", engine->container);
  if (result) {
    dbx_perror("unable to find `container` executable", result);
    goto error;
  }

  int status_exit;
  const char *const status[] = {"system", "status", NULL};
  result = dbx_proc_run(status, DBXFD_NONE, &status_exit);
  if (result) {
    dbx_perror("unable to check `container` status", result);
    goto error;
  }
  if (status_exit != 0) {
    dbx_printerr("container engine must started with `container system start`");
    goto error;
  }

  return engine;

error:
  free(engine);
  return NULL;
}

bool dbx_engine_create(struct dbx_engine *engine, const char *image,
                       const char *name, const char *ssh_pubkey,
                       uint16_t sshd_port) {
  // The container machine has its own network and does not bind-mount a port
  // on the host. We can therefore ignore the `sshd_port` setting.
  (void)sshd_port;

  int create_exit;
  const char *const status[] = {
      engine->container, "machine",           "create", "--name", name,
      "--no-boot",       "--home-mount=none", image,    NULL};
  int result = dbx_proc_run(status, DBXFD_STDOUT | DBXFD_STDERR, &status_exit);
  if (result) {
    dbx_perror("unable to create `container`", result);
    return false;
  }
  if (create_exit != 0) {
    dbx_printerr("container creation failed");
    return false;
  }

  // Unfortunately, there is no way to access files from the container machine
  // while it is stopped; instead boot it and run a small script that can
  // authorize our SSH key.
  char *key;
  result = dbx_readfile(ssh_pubkey, &key);
  if (result) {
    dbx_perror("unable to read SSH public key", result);
    return false;
  }
  char script[1024];
  int n = snprintf(script, sizeof(script),
                   "echo '%s' > /etc/ssh/authorized_keys", key);
  if (n < 0 || n >= (int)sizeof(script)) {
    dbx_printerr("invalid SSH public key format");
    return false;
  }

  // Note that `container machine run` _requires_ `stdin` for some reason (I
  // assume it is to attach to the VM process), make sure to not close it.

  if (dbx_proc_run(DBX_CMD(engine->container, "machine", "run", "--name", name,
                           "--root", script),
                   DBXFD_ALL) != 0) {
    return false;
  }

  // Make sure to shut down the machine, this will restart `SSHD` to ensure the
  // port configuration is correctly picked up.
  return dbx_engine_stop(engine, name);
}

static bool read_ipaddr(char *json, char hostname[HOSTNAME_MAX]) {
  assert(json != NULL && hostname != NULL);

  // We only need to extract the `ipAddress` field from the JSON, so abuse
  // `strtok_r` to read tokens until we get there. Assume that the JSON is valid
  // and that there is only a single `ipAddress` field in the document.

  char *tok, *save;
  const char *sep = " \t\r\n\":";
  for (tok = strtok_r(json, sep, &save); tok != NULL;
       tok = strtok_r(NULL, sep, &save)) {
    if (strcmp(tok, "ipAddress") == 0) {
      // The next token is going to be the IP address value.
      tok = strtok_r(NULL, sep, &save);
      break;
    }
  }

  if (tok == NULL) {
    return false;
  }

  int n = snprintf(hostname, HOSTNAME_MAX, "%s", tok);
  return n >= 0 && n < HOSTNAME_MAX;
}

bool dbx_engine_start(struct dbx_engine *engine, const char *name,
                      char hostname[HOSTNAME_MAX], uint16_t *sshd_port) {
  // The only way to start a comand is with `run` - so run something that
  // does nothing but return `0` exit code.
  if (dbx_proc_run(DBX_CMD(engine->container, "machine", "run", "--name", name,
                           "--root", "true"),
                   DBXFD_STDERR) != 0) {
    return false;
  }

  if (hostname != NULL) {
    char json[8192];
    if (dbx_proc_output(DBX_CMD(engine->container, "machine", "inspect", name),
                        json, sizeof(json)) != 0) {
      return false;
    }

    if (!read_ipaddr(json, hostname)) {
      return false;
    }
  }

  if (sshd_port != NULL) {
    *sshd_port = 22;
  }

  return true;
}

bool dbx_engine_stop(struct dbx_engine *engine, const char *name) {
  return dbx_proc_run(DBX_CMD(engine->container, "machine", "stop", name),
                      DBXFD_STDERR) == 0;
}

void dbx_engine_destroy(struct dbx_engine *engine) { free(engine); }
