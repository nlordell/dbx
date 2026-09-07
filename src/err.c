#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbx.h"

void dbx_printerr(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "%s: ", progname);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}

void dbx_perror(const char *s, int e) {
  fprintf(stderr, "%s: %s\n", s, strerror(e));
}
