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

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include "screenscraper.h"
#include "latency-benchmark.h"
#include "../third_party/mongoose/mongoose.h"

struct mg_context *mongoose = NULL;

// Makes an HTTP HEAD request to the given URL and returns true if the
// response's status code is HTTP 200 OK.
static bool check_url_returns_HTTP_200(char *host, int port, char *path) {
  assert(mongoose);
  char error_buffer[1000];
  error_buffer[0] = 0;
  struct mg_connection *connection = mg_download(host, port,
      /* use_ssl */ false, error_buffer, sizeof(error_buffer),
      "HEAD %s HTTP/1.0\r\n\r\n", path);
  // Mongoose stores the HTTP response code in the URI field of the
  // mg_request_info struct, strangely.
  struct mg_request_info *response = mg_get_request_info(connection);
  bool received_200 = strcmp("200", response->uri) == 0;
  mg_close_connection(connection);
  return received_200;
}

// Runs a latency test and reports the results as JSON written to the given
// connection.
static void report_latency(struct mg_connection *connection,
    const uint8_t magic_pattern[]) {
  double key_down_latency_ms = 0;
  double scroll_latency_ms = 0;
  double max_js_pause_time_ms = 0;
  double max_css_pause_time_ms = 0;
  double max_scroll_pause_time_ms = 0;
  char *error = "Unknown error.";
  if (!measure_latency(magic_pattern,
                       &key_down_latency_ms,
                       &scroll_latency_ms,
                       &max_js_pause_time_ms,
                       &max_css_pause_time_ms,
                       &max_scroll_pause_time_ms,
                       &error)) {
    // Report generic error.
    debug_log("measure_latency reported error: %s", error);
    mg_printf(connection, "HTTP/1.1 500 Internal Server Error\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Content-Type: text/plain\r\n\r\n"
              "Test failed. %s", error);
  } else {
    // Send the measured latency information back as JSON.
    mg_printf(connection, "HTTP/1.1 200 OK\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Cache-Control: no-cache\r\n"
              "Content-Type: text/plain\r\n\r\n"
              "{ \"keyDownLatencyMs\": %f, "
              "\"scrollLatencyMs\": %f, "
              "\"maxJSPauseTimeMs\": %f, "
              "\"maxCssPauseTimeMs\": %f, "
              "\"maxScrollPauseTimeMs\": %f}",
              key_down_latency_ms,
              scroll_latency_ms,
              max_js_pause_time_ms,
              max_css_pause_time_ms,
              max_scroll_pause_time_ms);
  }
}

// If the given request is a latency test request that specifies a valid
// pattern, returns true and fills in the given array with the pattern specified
// in the request's URL.
static bool is_latency_test_request(const struct mg_request_info *request_info,
    uint8_t magic_pattern[]) {
  assert(magic_pattern);
  // A valid test request will have the path /test and must specify a magic
  // pattern in the magicPattern query variable. The pattern is specified as a
  // string of hex digits and must be the exact length expected (3 bytes for
  // each pixel in the pattern).
  // Here is an example of a valid request:
  // http://localhost:5578/test?magicPattern=8a36052d02c596dfa4c80711
  if (strcmp(request_info->uri, "/test") == 0) {
    const int hex_pattern_length = pattern_magic_pixels * 3 * 2;
    char hex_pattern[hex_pattern_length + 1];
    if (hex_pattern_length == mg_get_var(
            request_info->query_string,
            strlen(request_info->query_string),
            "magicPattern",
            hex_pattern,
            hex_pattern_length + 1)) {
      bool failed = false;
      for (int i = 0; i < pattern_magic_bytes; i++) {
        // Read the pattern from hex. Every fourth byte is the alpha channel
        // with an expected value of 255.
        if (i % 4 == 3) {
          magic_pattern[i] = 255;
        } else {
          int hex_index = (i - i / 4) * 2;
          assert(hex_index < hex_pattern_length);
          int current_byte;
          int num_parsed = sscanf(hex_pattern + hex_index, "%2x", &current_byte);
          failed |= 1 != num_parsed;
          magic_pattern[i] = current_byte;
        }
      }
      return !failed;
    }
  }
  return false;
}

// The number of pages holding open keep-alive requests to the server is stored
// in this global counter, updated with atomic increment/decrement instructions.
// When it reaches zero the server will exit.
volatile long keep_alives = 0;

