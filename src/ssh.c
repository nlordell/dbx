#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dbx.h"

static bool get_xdg_dir(char path[PATH_MAX], const char *env, const char *def) {
  int result = -1;

  const char *xdg = getenv(env);
  if (xdg != NULL) {
    result = dbx_fpath(path, "%s/dbx", xdg);
  } else {
    const char *home = getenv("HOME");
    if (home != NULL) {
      result = dbx_fpath(path, "%s/%s/dbx", home, def);
    }
  }

  return result >= 0;
}

static bool get_data_dir(char path[PATH_MAX]) {
  return get_xdg_dir(path, "XDG_DATA_HOME", ".local/share");
}

static bool get_config_dir(char path[PATH_MAX]) {
  return get_xdg_dir(path, "XDG_CONFIG_HOME", ".config");
}

static bool mkdir_p(const char *path) {
  char buffer[PATH_MAX];
  if (dbx_fpath(buffer, "%s/", path) < 0) {
    return false;
  }

  for (char *cursor = strchr(buffer + 1, '/'); cursor;
       cursor = strchr(cursor + 1, '/')) {
    *cursor = '\0';
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
      dbx_perror("mkdir", errno);
      return false;
    }
    *cursor = '/';
  }

  return true;
}

bool dbx_ssh_init(const char *name, char ssh_pubkey[PATH_MAX]) {
  assert(name != NULL && ssh_pubkey != NULL);

  char data_dir[PATH_MAX];
  if (!get_data_dir(data_dir) || !mkdir_p(data_dir)) {
    return false;
  }

  char ssh_id[PATH_MAX];
  if (dbx_fpath(ssh_id, "%s/%s.id_ed25519", data_dir, name) < 0 ||
      dbx_fpath(ssh_pubkey, "%s/%s.id_ed25519.pub", data_dir, name) < 0) {
    return false;
  }
  if (unlink(ssh_id) != 0 && errno != ENOENT) {
    dbx_perror("unlink(ssh-id)", errno);
    return false;
  }

  return dbx_proc_run(DBX_CMD("ssh-keygen", "-C", "dbx", "-f", ssh_id, "-N", "",
                              "-t", "ed25519"),
                      DBXFD_NONE) == 0;
}

bool dbx_ssh_config(const char *name, const char *hostname, uint16_t port) {
  assert(name != NULL && hostname != NULL);

  char data_dir[PATH_MAX];
  char config_dir[PATH_MAX];
  if (!get_data_dir(data_dir) || !get_config_dir(config_dir)) {
    return false;
  }

  char ssh_config[PATH_MAX];
  char ssh_id[PATH_MAX];
  char known_hosts[PATH_MAX];
  char user_config[PATH_MAX];
  if (dbx_fpath(ssh_config, "%s/%s.ssh_config", data_dir, name) < 0 ||
      dbx_fpath(ssh_id, "%s/%s.id_ed25519", data_dir, name) < 0 ||
      dbx_fpath(known_hosts, "%s/%s.known_hosts", data_dir, name) < 0 ||
      dbx_fpath(user_config, "%s/%s.ssh_config", config_dir, name) < 0) {
    return false;
  }

  FILE *f = fopen(ssh_config, "w");
  if (f == NULL) {
    dbx_perror("fopen(ssh-config)", errno);
    return false;
  }

  bool result = true;
  char format[] = "Host %s\n"
                  "\tHostName %s\n"
                  "\tPort %d\n"
                  "\tIdentitiesOnly yes\n"
                  "\tIdentityFile \"%s\"\n"
                  "\tStrictHostKeyChecking no\n"
                  "\tUserKnownHostsFile \"%s\"\n"
                  "\tInclude \"%s\"\n";
  int n = fprintf(f, format, name, hostname, port, ssh_id, known_hosts,
                  user_config);
  if (n < 0) {
    dbx_perror("fprintf(ssh-config)", errno);
    result = false;
  }

  if (fclose(f) != 0) {
    dbx_perror("fclose(ssh-config)", errno);
    result = false;
  }

  return result;
}

bool dbx_ssh_cp(const char *name, const char *from, const char *to) {
  assert(name != NULL && from != NULL && to != NULL);

  char data_dir[PATH_MAX];
  if (!get_data_dir(data_dir)) {
    return false;
  }

  char ssh_config[PATH_MAX];
  if (dbx_fpath(ssh_config, "%s/%s.ssh_config", data_dir, name) < 0) {
    return false;
  }

  return dbx_proc_run(DBX_CMD("scp", "-F", ssh_config, from, to),
                      DBXFD_STDERR) == 0;
}

bool dbx_ssh_sys(const char *name, const char *command) {
  assert(name != NULL && command != NULL);

  char data_dir[PATH_MAX];
  if (!get_data_dir(data_dir)) {
    return false;
  }

  char ssh_config[PATH_MAX];
  if (dbx_fpath(ssh_config, "%s/%s.ssh_config", data_dir, name) < 0) {
    return false;
  }

  return dbx_proc_run(DBX_CMD("ssh", "-F", ssh_config, name, command),
                      DBXFD_ALL) == 0;
}
