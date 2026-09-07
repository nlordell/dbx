#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "dbx.h"

int dbx_formatpath(char path[PATH_MAX], const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  int n = vsnprintf(path, PATH_MAX, format, ap);
  va_end(ap);
  if (n < 0 || n >= PATH_MAX) {
    return ENAMETOOLONG;
  }
  return 0;
}

int dbx_writefile(const char *file, const char *format, ...) {
  if (file == NULL || format == NULL) {
    return EINVAL;
  }

  FILE *f = fopen(file, "w");
  if (f == NULL) {
    goto error;
  }

  errno = 0;
  va_list ap;
  va_start(ap, format);
  int n = vfprintf(f, format, ap);
  va_end(ap);

  if (n < 0) {
    goto error;
  }
  if (fclose(f) != 0) {
    f = NULL;
    goto error;
  }

  return 0;

error:;
  int result = errno;
  if (result == 0) {
    result = EIO;
  }

  if (f != NULL) {
    fclose(f);
  }
  return result;
}

int dbx_readfile(const char *file, char **contents) {
  if (file == NULL || contents == NULL) {
    return EINVAL;
  }

  char *buf = NULL;
  FILE *f = fopen(file, "r");
  if (f == NULL) {
    goto error;
  }

  size_t end;
  {
    long endl;
    if (fseek(f, 0, SEEK_END) || (endl = ftell(f)) < 0 ||
        fseek(f, 0, SEEK_SET)) {
      goto error;
    }
    if ((unsigned long)endl >= SIZE_MAX) {
      errno = ENOMEM;
      goto error;
    }
    end = (size_t)endl;
  }

  buf = malloc(end + 1);
  if (buf == NULL) {
    goto error;
  }

  errno = 0;
  if (fread(buf, 1, end, f) != end) {
    goto error;
  }
  if (fclose(f) != 0) {
    f = NULL;
    goto error;
  }

  buf[end] = '\0';
  *contents = buf;
  return 0;

error:;
  int result = errno;
  if (result == 0) {
    result = EIO;
  }

  if (f != NULL) {
    fclose(f);
  }
  free(buf);
  return result;
}
