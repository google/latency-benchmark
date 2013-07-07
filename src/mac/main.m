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
#import <dirent.h>
#import <OpenGL/OpenGL.h>
#include "../latency-benchmark.h"
#include "screenscraper.h"

NSOpenGLContext *context;
uint8_t pattern[pattern_bytes];
static int scrolls = 0;
static int key_downs = 0;
static int esc_presses = 0;

// This callback is called for each display refresh by CVDisplayLink so that we
// can draw at exactly the display's refresh rate.
static CVReturn vsync_callback(
    CVDisplayLinkRef displayLink,
    const CVTimeStamp *inNow,
    const CVTimeStamp *inOutputTime,
    CVOptionFlags flagsIn,
    CVOptionFlags *flagsOut,
    void *displayLinkContext) {
  if (getppid() == 1) {
    // The parent process died, so we should exit immediately.
    exit(0);
  }

  // Ideally we would wait until the vblank interval to perform the rendering
  // and swap buffers, to avoid tearing while keeping latency to a minimum.
  // Unfortunately I haven't found a way yet to tell when the vblank interval
  // is. CVDisplayLink doesn't tell us; it calls us at some random time during
  // the raster scan, which means rendering immediately can cause tearing. For
  // now we'll accept tearing to keep latency low.

  // Actually ideally, we would know the exact raster scan position, and would
  // start rendering at a time such that we would finish exactly when the scan
  // reaches the top pixel of our window. This could shave off a few
  // milliseconds of latency. Unfortunately few systems provide the necessary
  // timing information.

  // We must lock the OpenGL context since it's shared with the main thread.
  CGLLockContext((CGLContextObj)[context CGLContextObj]);
  [context makeCurrentContext];
  draw_pattern_with_opengl(pattern, scrolls, key_downs, esc_presses);
  [context flushBuffer];
  CGLUnlockContext((CGLContextObj)[context CGLContextObj]);
  return kCVReturnSuccess;
}

// NSWindow's default implementation of canBecomeKeyWindow returns NO if the
// window is borderless, which prevents the window from gaining keyboard focus.
// KeyWindow overrides canBecomeKeyWindow to always be YES.
@interface KeyWindow : NSWindow
@end
@implementation KeyWindow
- (BOOL)canBecomeKeyWindow { return YES; }
@end


void run_server(void);

int main(int argc, char *argv[])
{
  // SIGPIPE will terminate the process if we write to a closed socket, unless
  // we disable it like so. Note that GDB/LLDB will stop on SIGPIPE anyway
  // unless you configure them not to.
  // http://stackoverflow.com/questions/10431579/permanently-configuring-lldb-in-xcode-4-3-2-not-to-stop-on-signals
  signal(SIGPIPE, SIG_IGN);
  // If we have no arguments, run the test server.
  if (argc <= 1) {
    run_server();
    return 0;
  }
  // If we have arguments, assume we are a child process of a server, and our
  // job is to create a reference test window.
  memset(pattern, 0, sizeof(pattern));
  debug_log("argument 1: %s", argv[1]);
  if (!parse_hex_magic_pattern(argv[1], pattern)) {
    debug_log("Failed to parse pattern.");
    return 1;
  }
  ProcessSerialNumber psn;
  OSErr err = GetCurrentProcess(&psn);
  assert(!err);
  // By default, naked binary applications are not allowed to create windows
  // and receive input focus. This call allows us to create windows that receive
  // input focus, but does not cause the creation of a Dock icon or menu bar.
  TransformProcessType(&psn, kProcessTransformToUIElementApplication);
  @autoreleasepool {
    // Initialize Cocoa.
    [NSApplication sharedApplication];
    // Create a borderless window that can accept key window status (=input
    // focus).
    NSRect window_rect = NSMakeRect(0, 0, pattern_pixels, 1);
    window_rect = [[[NSScreen screens] objectAtIndex:0] convertRectFromBacking:window_rect];
    // TODO: Choose window position to overlap the test pattern in the browser
    // window running the test, so as to appear on the same monitor.
    window_rect.origin = CGPointMake(500, 500);
    NSWindow *window = [[KeyWindow alloc] initWithContentRect:window_rect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    // Create an OpenGL context.
    NSOpenGLPixelFormatAttribute attrs[] = { NSOpenGLPFADoubleBuffer, 0 };
    context = [[NSOpenGLContext alloc] initWithFormat:[[NSOpenGLPixelFormat alloc] initWithAttributes:attrs] shareContext:nil];
    // Disable vsync. When vsync is enabled calling [context flushBuffer] can
    // block for up to 1 frame, which can add unwanted latency. Actually Quartz
    // sometimes overrides this setting and temporarily turns vsync back on, but
    // there's nothing we can do about that from here. The only way to turn that
    // behavior off is with the "Beam Sync" setting available in Apple's "Quartz
    // Debug" tool, part of "Graphics Tools for XCode".
    GLint param = 0;
    CGLSetParameter([context CGLContextObj], kCGLCPSwapInterval, &param);
    // Make sure that the driver presents all frames immediately without queuing
    // any.
    param = 1;
    CGLSetParameter([context CGLContextObj], kCGLCPMPSwapsInFlight, &param);
    // Request a high DPI backbuffer on retina displays.
    [[window contentView] setWantsBestResolutionOpenGLSurface:YES];
    // Attach the context to the window.
    [context setView:[window contentView]];
    // Draw the test pattern on the window before it is shown.
    [context makeCurrentContext];
    draw_pattern_with_opengl(pattern, 0, 0, 0);
    [context flushBuffer];
    // Show the window.
    [window makeKeyAndOrderFront:window];
    // CVDisplayLink starts a dedicated rendering thread that receives callbacks
    // at each vsync interval.
    CVDisplayLinkRef displayLink;
    // TODO: Ensure the CVDisplayLink is attached to the correct monitor.
    CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
    CVDisplayLinkSetOutputCallback(displayLink, &vsync_callback, nil);
    CVDisplayLinkStart(displayLink);
    // Listen for scroll wheel and keyboard events and update the appropriate
    // counters (on the main UI thread).
    [NSEvent addLocalMonitorForEventsMatchingMask:NSScrollWheelMask handler:^NSEvent *(NSEvent *event) {
      scrolls++;
      return nil;
    }];
    [NSEvent addLocalMonitorForEventsMatchingMask:NSKeyDownMask handler:^NSEvent *(NSEvent *event) {
      if ([event keyCode] == 53) {
        esc_presses++;
      }
      key_downs++;
      return nil;
    }];
    // Steal input focus and become the topmost window.
    [NSApp activateIgnoringOtherApps:YES];
    // Pass control to the Cocoa event loop. The parent process will kill us
    // when it's done testing. As a fallback, the CVDisplayLink callback will
    // kill the process if the parent process dies before it gets a chance to
    // kill us.
    [NSApp run];
  }
  return 0;
}
