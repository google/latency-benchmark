![Screenshot](http://google.github.io/latency-benchmark/screenshot.png "Web Latency Benchmark")

#### Download for Windows: [latency-benchmark.exe](http://google.github.io/latency-benchmark/latency-benchmark.exe)<br/>Download for Mac: [latency-benchmark-mac.zip](http://google.github.io/latency-benchmark/latency-benchmark-mac.zip)<br/>Download for Linux: [latency-benchmark-linux.zip](http://google.github.io/latency-benchmark/latency-benchmark-linux.zip)

## About the benchmark

The Web Latency Benchmark is a new kind of benchmark that tests your browser's responsiveness by directly measuring *latency* and *jank*.

Other benchmarks might tell you that your browser can load a big page in 5 seconds, but what happens *during* those 5 seconds? Can you scroll, or is the page frozen until it finishes loading? Can you start typing immediately, or do you have to wait for everything else to finish? That's jank.

When you're dragging a map around does it snap to your mouse cursor instantly, or does it feel like it's being dragged through molasses? When you're playing a game does your avatar jump right when you hit the button, or is there a slight delay at the worst possible time? That's latency.

In an ideal browser, input latency would be 1 frame or less at 60 FPS, and nothing would ever cause jank. In today's browsers, input latency can range as high as 8-10 frames, and jank can last for hundreds of frames due to image loading, JavaScript execution, garbage collection, etc. The Web Latency Benchmark quantifies exactly how much latency and jank your browser has.

## For developers

### How the test works

The Web Latency Benchmark works by programmatically sending input events to a browser window, and using screenshot APIs to detect when the browser has finished drawing its response.

There are two main components: the latency-benchmark server (written in C/C++) and the HTML/JavaScript benchmark page. The HTML page draws a special pattern of pixels to the screen using WebGL or Canvas 2D, then makes an XMLHTTPRequest to the server to start the latency measurement. The server locates the browser window by searching a screenshot for the special pattern. Once the browser window is located, the server starts sending input events. Each time the HTML page recieves an input event it encodes that information into pixels in the on-screen pattern, drawn using the canvas element. Meanwhile the server is taking screenshots every few milliseconds. By decoding the pixel pattern the server can determine to within a few milliseconds how long it takes the browser to respond to each input event.

The native reference test is special because it requires extra support from the server. Using the native APIs of each platform, the server creates a special benchmark window that draws the same pattern as the test webpage, and responds to keyboard input in the same way. To ensure fairness when compared with the browser, this window is opened in a separate process and uses OpenGL to draw the pattern on the screen. The benchmark window opens as a popup window, only 1 pixel tall and without a border or title bar, so it's almost unnoticeable.

### License and distribution

The Web Latency Benchmark is licensed under the Apache License version 2.0. This is an open source project; it is not an official Google product.

### Build prerequisites

Python 2.x is required on all platforms for GYP, which generates the build files.

* Windows: GYP is currently configured to generate project files for Visual Studio 2012 (Express works). 2010 might work too if you edit generate-project-files.bat to change the version. The Windows 8 SDK is required due to the use of DXGI 1.2. It can be installed on Windows 7 and Windows Vista.
* Mac: XCode 4 is required.
* Linux: Clang is required. The benchmark does not compile with GCC. The only other requirement should be X11 development headers and libraries, in the xorg-dev package on Debian/Ubuntu.

### Build steps

First, you need to `git submodule init && git submodule update` to fetch the submodules in third_party. Then, you need to run `generate-project-files`, which will run GYP and generate platform-specific project files in build/.

* Windows: Open `build/latency-benchmark.sln`.
* Mac: Open `build/latency-benchmark.xcodeproj`. For debugging you will need to edit the default scheme to change the working directory of the `latency-test` executable to `$(PROJECT_DIR)` so it can find the HTML files. You will also want to [configure the debugger to ignore SIGPIPE](http://stackoverflow.com/questions/10431579/permanently-configuring-lldb-in-xcode-4-3-2-not-to-stop-on-signals).
* Linux: Run the script `linux-build` to compile with Clang. The binary will be built at `build/out/Debug/latency-benchmark`. Run it in the top-level directory so it can find the HTML files. You can build the release version by defining the environment variable `BUILDTYPE=Release`.

You shouldn't make any changes to the XCode or Visual Studio project files directly. Instead, you should edit `latency-benchmark.gyp` to reflect the changes you want, and re-run the `generate-project-files` script to update the project files with the changes. This ensures that the project files stay in sync across platforms.

### TODO

* Disable mouse and keyboard input during the test to avoid interference.
* Hide the mouse cursor during the test.
* Find a way to share constants like the test timeout between JS and C.
* Test more possible causes of jank:
    * audio/video loading
    * plugins
    * JavaScript parsing
    * GC
    * WebGL shader compilation
    * Web Worker JavaScript parsing/execution
    * GC in a worker
    * HTML parsing
    * CSS parsing
    * layout
    * DNS resolution
    * window resizing
    * image resizing
    * XHR starting/ending
    * All of the above in an iframe or popup.
