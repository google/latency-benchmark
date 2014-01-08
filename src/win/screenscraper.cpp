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

static INIT_ONCE start_time_init_once;
static LARGE_INTEGER start_time;
static LARGE_INTEGER frequency;
BOOL CALLBACK init_start_time(PINIT_ONCE ignored, void *ignored2,
    void **ignored3) {
  BOOL r;
  r = QueryPerformanceCounter(&start_time);
  assert(r);
  r = QueryPerformanceFrequency(&frequency);
  assert(r);
  return TRUE;
}


int64_t get_nanoseconds_elapsed(const LARGE_INTEGER &current_time) {
  BOOL r = InitOnceExecuteOnce(&start_time_init_once, &init_start_time, NULL,
      NULL);
  assert(r);
  int64_t elapsed = current_time.QuadPart - start_time.QuadPart;
  if (elapsed < 0) {
    elapsed = 0;
  }
  // We switch to floating point here before the division to avoid overflow
  // and/or loss of precision. We can't do this in 64-bit signed integer math:
  // doing the multiplication first would overflow in a matter of seconds, while
  // doing the division first risks loss of accuracy if the timer resolution is
  // close to the nanosecond range.
  double elapsed_ns =
      elapsed * (nanoseconds_per_second / (double)frequency.QuadPart);
  return (int64_t) elapsed_ns;
}


// Returns the number of nanoseconds elapsed since the first call to
// get_nanoseconds in this process. The first call always returns 0.
int64_t get_nanoseconds() {
  LARGE_INTEGER current_time;
  BOOL r = QueryPerformanceCounter(&current_time);
  assert(r);
  return get_nanoseconds_elapsed(current_time);
}


static INIT_ONCE directx_initialization;
static CRITICAL_SECTION directx_critical_section;
static ID3D11Device *device = NULL;
static ID3D11DeviceContext *context = NULL;
static IDXGIFactory2 *dxgi_factory = NULL;
static IDXGIAdapter1 *dxgi_adapter = NULL;
static IDXGIOutput *dxgi_output = NULL;
static IDXGIOutput1 *dxgi_output1 = NULL;
static IDXGIOutputDuplication *dxgi_output_duplication = NULL;

BOOL CALLBACK create_device(PINIT_ONCE ignored, void *ignored2,
    void **ignored3) {
  debug_log("creating device");
  HRESULT hr;
  hr = CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void **)&dxgi_factory);
  assert(hr == S_OK);
  hr = dxgi_factory->EnumAdapters1(0, &dxgi_adapter);
  assert(hr == S_OK);
  hr = dxgi_adapter->EnumOutputs(0, &dxgi_output);
  assert(hr == S_OK);
  hr = dxgi_output->QueryInterface(__uuidof(IDXGIOutput1),
      (void **)&dxgi_output1);
  assert(hr == S_OK);
  const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
  D3D_FEATURE_LEVEL out_level;
  UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  hr = D3D11CreateDevice(dxgi_adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags,
      levels, 1, D3D11_SDK_VERSION, &device, &out_level, &context);
  assert(hr == S_OK);
  hr = dxgi_output1->DuplicateOutput(device, &dxgi_output_duplication);
  assert(hr == S_OK);
  InitializeCriticalSection(&directx_critical_section);
  return TRUE;
}


