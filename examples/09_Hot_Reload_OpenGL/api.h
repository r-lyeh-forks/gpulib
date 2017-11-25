#pragma once

#define GPULIB_DEBUG_MANUAL
#include "../../gpulib_debug.h"

struct api_t {
  void * (*Init)(Display *, Window, char *);
  void   (*Load)(void *, Display *, Window, char *);
  int    (*Step)(void *, Display *, Window, char *);
  void   (*Unload)(void *);
  void   (*Deinit)(void *);
};