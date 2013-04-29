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

#import "../screenscraper.h"
#import <Cocoa/Cocoa.h>

const float float_epsilon = 0.0001;
bool near_integer(float f) {
  return fabsf(remainderf(f, 1)) < float_epsilon;
}

static const CGWindowImageOption image_options =
    kCGWindowImageBestResolution | kCGWindowImageShouldBeOpaque;

screenshot *take_screenshot(uint32_t x, uint32_t y, uint32_t width,
    uint32_t height) {
  // TODO: support multiple monitors.
  NSScreen *screen = [[NSScreen screens] objectAtIndex:0];
  CGRect screen_rect = [screen convertRectToBacking:[screen frame]];
  CGRect capture_rect = { .origin.x = x, .origin.y = y, .size.width = width,
      .size.height = height };
  // Clamp to the screen.
  capture_rect = CGRectIntersection(capture_rect, screen_rect);
  // Convert to logical pixels from backing store pixels.
  CGRect converted_capture_rect = [screen convertRectFromBacking:capture_rect];
  // Make sure we are at an integer logical pixel to satisfy CGWindowListCreatImage.
  if (!near_integer(converted_capture_rect.origin.x) ||
      !near_integer(converted_capture_rect.origin.y)) {
    debug_log(
        "Can't take screenshot at odd coordinates on a high DPI display.");
    return NULL;
  }
  // Round width/height up to the next logical pixel.
  converted_capture_rect.size.width = ceilf(converted_capture_rect.size.width);
  converted_capture_rect.size.height =
      ceilf(converted_capture_rect.size.height);
  // Update capture_rect with the final rounded values.
  capture_rect = [screen convertRectToBacking:converted_capture_rect];
  CGImageRef window_image = CGWindowListCreateImage(converted_capture_rect,
      kCGWindowListOptionAll, kCGNullWindowID, image_options);
  int64_t screenshot_time = get_nanoseconds();
  if (!window_image) {
    debug_log("CGWindowListCreateImage failed");
    return NULL;
  }
  size_t image_width = CGImageGetWidth(window_image);
  size_t image_height = CGImageGetHeight(window_image);
  assert(image_width == capture_rect.size.width);
  assert(image_height == capture_rect.size.height);
  size_t stride = CGImageGetBytesPerRow(window_image);
  // Assert 32bpp BGRA pixel format.
  size_t bpp = CGImageGetBitsPerPixel(window_image);
  size_t bpc = CGImageGetBitsPerComponent(window_image);
  CGBitmapInfo bitmap_info = CGImageGetBitmapInfo(window_image);
  // I think something will probably break if we're not little endian.
  assert(kCGBitmapByteOrder32Little == kCGBitmapByteOrder32Host);
  // We expect little-endian, alpha "first", which in reality comes out to BGRA byte order.
  bool correct_byte_order =
      (bitmap_info & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Little;
  bool correct_alpha_location = bitmap_info & kCGBitmapAlphaInfoMask &
      (kCGImageAlphaFirst | kCGImageAlphaNoneSkipFirst |
       kCGImageAlphaPremultipliedFirst);
  if (bpp != 32 || bpc != 8 || !correct_byte_order || !correct_alpha_location) {
    debug_log("Incorrect image format from CGWindowListCreateImage. "
              "bpp = %d, bpc = %d, byte order = %s, alpha location = %s",
              bpp, bpc, correct_byte_order ? "correct" : "wrong",
              correct_alpha_location ? "correct" : "wrong");
    CFRelease(window_image);
    return NULL;
  }
  CFDataRef image_data =
      CGDataProviderCopyData(CGImageGetDataProvider(window_image));
  CFRelease(window_image);
  const uint8_t *pixels = CFDataGetBytePtr(image_data);
  screenshot *shot = (screenshot *)malloc(sizeof(screenshot));
  shot->width = (int32_t)image_width;
  shot->height = (int32_t)image_height;
  shot->stride = (int32_t)stride;
  shot->pixels = pixels;
  shot->time_nanoseconds = screenshot_time;
  shot->platform_specific_data = (void *)image_data;
  return shot;
}

void free_screenshot(screenshot *shot) {
  CFRelease((CFDataRef)shot->platform_specific_data);
  free(shot);
}

bool send_keystroke_z() {
  static CGEventRef down = NULL;
  static CGEventRef up = NULL;
  if (!down || !up) {
    down = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)6, true);
    up = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)6, false);
  }
  CGEventPost(kCGHIDEventTap, down);
  CGEventPost(kCGHIDEventTap, up);
  return true;
}

bool send_scroll_down(x, y) {
  CGFloat devicePixelRatio =
      [[[NSScreen screens] objectAtIndex:0] backingScaleFactor];
  CGWarpMouseCursorPosition(
      CGPointMake(x / devicePixelRatio, y / devicePixelRatio));
  static CGEventRef down = NULL;
  if (!down) {
    down = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitPixel, 1, -20);
  }
  CGEventPost(kCGHIDEventTap, down);
  return true;
}

static AbsoluteTime start_time = { .hi = 0, .lo = 0 };
int64_t get_nanoseconds() {
  // TODO: Apple deprecated UpTime(), so switch to mach_absolute_time.
  if (UnsignedWideToUInt64(start_time) == 0) {
    start_time = UpTime();
    return 0;
  }
  return UnsignedWideToUInt64(AbsoluteDeltaToNanoseconds(UpTime(), start_time));
}

void debug_log(const char *message, ...) {
#ifdef DEBUG
  va_list list;
  va_start(list, message);
  vprintf(message, list);
  va_end(list);
  putchar('\n');
#endif
}

bool open_browser(const char *url) {
    return [[NSWorkspace sharedWorkspace] openURL:
        [NSURL URLWithString:[NSString stringWithUTF8String:url]]];
}

bool open_control_window() {
  // TODO
  return false;
}

bool close_control_window() {
  // TODO
  return false;
}
