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
int scrolls = 0;
int key_downs = 0;

CVReturn vsync_callback(CVDisplayLinkRef displayLink, const CVTimeStamp *inNow, const CVTimeStamp *inOutputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut, void *displayLinkContext) {
  if (getppid() == 1) {
    // The parent process died, so we should exit immediately.
    exit(0);
  }
  CGLLockContext((CGLContextObj)[context CGLContextObj]);
  [context makeCurrentContext];

  draw_pattern_with_opengl(pattern, scrolls, key_downs);

  [context flushBuffer];
  CGLUnlockContext((CGLContextObj)[context CGLContextObj]);
  return kCVReturnSuccess;
}

// NSWindow's default implementation of canBecomeKeyWindow returns NO if the window is borderless, which prevents the window from gaining keyboard focus. KeyWindow overrides canBecomeKeyWindow to always be YES.
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
  if (argc <= 1) {
    run_server();
    return 0;
  }
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
    [NSApplication sharedApplication];
    NSWindow *window = [[KeyWindow alloc] initWithContentRect:NSMakeRect(200, 200, 500, 500) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    NSOpenGLPixelFormatAttribute attrs[] = { NSOpenGLPFADoubleBuffer, 0 };
    context = [[NSOpenGLContext alloc] initWithFormat:[[NSOpenGLPixelFormat alloc] initWithAttributes:attrs] shareContext:nil];
    [[window contentView] setWantsBestResolutionOpenGLSurface:YES];
    [context setView:[window contentView]];
    [context makeCurrentContext];
    draw_pattern_with_opengl(pattern, 0, 0);
    [context flushBuffer];
    [window makeKeyAndOrderFront:window];
    CVDisplayLinkRef displayLink;
    CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
    CVDisplayLinkSetOutputCallback(displayLink, &vsync_callback, nil);
    CVDisplayLinkStart(displayLink);
    [NSEvent addLocalMonitorForEventsMatchingMask:NSScrollWheelMask handler:^NSEvent *(NSEvent *event) {
      scrolls++;
      return event;
    }];
    [NSEvent addLocalMonitorForEventsMatchingMask:NSKeyDownMask handler:^NSEvent *(NSEvent *event) {
      key_downs++;
      return event;
    }];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
  }
  return 0;
}
