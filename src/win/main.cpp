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

#include "stdafx.h"

void run_server(void);

int __cdecl CrtDbgHook(int nReportType, char* szMsg, int* pnRet) {
  // Break into the debugger, and then report that the exception was handled.
  _CrtDbgBreak();
  return TRUE;
}

// Entry point for compilation with /SUBSYSTEM:WINDOWS
int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                       _In_opt_ HINSTANCE hPrevInstance,
                       _In_ LPTSTR    lpCmdLine,
                       _In_ int       nCmdShow)
{
  UNREFERENCED_PARAMETER(hInstance);
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  UNREFERENCED_PARAMETER(nCmdShow);

  // Prevent error dialogs.
  _set_error_mode(_OUT_TO_STDERR);
  _CrtSetReportHook(CrtDbgHook);
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
      SEM_NOOPENFILEERRORBOX);

  // The Visual Studio debugger starts us in the build\ subdirectory by
  // default, which will prevent us from finding the test files in the html\
  // directory. This workaround will locate the html\ directory even if it's
  // in the parent directory so we will work in the debugger out of the box.
  WIN32_FIND_DATA find_data;
  HANDLE h = FindFirstFile("html", &find_data);
  if (h == INVALID_HANDLE_VALUE) {
    h = FindFirstFile("..\\html", &find_data);
    if (h != INVALID_HANDLE_VALUE) {
      SetCurrentDirectory("..");
    }
  }
  if (h != INVALID_HANDLE_VALUE) {
    FindClose(h);
  }

  run_server();
  return 0;
}

// Entry point for compilation with /SUBSYSTEM:CONSOLE
void main() {
  _tWinMain(0, 0, 0, 0);
}
