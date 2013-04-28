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

 // This is a platform abstraction layer that encapsulates everything you need to screenshot a browser window, send input events to it, and get precise timing information. It is intended to be implementable on Mac, Windows, and Linux (X11). Android and iOS would be cool too, eventually.
#pragma once
#include <stdint.h>
#include <math.h>
#if __STDC_VERSION__ >= 199901L // C99
#include <stdbool.h>
#endif
#ifdef _WINDOWS
// MSVC doesn't support C99, so we compile all the C code as C++ instead.
#include <limits>
#define INFINITY (std::numeric_limits<double>::infinity())
#include <intrin.h>
#define __sync_fetch_and_add _InterlockedExchangeAdd
#endif

typedef struct {
    uint32_t width, height, stride;
    const uint8_t *pixels; // 32-bit BGRA format
    int64_t time_nanoseconds;
    void *platform_specific_data;
} screenshot;

// width and height are automatically clamped to the screen, so to take a full-screen screenshot simply pass UINT32_MAX. May block until the next time something is drawn to the screen, or not depending on platform.
screenshot *take_screenshot(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void free_screenshot(screenshot *screenshot);

// Sends key down and key up events to the foreground window for the 'z' key.
bool send_keystroke_z();
// Warps the mouse to the given point and sends a mousewheel scroll down event.
bool send_scroll_down(int x, int y);

// Returns the number of nanoseconds elapsed relative to some fixed point in the past. The point to which this duration is relative does not change during the lifetime of the process, but can change between different processes.
int64_t get_nanoseconds();
static const int64_t nanoseconds_per_millisecond = 1000000;
static const int64_t nanoseconds_per_second = nanoseconds_per_millisecond * 1000;

// Sends a message to the debug console (which printf doesn't do on Windows...). Accepts printf format
// strings. Always writes a newline at the end of the message.
void debug_log(const char *message, ...);

// From unistd.h, but unistd.h is not available on Windows, so redefine it here.
int usleep(unsigned int microseconds);

bool open_browser(const char *url);
bool open_control_window(uint8_t *test_pattern);
bool close_control_window();

// The magic bytes used to recognize the pattern in the screenshot.
static const int pattern_magic_pixels = 4;
static const int pattern_magic_bytes = pattern_magic_pixels * 4;
static const int pattern_pixels = 8;
static const int pattern_bytes = pattern_pixels * 4;
static const int data_pixels = pattern_pixels - pattern_magic_pixels;
static const int data_bytes = data_pixels * 3;
