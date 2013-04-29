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

#import <Cocoa/Cocoa.h>
#include <dirent.h>

void run_server(void);

int main(int argc, char *argv[])
{
  // SIGPIPE will terminate the process if we write to a closed socket, unless
  // we disable it like so. Note that GDB/LLDB will stop on SIGPIPE anyway
  // unless you configure them not to.
  // http://stackoverflow.com/questions/10431579/permanently-configuring-lldb-in-xcode-4-3-2-not-to-stop-on-signals
  signal(SIGPIPE, SIG_IGN);
  run_server();
  return 0;
}
