/*
 * Copyright 2013 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include "../clioptions.h"

void run_server(clioptions *opts);

int main(int argc, const char **argv)
{
  clioptions *opts = parse_commandline(argc, argv);
  if (opts == NULL) {
    return 1;
  }

  if (opts->browser == NULL) {
    opts->browser = "xdg-open";
  }

  if (opts->results == NULL) {
    opts->results = "";
  }

  if (opts->profile != NULL) {
    char args[] = "-no-remote -profile";
    char *profile = strdup(opts->profile);
    opts->profile = malloc(((strlen(args) + strlen(profile)) * sizeof(char)) + 2);
    sprintf(opts->profile, "%s %s", args, profile);
    free(profile);
  }

  run_server(opts);
  if (opts->profile != NULL) {
    free(opts->profile);
  }
  free(opts);
  return 0;
}

