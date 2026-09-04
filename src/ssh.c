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

static bool format_path(char path[PATH_MAX], const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  int n = vsnprintf(path, PATH_MAX, format, ap);
  va_end(ap);

  return n >= 0 && n < PATH_MAX;
}

static bool get_xdg_dir(char path[PATH_MAX], const char *env, const char *def) {
  bool result = false;

  const char *xdg = getenv(env);
  if (xdg != NULL) {
    result = format_path(path, "%s/dbx", xdg);
  } else {
    const char *home = getenv("HOME");
    if (home != NULL) {
      result = format_path(path, "%s/%s/dbx", home, def);
    }
  }

  return result;
}

static bool mkdir_p(const char *path) {
  char buffer[PATH_MAX];
  if (!format_path(buffer, "%s/", path)) {
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

bool dbx_ssh_init(const char *name) {
  char data_dir[PATH_MAX];
  if (!get_xdg_dir(data_dir, "XDG_DATA_HOME", ".local/share") ||
      !mkdir_p(data_dir)) {
    return false;
  }

  char ssh_id[PATH_MAX];
  if (!format_path(ssh_id, "%s/%s.id_ed25519", data_dir, name)) {
    return false;
  }
  if (unlink(ssh_id) != 0 && errno != ENOENT) {
    dbx_perror("unlink(ssh-id)", errno);
    return false;
  }

  const char *const keygen[] = {"ssh-keygen", "-C", name, "-f",      ssh_id,
                                "-N",         "",   "-t", "ed25519", NULL};
  return dbx_proc_run(keygen, DBXP_ALL) == 0;
}

bool dbx_ssh_set_hostname(const char *name, const char *hostname) {
  char data_dir[PATH_MAX];
  char config_dir[PATH_MAX];
  if (!get_xdg_dir(data_dir, "XDG_DATA_HOME", ".local/share") ||
      !get_xdg_dir(config_dir, "XDG_CONFIG_HOME", ".config")) {
    return false;
  }

  char ssh_config[PATH_MAX];
  char ssh_id[PATH_MAX];
  char known_hosts[PATH_MAX];
  char user_config[PATH_MAX];
  if (!format_path(ssh_config, "%s/%s.ssh_config", data_dir, name) ||
      !format_path(ssh_id, "%s/%s.id_ed25519", data_dir, name) ||
      !format_path(known_hosts, "%s/%s.known_hosts", data_dir, name) ||
      !format_path(user_config, "%s/%s.ssh_config", config_dir, name)) {
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
                  "\tIdentitiesOnly yes\n"
                  "\tIdentityFile \"%s\"\n"
                  "\tStrictHostKeyChecking no\n"
                  "\tUserKnownHostsFile \"%s\"\n"
                  "\tInclude \"%s\"\n";
  int n = fprintf(f, format, name, hostname, ssh_id, known_hosts, user_config);
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
