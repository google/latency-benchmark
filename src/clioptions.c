#include <stdlib.h>
#include <unistd.h>
#include "clioptions.h"

clioptions* parse_commandline(int argc, const char **argv) {
  clioptions *options = (clioptions *) malloc(sizeof(clioptions));

  options->automated = false;
  options->profile = NULL;
  options->browser = NULL;
  options->results = NULL;

  // parse command line arguments
  int c;

  //TODO: use getopt_long for better looking cli args
  while ((c = getopt(argc, argv, "ap:b:d:r:")) != -1) {
    switch(c) {
    case 'a':
      options->automated = true;
      break;
    case 'p':
      options->profile = optarg;
      break;
    case 'b':
      options->browser = optarg;
      break;
    case 'r':
      options->results = optarg;
      break;
    case ':':       
      fprintf(stderr,
          "Option -%c requires an operand\n", optopt);
      break;
    case '?':
      fprintf(stderr,
          "Unrecognized option: '-%c'\n", optopt);
    }
  }

  return options;
}
