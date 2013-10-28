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
#include "../latency-benchmark.h"
#include "../screenscraper.h"

void run_server(void);

static BOOL (APIENTRY *wglSwapIntervalEXT)(int) = 0;
static HGLRC context = NULL;
static uint8_t pattern[pattern_bytes];
static int scrolls = 0;
static int key_downs = 0;
static int esc_presses = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  int64_t paint_time = get_nanoseconds();
  switch(msg) {
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  case WM_MOUSEWHEEL:
    scrolls++;
    InvalidateRect(hwnd, NULL, false);
    break;
  case WM_KEYDOWN:
    if (wParam == VK_ESCAPE) {
      esc_presses++;
    }
    key_downs++;
    InvalidateRect(hwnd, NULL, false);
    break;
  case WM_PAINT:
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    wglMakeCurrent(ps.hdc, context);
    draw_pattern_with_opengl(pattern, scrolls, key_downs, esc_presses);
    SwapBuffers(ps.hdc);
    EndPaint(hwnd, &ps);
    break;
  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}


static const char *window_class_name = "window_class_name";
void message_loop(HANDLE parent_process) {
  WNDCLASS window_class;
  if (!GetClassInfo(GetModuleHandle(NULL), window_class_name, &window_class)) {
    memset(&window_class, 0, sizeof(WNDCLASS));
    window_class.hInstance = GetModuleHandle(NULL);
    window_class.lpszClassName = window_class_name;
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOWTEXT + 1);
    window_class.lpfnWndProc = WndProc;
    ATOM a = RegisterClass(&window_class);
    assert(a);
  }
  HWND native_reference_window = NULL;
  native_reference_window = CreateWindowEx(WS_EX_TOPMOST, // Top most window and No taskbar button
                                window_class_name,
                                "Web Latency Benchmark test window",
                                WS_DISABLED | WS_POPUP, // Borderless and user-inputs Disabled
                                200, 200, pattern_pixels, 1,
                                NULL, NULL, window_class.hInstance, NULL);
  assert(native_reference_window);
  PIXELFORMATDESCRIPTOR pfd;
  ZeroMemory(&pfd, sizeof(pfd));
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER | PFD_DEPTH_DONTCARE;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  HDC hdc = GetDC(native_reference_window);
  int pixel_format = ChoosePixelFormat(hdc, &pfd);
  assert(pixel_format);
  if (!SetPixelFormat(hdc, pixel_format, &pfd)) {
    debug_log("SetPixelFormat failed");
    exit(1);
  }
  context = wglCreateContext(hdc);
  assert(context);
  if (!wglMakeCurrent(hdc, context)) {
    debug_log("Failed to init OpenGL");
    exit(1);
  }
  wglSwapIntervalEXT = (BOOL (APIENTRY *)(int)) wglGetProcAddress("wglSwapIntervalEXT");
  if (wglSwapIntervalEXT == NULL) {
    debug_log("Failed to get wglSwapIntervalEXT");
    exit(1);
  }
  if (!wglSwapIntervalEXT(0)) {
    debug_log("Failed to disable vsync");
    exit(1);
  }
  ReleaseDC(native_reference_window, hdc);
  ShowWindow(native_reference_window, SW_SHOWNORMAL);
  SetWindowPos(native_reference_window, HWND_TOPMOST, 0, 0, 0, 0,
      SWP_NOMOVE | SWP_NOSIZE);
  SetForegroundWindow(native_reference_window);
  UpdateWindow(native_reference_window);

  MSG msg;
  while(1) {
    while(!PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      InvalidateRect(native_reference_window, NULL, false);
      if (parent_process && WaitForSingleObject(parent_process, 0) != WAIT_TIMEOUT) {
        debug_log("parent process died, exiting");
        exit(1);
      }
      usleep(1000);
    }
    if (msg.message == WM_QUIT)
      break;
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

// Override the default behavior for assertion failures to break into the
// debugger.
int __cdecl CrtDbgHook(int nReportType, char* szMsg, int* pnRet) {
  // Break into the debugger, and then report that the exception was handled.
  _CrtDbgBreak();
  return TRUE;
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                       _In_opt_ HINSTANCE hPrevInstance,
                       _In_ LPTSTR    lpCmdLine,
                       _In_ int       nCmdShow)
{
  UNREFERENCED_PARAMETER(hInstance);
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  UNREFERENCED_PARAMETER(nCmdShow);
  debug_log("starting process");

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

  // Prevent automatic DPI scaling.
  SetProcessDPIAware();

  if (__argc == 1) {
    debug_log("running server");
    HDC desktop = GetDC(NULL);
    assert(desktop);
    if (GetDeviceCaps(desktop, LOGPIXELSX) != 96) {
      MessageBox(NULL, "Unfortunately, due to browser bugs you must set the Windows DPI scaling factor to \"100%\" or \"Smaller\" (96 DPI) for this test to work. Please change it and reboot.",
                 "Unsupported DPI", MB_ICONERROR | MB_OK);
      WinExec("DpiScaling.exe", SW_NORMAL);
      return 1;
    }
    ReleaseDC(NULL, desktop);
    run_server();
  }
  debug_log("opening native reference window");
  if (__argc != 3) {
    debug_log("Unrecognized number of arguments");
    return 1;
  }
  // The first argument is the magic pattern to draw on the window, encoded as hex.
  memset(pattern, 0, sizeof(pattern));
  if (!parse_hex_magic_pattern(__argv[1], pattern)) {
    debug_log("Failed to parse pattern");
    return 1;
  }
  // The second argument is the handle of the parent process.
  HANDLE parent_process = (HANDLE)_strtoui64(__argv[2], NULL, 16);
  message_loop(parent_process);
  return 0;
}
