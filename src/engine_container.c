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
  struct dbx_engine *engine = calloc(1, sizeof(struct dbx_engine));
  if (engine == NULL) {
    dbx_perror("calloc", errno);
    return NULL;
  }

  if (!dbx_proc_find("container", engine->container)) {
    goto error;
  }

  if (dbx_proc_run(DBX_CMD(engine->container, "system", "status"),
                   DBXFD_NONE) != 0) {
    dbx_printerr("container engine must started with `container system start`");
    goto error;
  }

  return engine;

error:
  free(engine);
  return NULL;
}

#define PUBKEY_MAX 256

static int read_pubkey(const char *file, char key[PUBKEY_MAX]) {
  FILE *f = fopen(file, "rb");
  if (!f) {
    dbx_perror("fopen(ssh-pubkey)", errno);
    return -1;
  }

  int total = 0;
  while (total < PUBKEY_MAX) {
    size_t n = fread(key + total, 1, PUBKEY_MAX - total, f);
    if (n == 0) {
      if (!feof(f)) {
        dbx_printerr("failed to read %s", file);
        total = -1;
      }
      break;
    }
    total += n;
  }

  if (--total >= 0 && total < PUBKEY_MAX && key[total] == '\n') {
    key[total] = '\0';
  } else {
    total = -1;
  }

  if (fclose(f) != 0) {
    dbx_perror("fclose(ssh-pubkey)", errno);
    total = -1;
  };
  return total;
}

bool dbx_engine_create(struct dbx_engine *engine, const char *image,
                       const char *name, const char *ssh_pubkey,
                       uint16_t sshd_port) {
  // The container machine has its own network and does not bind-mount a port
  // on the host. We can therefore ignore the `sshd_port` setting.
  (void)sshd_port;

  if (dbx_proc_run(DBX_CMD(engine->container, "machine", "create", "--name",
                           name, "--no-boot", "--home-mount=none", image),
                   DBXFD_STDOUT | DBXFD_STDERR) != 0) {
    return false;
  }

  // Unfortunately, there is no way to access files from the container machine
  // while it is stopped; instead boot it and run a small script that can
  // authorize our SSH key.
  char key[PUBKEY_MAX];
  if (read_pubkey(ssh_pubkey, key) < 0) {
    return false;
  }
  char script[1024];
  int n = snprintf(script, sizeof(script),
                   "echo '%s' > /etc/ssh/authorized_keys", key);
  if (n < 0 || n >= (int)sizeof(script)) {
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