static int64_t last_screenshot_time = 0;
static screenshot *take_screenshot_with_dxgi(uint32_t x, uint32_t y,
    uint32_t width, uint32_t height) {
  HRESULT hr = S_OK;
  BOOL r =
      InitOnceExecuteOnce(&directx_initialization, &create_device, NULL, NULL);
  assert(r);
  EnterCriticalSection(&directx_critical_section);
  if (dxgi_output_duplication == NULL) {
    hr = dxgi_output1->DuplicateOutput(device, &dxgi_output_duplication);
    if (hr != S_OK) {
      debug_log("Failed to create output duplication interface.");
      return false;
    }
  }
  DXGI_OUTDUPL_FRAME_INFO frame_info;
  frame_info.AccumulatedFrames = 0;
  IDXGIResource *screen_resource = NULL;
  while(true) {
    hr = dxgi_output_duplication->AcquireNextFrame(INFINITE, &frame_info,
        &screen_resource);
    if (hr == S_OK && frame_info.AccumulatedFrames > 0) {
      // A new screenshot was taken.
      if (frame_info.AccumulatedFrames > 1) {
        debug_log("more than one frame at a time: %d frames",
            frame_info.AccumulatedFrames);
      }
      int64_t new_screenshot_time =
          get_nanoseconds_elapsed(frame_info.LastPresentTime);
      last_screenshot_time = new_screenshot_time;
      break;
    } else if (hr != S_OK) {
      // This happens if the screensaver came on or the computer went to sleep.
      // Try recreating the output duplication interface before giving up.
      debug_log("AcquireNextFrame failed");
      dxgi_output_duplication->Release();
      dxgi_output_duplication = NULL;
      hr = dxgi_output1->DuplicateOutput(device, &dxgi_output_duplication);
      if (hr != S_OK) {
        debug_log("Failed to recreate output duplication interface.");
        return false;
      }
    } else {
      // This was a mouse movement, not a screenshot. Try again.
      screen_resource->Release();
      dxgi_output_duplication->ReleaseFrame();
    }
  }
  ID3D11Texture2D *framebuffer = NULL;
  hr = screen_resource->QueryInterface(__uuidof(ID3D11Texture2D),
      (void **)&framebuffer);
  assert(hr == S_OK);
  D3D11_TEXTURE2D_DESC framebuffer_desc;
  framebuffer->GetDesc(&framebuffer_desc);
  assert(framebuffer_desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM);
  // Clamp width and height.
  width = min(width, framebuffer_desc.Width - x);
  height = min(height, framebuffer_desc.Height - y);
  if (x >= framebuffer_desc.Width || y >= framebuffer_desc.Height ||
      width == 0 || height == 0) {
    debug_log("Invalid rectangle for screenshot.");
    framebuffer->Release();
    screen_resource->Release();
    LeaveCriticalSection(&directx_critical_section);
    return NULL;
  }
  ID3D11Texture2D *screenshot_texture = NULL;
  D3D11_TEXTURE2D_DESC screenshot_desc;
  D3D11_MAPPED_SUBRESOURCE screenshot_mapped;
  screenshot_desc = framebuffer_desc;
  screenshot_desc.Width = width;
  screenshot_desc.Height = height;
  screenshot_desc.Usage = D3D11_USAGE_STAGING;
  screenshot_desc.CPUAccessFlags =
      D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
  screenshot_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  screenshot_desc.MipLevels = screenshot_desc.ArraySize = 1;
  screenshot_desc.SampleDesc.Count = 1;
  screenshot_desc.BindFlags = screenshot_desc.MiscFlags = 0;
  device->CreateTexture2D(&screenshot_desc, NULL, &screenshot_texture);
  D3D11_BOX box;
  box.left = x;
  box.top = y;
  box.right = x + width;
  box.bottom = y + height;
  box.front = 0;
  box.back = 1;
  context->CopySubresourceRegion(screenshot_texture, 0, 0, 0, 0, framebuffer, 0,
      &box);
  hr = context->Map(screenshot_texture, 0, D3D11_MAP_READ_WRITE, 0,
      &screenshot_mapped);
  assert(hr == S_OK);
  screenshot *screen = (screenshot *)malloc(sizeof(screenshot));
  screen->width = width;
  screen->height = height;
  screen->stride = screenshot_mapped.RowPitch;
  screen->pixels = (uint8_t *)screenshot_mapped.pData;
  screen->time_nanoseconds = last_screenshot_time;
  screen->platform_specific_data = screenshot_texture;
  framebuffer->Release();
  screen_resource->Release();
  hr = dxgi_output_duplication->ReleaseFrame();
  assert(hr == S_OK);
  LeaveCriticalSection(&directx_critical_section);
  return screen;
}


