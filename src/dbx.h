#ifndef _DBX_H_
#define _DBX_H_

#include <stdint.h>

struct dbx_options {
  char *command;
  char *name;
  char *image;
  struct dbx_options_ports {
    uint16_t container;
    uint16_t host;
  } ports;
};

#define COUNTOF(x) ((sizeof(x)) / (sizeof(*x)))

#endif
