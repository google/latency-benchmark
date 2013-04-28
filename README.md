# Web Latency Benchmark

The Web Latency Benchmark measures *input* latency: that is, the time between when an input event (such as a mouse click) is received by the browser, and when the browser responds by drawing to the screen. It also quantifies jank by measuring how many frames of animation the browser skips while performing tasks such as image loading or executing JavaScript.

This is different from benchmarks like SunSpider which measure raw JavaScript execution speed but aren't sensitive to latency and jank. Latency and jank determine how responsive a browser feels in use, and are very important for HTML5 games and rich web apps, but aren't well tested by existing benchmarks because they can't be measured by JavaScript alone. The Web Latency Benchmark uses a local server to perform the latency measurements.

## Disclaimer

The Web Latency Benchmark is not complete and is under active development. Results should not be considered definitive. The intended audience is currently only web browser developers.

## How to build/run

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

## How it works

The Web latency Benchmark works by programmatically sending input events to a browser window, and using screenshot APIs to detect when the browser has finished drawing its response.

There are two main components: the latency-benchmark server (written in C/C++) and the HTML/JavaScript benchmark page. The HTML page draws a special pattern of pixels to the screen using WebGL or Canvas 2D, then makes an XMLHTTPRequest to the server to start the latency measurement. The server locates the browser window by searching a screenshot for the special pattern. Once the browser window is located, the server starts sending input events. Each time the HTML page recieves an input event it encodes that information into pixels in the on-screen pattern, drawn using the canvas element. Meanwhile the server is taking screenshots every few milliseconds. By decoding the pixel pattern the server can determine to within a few milliseconds how long it takes the browser to respond to each input event.

## License and distribution

The Web Latency Benchmark is licensed under the Apache License version 2.0. This is an open source project; it is not an official Google product.

## TODO

* Improve the styling and presentation of the test pages and add more explanation about what each test is doing.
* Embed the test HTML/JS into the executable for release builds for easier distribution.
* Windows has a partial implementation of a "reference window" which implements the tests in pure C++ to establish a lower bound on what kind of latency is possible to achieve. (See the open_control_window function.) This should be integrated into the main test page and implemented on the other platforms too.
* Defend against non-test webpages making XMLHttpRequests to the server (a possible security issue, since the screenshotting code isn't security audited).
* Clean up latency-benchmark.html and latency-benchmark.js. Separate the JavaScript into more files.
* Find a way to share constants like the test timeout between JS and C.
* Test more causes of jank: audio/video loading, plugins, JS parsing, JS execution, GC, worker GC, worker JS parsing/execution, HTML parsing, CSS parsing, layout, DNS resolution, window resizing, image resizing, XHR starting/ending, plus all of the above in an iframe or popup.
* IE9 doesn't support typed arrays; support it anyway using a polyfill for typed arrays.
* Support Windows Vista. Look into IE7/8 support with a polyfill for 2D canvas.