bool composition_disabled = false;
static screenshot *take_screenshot_with_gdi(uint32_t x, uint32_t y,
    uint32_t width, uint32_t height) {
  HRESULT hr;
  int ir;
  BOOL r;
  // DWM causes screenshotting with GDI to be very slow. The only fix is to
  // disable DWM during the test. DWM will automatically be re-enabled when the
  // process exits. This API stops working on Windows 8 (though it returns
  // success regardless).
  if (!composition_disabled) {
    // HACK: DwmEnableComposition fails unless GetDC(NULL) has been called
    // first. Who knows why.
    HDC workaround = GetDC(NULL);
    int ir = ReleaseDC(NULL, workaround);
    assert(ir);
    hr = DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
    assert(hr == S_OK);
    // DWM takes a little while to repain the screen after being disabled.
    Sleep(1000);
    composition_disabled = true;
  }
  int virtual_x = ((int)x) - GetSystemMetrics(SM_XVIRTUALSCREEN);
  int virtual_y = ((int)y) - GetSystemMetrics(SM_YVIRTUALSCREEN);
  width = min(width, (uint32_t)GetSystemMetrics(SM_CXVIRTUALSCREEN));
  height = min(height, (uint32_t)GetSystemMetrics(SM_CYVIRTUALSCREEN));
  HDC screen_dc = GetDC(NULL);
  assert(screen_dc);
  HDC memory_dc = CreateCompatibleDC(screen_dc);
  assert(memory_dc);
  BITMAPINFO bitmap_info;
  memset(&bitmap_info, 0, sizeof(BITMAPINFO));
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = width;
  bitmap_info.bmiHeader.biHeight = -(int)height;
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;
  uint8_t *pixels = NULL;
  HBITMAP hbitmap = CreateDIBSection(screen_dc, &bitmap_info, DIB_RGB_COLORS,
      (void **)&pixels, NULL, 0);
  assert(hbitmap);
  SelectObject(memory_dc, hbitmap);
  r = BitBlt(memory_dc, 0, 0, width, height, screen_dc, virtual_x, virtual_y,
      SRCCOPY);
  assert(r);
  ir = ReleaseDC(NULL, screen_dc);
  assert(ir);
  r = DeleteObject(memory_dc);
  assert(r);
  screenshot *shot = (screenshot *)malloc(sizeof(screenshot));
  shot->pixels = pixels;
  shot->width = width;
  shot->height = height;
  shot->stride = width * 4;
  shot->platform_specific_data = hbitmap;
  shot->time_nanoseconds = get_nanoseconds();
  return shot;
}


static bool use_dxgi() {
  OSVERSIONINFO version;
  memset(&version, 0, sizeof(OSVERSIONINFO));
  version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  BOOL r = GetVersionEx(&version);
  assert(r);
  bool windows_8_or_greater = version.dwMajorVersion > 6 ||
      (version.dwMajorVersion == 6 && version.dwMinorVersion >= 2);
  return windows_8_or_greater;
}


screenshot *take_screenshot(uint32_t x, uint32_t y, uint32_t width,
    uint32_t height) {
  if (use_dxgi()) {
    // On Windows 8+ we use the DXGI 1.2 Desktop Duplication API.
    return take_screenshot_with_dxgi(x, y, width, height);
  } else {
    // Before Windows 8 we use GDI to take the screenshot.
    return take_screenshot_with_gdi(x, y, width, height);
  }
}


void free_screenshot(screenshot *shot) {
  if (use_dxgi()) {
    EnterCriticalSection(&directx_critical_section);
    ID3D11Texture2D *texture = (ID3D11Texture2D *)shot->platform_specific_data;
    context->Unmap(texture, 0);
    texture->Release();
    free(shot);
    LeaveCriticalSection(&directx_critical_section);
  } else {
    DeleteObject((HBITMAP)shot->platform_specific_data);
    free(shot);
  }
}


// Sends a keydown+keyup event for the given key to the foreground window.
static bool send_keystroke(WORD key_code) {
  INPUT input[2];
  memset(input, 0, sizeof(INPUT) * 2);
  input[0].type = INPUT_KEYBOARD;
  input[0].ki.wVk = key_code;
  input[1].type = INPUT_KEYBOARD;
  input[1].ki.wVk = key_code;
  input[1].ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(2, input, sizeof(INPUT));
  return true;
}

bool send_keystroke_b() { return send_keystroke(0x42); }
bool send_keystroke_t() { return send_keystroke(0x54); }
bool send_keystroke_w() { return send_keystroke(0x57); }
bool send_keystroke_z() { return send_keystroke(0x5A); }

bool send_scroll_down(int x, int y) {
  SetCursorPos(x, y);
  INPUT input;
  memset(&input, 0, sizeof(INPUT));
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = MOUSEEVENTF_WHEEL;
  input.mi.mouseData = -WHEEL_DELTA;
  SendInput(1, &input, sizeof(INPUT));
  return true;
}


