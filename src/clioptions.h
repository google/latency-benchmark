#include <stdio.h>
#include <string.h>

#ifndef bool
#define false   0
#define true    1
#define bool int
#endif

typedef struct {
  bool automated; // is this an automated run, shutdown the browser when done
  char *browser; // path to the executable for the browser to launch
  char *browser_args; // args passed to the browser
  char *results; // path to the executable for the browser to launch
} clioptions;

clioptions* parse_commandline(int argc, const char **argv);
