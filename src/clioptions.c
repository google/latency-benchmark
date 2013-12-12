#include <stdlib.h>
#include "clioptions.h"

extern char *optarg;
extern int optind;
extern char optopt;
int getopt(int, char **, char *);

clioptions* parse_commandline(int argc, const char **argv) {
  clioptions *options = (clioptions *) malloc(sizeof(clioptions));

  options->automated = false;
  options->browser = NULL;
  options->results = NULL;
  options->browser_args = NULL;

  // parse command line arguments
  int c;

  //TODO: use getopt_long for better looking cli args
  while ((c = getopt(argc, (char **)argv, "ab:d:r:e:")) != -1) {
    switch(c) {
    case 'a':
      options->automated = true;
      break;
    case 'b':
      options->browser = optarg;
      break;
    case 'r':
      options->results = optarg;
      break;
    case 'e':
      options->browser_args = optarg;
      break;
    case ':':
      fprintf(stderr,
          "Option -%c requires an operand\n", optopt);
      break;
    case '?':
	  printf("JMAHER: %c", optopt);
      fprintf(stderr,
          "Unrecognized option: '-%c'\n", optopt);
    }
  }

  return options;
}