static int mongoose_begin_request_callback(struct mg_connection *connection) {
  const struct mg_request_info *request_info = mg_get_request_info(connection);
  uint8_t magic_pattern[pattern_magic_bytes];
  if (is_latency_test_request(request_info, magic_pattern)) {
    // This is an XMLHTTPRequest made by JavaScript to measure latency in a browser window.
    // magic_pattern has been filled in with a pixel pattern to look for.
    report_latency(connection, magic_pattern);
    return 1;  // Mark as processed
  } else if (strcmp(request_info->uri, "/keepServerAlive") == 0) {
    __sync_fetch_and_add(&keep_alives, 1);
    mg_printf(connection, "HTTP/1.1 200 OK\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Content-Type: application/octet-stream\r\n"
              "Cache-Control: no-cache\r\n"
              "Transfer-Encoding: chunked\r\n\r\n");
    const int chunk_size = 7;
    char *chunk = "2\r\nZ\n\r\n";
    const int warmup_chunks = 2048;
    for (int i = 0; i < warmup_chunks; i++) {
      mg_write(connection, chunk, chunk_size);
    }
    while(mg_write(connection, chunk, chunk_size)) {
      usleep(1000 * 1000);
    }
    __sync_fetch_and_add(&keep_alives, -1);
    return 1;
  } else if (strcmp(request_info->uri, "/quit") == 0) {
    mg_printf(connection, "HTTP/1.1 500 Internal Server Error\r\n"
              "Content-Type: text/plain\r\n"
              "Cache-Control: no-cache\r\n\r\n");
    const int error_buffer_size = 10000;
    char error_buffer[error_buffer_size];
    if (mg_get_var(request_info->query_string, strlen(request_info->query_string), "error", error_buffer, error_buffer_size) > 0) {
      mg_printf(connection, "Error: %s", error_buffer);
    }
    mg_printf(connection, "\n\nServer exiting.");
    keep_alives = -1;
    return 1;
  } else if(strcmp(request_info->uri, "/runControlTest") == 0) {
    uint8_t *test_pattern = (uint8_t *)malloc(pattern_bytes);
    memset(test_pattern, 0, pattern_bytes);
    for (int i = 0; i < pattern_magic_bytes; i++) {
      test_pattern[i] = rand();
    }
    test_pattern[4 * 4 + 2] = TEST_MODE_JAVASCRIPT_LATENCY;
    open_control_window(test_pattern);
    report_latency(connection, test_pattern);
    close_control_window();
    return 1;
  } else {
    // This request doesn't look special.
    // Pass the request on to mongoose for default handling (file serving, or 404).
    return 0;
  }
}

// This is the entry point called by main().
void run_server() {
  assert(mongoose == NULL);
  srand((unsigned int)time(NULL));
  const char *options[] = {
    "listening_ports", "5578",
    // Serve files from the ./html directory.
    "document_root", "html",
    // Forbid everyone except localhost.
    "access_control_list", "-0.0.0.0/0,+127.0.0.0/8",
    // We have a lot of concurrent long-lived requests, so start a lot of threads to make sure we can handle them all.
    "num_threads", "100",
    NULL
  };
  struct mg_callbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.begin_request = mongoose_begin_request_callback;
  mongoose = mg_start(&callbacks, NULL, options);
  if (!mongoose) {
    debug_log("Failed to start server.");
    exit(1);
  }
  usleep(0);

  // Check that the server is operating properly.
  if (!check_url_returns_HTTP_200("localhost", 5578, "/") || !check_url_returns_HTTP_200("localhost", 5578, "/latency-benchmark.html") || !check_url_returns_HTTP_200("localhost", 5578, "/latency-benchmark.js")) {
    // Show an error to the user.
    if (!open_browser("http://localhost:5578/quit?error=Could%20not%20locate%20test%20HTML%20files.%20The%20test%20HTML%20files%20must%20be%20located%20in%20a%20directory%20named%20'html'%20next%20to%20the%20server%20executable.")) {
      debug_log("Failed to open browser.");
    }
    // Wait for the error message to be displayed.
    usleep(1000 * 1000 * 1000);
    exit(1);
  }

  if (!open_browser("http://localhost:5578/")) {
    debug_log("Failed to open browser.");
  }
  // Wait for an initial keep-alive connection to be established.
  while(keep_alives == 0) {
    usleep(1000 * 1000);
  }
  // Wait for all keep-alive connections to be closed.
  while(keep_alives > 0) {
    // NOTE: If you are debugging using GDB or XCode, you may encounter signal
    // SIGPIPE on this line. SIGPIPE is harmless and you should configure your
    // debugger to ignore it. For instructions see here:
    // http://stackoverflow.com/questions/10431579/permanently-configuring-lldb-in-xcode-4-3-2-not-to-stop-on-signals
    // http://ricochen.wordpress.com/2011/07/14/debugging-with-gdb-a-couple-of-notes/
    usleep(1000 * 100);
  }
  mg_stop(mongoose);
  mongoose = NULL;
}
