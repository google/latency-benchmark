#ifndef WLB_CLIOPTIONS_H_
#define WLB_CLIOPTIONS_H_

#include "screenscraper.h"

typedef struct {
  bool automated; // is this an automated run, shutdown the browser when done
  char *browser; // path to the executable for the browser to launch
  char *browser_args; // args passed to the browser
  char *results_url; // URL to post results to after an automated run.
  char *magic_pattern; // When launching a native reference test window, this
                       // contains the magic pattern to draw, encoded in
                       // hexadecimal.
} clioptions;

void parse_commandline(int argc, const char **argv, clioptions *options);

#endif // WLB_CLIOPTIONS_H