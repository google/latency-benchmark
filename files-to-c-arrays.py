#!/usr/bin/env python
# Copyright 2013 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import sys
import os

if len(sys.argv) < 3:
    print 'Usage: ' + sys.argv[0] + 'output_file input_file1 input_file2 ... input_fileN'
    print
    print 'Generates a .c file containing all of the input files as static'
    print 'character arrays, along with a function to retrieve them.'
    print
    print 'const char *get_file(const char *path, size_t *out_size)'
    exit(1)


def chunk(list, n):
    """Split a list into size n chunks (the last chunk may be shorter)."""
    return (list[i : i + n] for i in range(0, len(list), n))

filesizes = []
filepaths = []
filearrays = []
for filepath in sys.argv[2:]:
    filepaths.append(filepath)
    file = open(filepath, 'rb').read()
    filesizes.append(len(file))
    escapedfile = '\\x' + '\\x'.join(chunk(file.encode('hex'), 2))
    filearrays.append('"\n  "'.join(chunk(escapedfile, 76)))

template = """#include <stdint.h>
#include <string.h>

static const char *file_paths[] = {"%s"};
static const size_t file_sizes[] = {%s};
static const int num_files = %d;
static const char *files[] = {
  "%s"
};

const char *get_file(const char *path, size_t *out_size) {
  for (int i = 0; i < num_files; i++) {
    if (strcmp(file_paths[i], path) == 0) {
      *out_size = file_sizes[i];
      return files[i];
    }
  }
  return NULL;
}
"""

output = open(sys.argv[1], 'w')
output.write(template % ('", "'.join(filepaths),
                         ', '.join(str(x) for x in filesizes),
                         len(filepaths),
                         '",\n  "'.join(filearrays)))
