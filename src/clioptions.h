#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
  bool automated; // is this an automated run, shutdown the browser when done
  char *browser; // path to the executable for the browser to launch
  char *browser_args; // args passed to the browser
  char *results; // path to the executable for the browser to launch
} clioptions;

clioptions* parse_commandline(int argc, const char **argv);
