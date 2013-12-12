#include "clioptions.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern char *optarg;
extern int optind;
extern char optopt;
int getopt(int, char **, char *);

void print_usage_and_exit() {
  fprintf(stderr, "usage: latency-benchmark [-a [-r url_to_post_results_to]]\n");
  fprintf(stderr, "           [-b path_to_browser_executable [-e arguments_for_browser] ]\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Measures input latency and jank in web browsers. Specify -a, -b,\n");
  fprintf(stderr, "and -r to automatically run the test and report results to a server.\n");
  exit(1);
}

void parse_commandline(int argc, const char **argv, clioptions *options) {
  memset(options, 0, sizeof(*options));

  // parse command line arguments
  int c;

  //TODO: use getopt_long for better looking cli args
  while ((c = getopt(argc, (char **)argv, "ab:d:r:e:p:")) != -1) {
    switch(c) {
    case 'a':
      options->automated = true;
      break;
    case 'b':
      options->browser = optarg;
      break;
    case 'r':
      options->results_url = optarg;
      break;
    case 'e':
      options->browser_args = optarg;
      break;
    case 'p':
      options->magic_pattern = optarg;
      break;
    case ':':
      fprintf(stderr, "Option -%c requires an operand\n", optopt);
      print_usage_and_exit();
      break;
    case '?':
	    fprintf(stderr, "Unrecognized option: '-%c'\n", optopt);
      print_usage_and_exit();
    }
  }
  
  // Validate the options.
  if (options->magic_pattern) {
    if (options->automated || options->browser || options->results_url ||
        options->browser_args) {
      fprintf(stderr, "-p is incompatible with all other options.\n");
      print_usage_and_exit();
    }
  }
  if (options->automated && !options->browser) {
    fprintf(stderr, "You must specify a browser executable to run in automatic mode.\n");
    print_usage_and_exit();
  }
  if (options->results_url && !options->automated) {
    fprintf(stderr, "Results can only be reported in automatic mode.");
    print_usage_and_exit();
  }
  if (options->browser_args && !options->browser) {
    fprintf(stderr, "-b must be specified when -e is present.");
    print_usage_and_exit();
  }
}