static const int log_buffer_size = 1000;
void debug_log(const char *message, ...) {
#ifndef NDEBUG
  char buf[log_buffer_size];
  va_list args;
  va_start(args, message);
  vsnprintf_s(buf, _TRUNCATE, message, args);
  va_end(args);
  OutputDebugStringA(buf);
  OutputDebugStringA("\n");
  printf("%s\n", buf);
#endif
}

void always_log(const char *message, ...) {
  char buf[log_buffer_size];
  va_list args;
  va_start(args, message);
  vsnprintf_s(buf, _TRUNCATE, message, args);
  va_end(args);
  OutputDebugStringA(buf);
  OutputDebugStringA("\n");
  printf("%s\n", buf);
}


int usleep(unsigned int microseconds) {
  Sleep(microseconds / 1000);
  return 0;
}

HANDLE browser_process_handle = NULL;

bool open_browser(const char *program, const char *args, const char *url) {
  // Recommended by:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/bb762153(v=vs.85).aspx

  if (program == NULL || strcmp(program, "") == 0) {
	HRESULT hr =
      CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    assert(hr == S_OK);
    return 32 < (int)ShellExecuteA(NULL, "open", url, args, NULL, SW_SHOWNORMAL);
  }

  if (args == NULL) {
    args = "";
  }
  char command_line[4096];
  sprintf_s(command_line, sizeof(command_line), "\"%s\" %s \"%s\"", program, args, url);

  PROCESS_INFORMATION process_info;
  STARTUPINFO startup_info;
  memset(&startup_info, 0, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);
  if (!CreateProcess(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL,
                     &startup_info, &process_info)) {
    debug_log("Failed to start process");
    return false;
  }
  browser_process_handle = process_info.hProcess;
  return true;
}

bool close_browser() {
  if (browser_process_handle == NULL) {
    debug_log("browser not open");
    return false;
  }
  if (!TerminateProcess(browser_process_handle, 0)) {
    debug_log("Failed to terminate process");
    browser_process_handle = NULL;
    return false;
  }
  browser_process_handle = NULL;
  return true;
}

HANDLE window_process_handle = NULL;

bool open_native_reference_window(uint8_t *test_pattern_for_window) {
  // The native reference window is opened in a new child process to make the
  // test more fair. Unfortunately Visual Studio can't automatically attach to
  // child processes. WinDbg can, so you can use WinDbg to debug the native
  // reference window process. If that's too painful, you can configure
  // Visual Studio to pass arguments on startup to debug the native reference
  // window code in isolation. Here are some sample arguments that will work:
  // -p 2923BEE16CD6529049F1BBE9 -h 0

  if (window_process_handle != NULL) {
    debug_log("native window already open");
    return false;
  }
  PROCESS_INFORMATION process_info;
  char hex_pattern[hex_pattern_length + 1];
  hex_encode_magic_pattern(test_pattern_for_window, hex_pattern);
  STARTUPINFO startup_info;
  memset(&startup_info, 0, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);
  char command_line[4096];
  HANDLE current_process_handle = NULL;
  if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(),
                       GetCurrentProcess(), &current_process_handle, 0, TRUE,
                       DUPLICATE_SAME_ACCESS)) {
    debug_log("DuplicateHandle failed");
    return false;
  }
  int numArgs;
  HMODULE exe = GetModuleHandle(NULL);
  char filename_of_exe[2048];
  DWORD result = GetModuleFileName(exe, filename_of_exe, sizeof(filename_of_exe));
  assert(result);
  sprintf_s(command_line, sizeof(command_line), "\"%s\" -p %s -h %X", filename_of_exe,
            hex_pattern, current_process_handle);
  if (!CreateProcess(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL,
                     &startup_info, &process_info)) {
    debug_log("Failed to start process");
    return false;
  }
  window_process_handle = process_info.hProcess;
  // Wait for window show animation to finish.
  usleep(1000 * 1000 * 2);
  return true;
}

bool close_native_reference_window() {
  if (window_process_handle == NULL) {
    debug_log("native window not open");
    return false;
  }
  if (!TerminateProcess(window_process_handle, 0)) {
    debug_log("Failed to terminate process");
    window_process_handle = NULL;
    return false;
  }
  window_process_handle = NULL;
  return true;
}
