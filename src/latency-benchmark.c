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

#include "../third_party/mongoose/mongoose.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include "screenscraper.h"


// This function works something like memmem, except that it expects needle to be 4-byte aligned in haystack (since each pixel is 4 bytes) and it ignores every fourth byte (starting with haystack[3]) because those bytes represent alpha.
static const uint8_t *find_BGRA_pixels_ignoring_alpha(const uint8_t *haystack, size_t haystack_length, const uint8_t *needle, size_t needle_length) {
    const uint8_t *haystack_end = haystack + haystack_length;
    for(const uint8_t *i = haystack; i < haystack_end - needle_length; i += 4) {
        for(size_t j = 0; j < needle_length; j++) {
            if (j % 4 == 3 /* alpha byte */ || i[j] == needle[j] /* non-alpha byte */) {
                if (j == needle_length - 1)
                    return i;
            } else {
                break;
            }
        }
    }
    return NULL;
}


static bool find_pattern(const uint8_t magic_pattern[], screenshot *screenshot, size_t *out_x, size_t *out_y) {
    assert(out_x && out_y && screenshot->width && screenshot->height);
    const uint8_t *found = find_BGRA_pixels_ignoring_alpha(screenshot->pixels, screenshot->stride * screenshot->height, magic_pattern, pattern_magic_bytes);
    if (found) {
        size_t offset = (size_t)(found - screenshot->pixels);
        *out_x = (offset % screenshot->stride) / 4;
        *out_y = offset / screenshot->stride;
        return true;
    }
    return false;
}

typedef enum {
    TEST_MODE_JAVASCRIPT_LATENCY = 1,
    TEST_MODE_SCROLL_LATENCY = 2,
    TEST_MODE_PAUSE_TIME = 3,
    TEST_MODE_PAUSE_TIME_TEST_FINISHED = 4
} test_mode_t;

typedef struct {
    int64_t screenshot_time;
    uint8_t javascript_frames;
    uint8_t key_down_events;
    uint8_t css_frames;
    uint8_t scroll_position;
    test_mode_t test_mode;
} measurement_t;

static bool read_data_from_screen(uint32_t x, uint32_t y, const uint8_t magic_pattern[], measurement_t *out) {
    assert(out);
    screenshot *screenshot = take_screenshot(x, y, pattern_pixels, 1);
    if (!screenshot) {
        return false;
    }
    if (screenshot->width != pattern_pixels) {
        free_screenshot(screenshot);
        return false;
    }
    // Check that the magic pattern is there, starting at the first pixel.
    size_t found_x, found_y;
    if (!find_pattern(magic_pattern, screenshot, &found_x, &found_y) || found_x || found_y) {
        free_screenshot(screenshot);
        return false;
    }
    out->javascript_frames = screenshot->pixels[pattern_magic_pixels * 4 + 0];
    out->key_down_events = screenshot->pixels[pattern_magic_pixels * 4 + 1];
    out->test_mode = (test_mode_t) screenshot->pixels[pattern_magic_pixels * 4 + 2];
    out->scroll_position = screenshot->pixels[(pattern_magic_pixels + 1) * 4];
    out->css_frames = screenshot->pixels[(pattern_magic_pixels + 2) * 4];
    out->screenshot_time = screenshot->time_nanoseconds;
    free_screenshot(screenshot);
    debug_log("javascript frames: %d, javascript events: %d, scroll position: %d, css frames: %d, test mode: %d", out->javascript_frames, out->key_down_events, out->scroll_position, out->css_frames, out->test_mode);
    return true;
}


static bool is_latency_test_request(const struct mg_request_info *request_info, uint8_t magic_pattern[]) {
    assert(magic_pattern);
    // A valid test request will have the path /test and must specify a magic pattern in the magicPattern query variable. The pattern is specified as a string of hex digits and must be the exact length expected (3 bytes for each pixel in the pattern).
    // Here is an example of a valid request: http://localhost:5578/test?magicPattern=8a36052d02c596dfa4c80711
    if (strcmp(request_info->uri, "/test") == 0) {
        const int hex_pattern_length = pattern_magic_pixels * 3 * 2;
        char hex_pattern[hex_pattern_length + 1];
        if (hex_pattern_length == mg_get_var(request_info->query_string, strlen(request_info->query_string), "magicPattern", hex_pattern, hex_pattern_length + 1)) {
            bool failed = false;
            for (int i = 0; i < pattern_magic_bytes; i++) {
                // Read the pattern from hex. Every fourth byte is the alpha channel with an expected value of
                // 255.
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

typedef struct {
    int64_t previous_change_time;
    int value;
    int value_delta;
    int64_t lower_bound_time;
    int64_t upper_bound_time;
    int measurements;
    int64_t max_lower_bound;
    char *name;
} statistic;


bool update_statistic(statistic *stat, int value, int64_t screenshot_time, int64_t previous_screenshot_time) {
    assert(value >= 0 && stat->value >= 0);
    int change = value - stat->value;
    if (change < 0) {
        // Handle values that wrap at 255.
        assert(stat->value < 256 && value < 256);
        change += 256;
        assert(change > 0);
    }
    assert(change >= 0);
    if (change == 0) {
        return false;
    }
    int64_t lower_bound_time = previous_screenshot_time - stat->previous_change_time;
    int64_t screenshot_duration = screenshot_time - previous_screenshot_time;
    if (lower_bound_time <= 0) {
        debug_log("%s: Didn't get a screenshot before response.", stat->name);
    } else if (screenshot_duration > 20 * nanoseconds_per_millisecond && lower_bound_time < 5 * nanoseconds_per_millisecond) {
        debug_log("%s: Ignoring measurement due to slow screenshot.", stat->name);
    } else {
        // Record the measurement.
        stat->measurements++;
        stat->upper_bound_time += screenshot_time - stat->previous_change_time;
        stat->lower_bound_time += lower_bound_time;
        if (lower_bound_time > stat->max_lower_bound) {
            debug_log("%s: updated max_lower_bound to %f", stat->name, lower_bound_time / (double)nanoseconds_per_millisecond);
            stat->max_lower_bound = lower_bound_time;
        }
    }
    stat->previous_change_time = screenshot_time;
    stat->value = value;
    stat->value_delta += change;
    return true;
}

double upper_bound_ms(statistic *stat) {
    double bound = stat->upper_bound_time / (double) stat->measurements / nanoseconds_per_millisecond;
    if (bound != bound)
        bound = 0;
    return bound;
}

double lower_bound_ms(statistic *stat) {
    double bound = stat->lower_bound_time / (double) stat->measurements / nanoseconds_per_millisecond;
    if (bound != bound)
        bound = 0;
    return bound;
}

void init_statistic(char *name, statistic *stat, int value, int64_t start_time) {
    memset(stat, 0, sizeof(statistic));
    stat->value = value;
    stat->previous_change_time = start_time;
    stat->name = name;
}


static const int64_t test_timeout_ms = 40000;
static const int64_t event_response_timeout_ms = 4000;
static const int latency_measurements_to_take = 50;
static bool measure_latency(double *out_key_down_latency_ms, double *out_scroll_latency_ms, double *out_max_js_pause_time_ms, double *out_max_css_pause_time_ms, double *out_max_scroll_pause_time_ms, char **error, const uint8_t magic_pattern[]) {
    screenshot *screenshot = take_screenshot(0, 0, UINT32_MAX, UINT32_MAX);
    if (!screenshot) {
        *error = "Failed to take screenshot.";
        return false;
    }
    assert(screenshot->width > 0 && screenshot->height > 0);

    size_t x, y;
    bool found_pattern = find_pattern(magic_pattern, screenshot, &x, &y);
    free_screenshot(screenshot);
    if (!found_pattern) {
        *error = "Failed to find test pattern on screen.";
        return false;
    }
    uint8_t full_pattern[pattern_bytes];
    for (int i = 0; i < pattern_magic_bytes; i++) {
        full_pattern[i] = magic_pattern[i];
    }
    measurement_t measurement;
    measurement_t previous_measurement;
    memset(&measurement, 0, sizeof(measurement_t));
    memset(&previous_measurement, 0, sizeof(measurement_t));
    int screenshots = 0;
    bool first_screenshot_successful = read_data_from_screen((uint32_t)x, (uint32_t) y, magic_pattern, &measurement);
    if (!first_screenshot_successful) {
        *error = "Failed to read data from test pattern.";
        return false;
    }
    int64_t start_time = measurement.screenshot_time;
    previous_measurement = measurement;
    statistic javascript_frames;
    statistic css_frames;
    statistic key_down_events;
    statistic scroll_stats;
    init_statistic("javascript_frames", &javascript_frames, measurement.javascript_frames, start_time);
    init_statistic("key_down_events", &key_down_events, measurement.key_down_events, start_time);
    init_statistic("css_frames", &css_frames, measurement.css_frames, start_time);
    init_statistic("scroll", &scroll_stats, measurement.scroll_position, start_time);
    int sent_events = 0;
    int64_t previous_screenshot_time = start_time;
    int scroll_x = x + 40;
    int scroll_y = y + 40;
    int64_t last_scroll_sent = start_time;
    if (measurement.test_mode == TEST_MODE_SCROLL_LATENCY) {
        send_scroll_down(scroll_x, scroll_y);
        scroll_stats.previous_change_time = get_nanoseconds();
    }
    while(true) {
        bool screenshot_successful = read_data_from_screen((uint32_t)x, (uint32_t) y, magic_pattern, &measurement);
        if (!screenshot_successful) {
            *error = "Test window moved during test. The test window must remain stationary and focused during the entire test.";
            return false;
        }
        screenshots++;
        int64_t screenshot_time = measurement.screenshot_time;
        int64_t previous_screenshot_time = previous_measurement.screenshot_time;
        debug_log("screenshot time %f", (screenshot_time - previous_screenshot_time) / (double)nanoseconds_per_millisecond);
        update_statistic(&javascript_frames, measurement.javascript_frames, screenshot_time, previous_screenshot_time);
        update_statistic(&key_down_events, measurement.key_down_events, screenshot_time, previous_screenshot_time);
        update_statistic(&css_frames, measurement.css_frames, screenshot_time, previous_screenshot_time);
        bool scroll_updated = update_statistic(&scroll_stats, measurement.scroll_position, screenshot_time, previous_screenshot_time);

        if (measurement.test_mode == TEST_MODE_JAVASCRIPT_LATENCY) {
            if (key_down_events.measurements >= latency_measurements_to_take) {
                break;
            }
            if (key_down_events.value_delta > sent_events) {
                *error = "More events received than sent! This is probably a bug in the test.";
                return false;
            }
            if (screenshot_time - key_down_events.previous_change_time > event_response_timeout_ms * nanoseconds_per_millisecond) {
                *error = "Browser did not respond to keyboard input. Make sure the test page remains focused for the entire test.";
                return false;
            }
            if (key_down_events.value_delta == sent_events) {
                // We want to avoid sending input events at a predictable time relative to frames, so introduce a random delay of up to 1 frame (16.67 ms) before sending the next event.
                usleep((rand() % 17) * 1000);
                if (!send_keystroke_z()) {
                    *error = "Failed to send keystroke for \"Z\" key to test window.";
                    return false;
                }
                key_down_events.previous_change_time = get_nanoseconds();
                sent_events++;
            }
        } else if (measurement.test_mode == TEST_MODE_SCROLL_LATENCY) {
            if (scroll_stats.measurements >= latency_measurements_to_take) {
                break;
            }
            if (screenshot_time - scroll_stats.previous_change_time > event_response_timeout_ms * nanoseconds_per_millisecond) {
                *error = "Browser did not respond to scroll events. Make sure the test page remains focused for the entire test.";
                return false;
            }
            if (scroll_updated) {
                // We saw the start of a scroll. Wait for the scroll animation to finish before continuing. We assume the animation is finished if it's been 100 milliseconds since we last saw the scroll position change.
                int64_t scroll_update_time = screenshot_time;
                int64_t scroll_wait_start_time = screenshot_time;
                while (screenshot_time - scroll_update_time < 100 * nanoseconds_per_millisecond) {
                    screenshot_successful = read_data_from_screen((uint32_t)x, (uint32_t) y, magic_pattern, &measurement);
                    if (!screenshot_successful) {
                        *error = "Test window moved during test. The test window must remain stationary and focused during the entire test.";
                        return false;
                    }
                    screenshot_time = measurement.screenshot_time;
                    if (screenshot_time - scroll_wait_start_time > nanoseconds_per_second) {
                        *error = "Browser kept scrolling for more than 1 second after a single scrollwheel event.";
                        return false;
                    }
                    if (measurement.scroll_position != scroll_stats.value) {
                        scroll_stats.value = measurement.scroll_position;
                        scroll_update_time = screenshot_time;
                    }
                }
                // We want to avoid sending input events at a predictable time relative to frames, so introduce a random delay of up to 1 frame (16.67 ms) before sending the next event.
                usleep((rand() % 17) * 1000);
                send_scroll_down(scroll_x, scroll_y);
                scroll_stats.previous_change_time = get_nanoseconds();
            }
        } else if (measurement.test_mode == TEST_MODE_PAUSE_TIME) {
            // For the pause time test we want the browser to scroll continuously. Send a scroll event every frame.
            if (screenshot_time - last_scroll_sent > 17 * nanoseconds_per_millisecond) {
                send_scroll_down(scroll_x, scroll_y);
                last_scroll_sent = get_nanoseconds();
            }
        } else if (measurement.test_mode == TEST_MODE_PAUSE_TIME_TEST_FINISHED) {
            break;
        } else {
            *error = "Invalid test type. This is a bug in the test.";
            return false;
        }

        if (screenshot_time - start_time > test_timeout_ms * nanoseconds_per_millisecond) {
            *error = "Timeout.";
            return false;
        }
        previous_measurement = measurement;
        usleep(0);
    }
    // The latency we report is the midpoint of the interval given by the upper and lower bounds we've computed.
    *out_key_down_latency_ms = (upper_bound_ms(&key_down_events) + lower_bound_ms(&key_down_events)) / 2;
    *out_scroll_latency_ms = (upper_bound_ms(&scroll_stats) + lower_bound_ms(&scroll_stats) / 2);
    *out_max_js_pause_time_ms = javascript_frames.max_lower_bound / (double) nanoseconds_per_millisecond;
    *out_max_css_pause_time_ms = css_frames.max_lower_bound / (double) nanoseconds_per_millisecond;
    *out_max_scroll_pause_time_ms = scroll_stats.max_lower_bound / (double) nanoseconds_per_millisecond;
    debug_log("out_key_down_latency_ms: %f out_scroll_latency_ms: %f out_max_js_pause_time_ms: %f out_max_css_pause_time: %f\n out_max_scroll_pause_time_ms: %f", *out_key_down_latency_ms, *out_scroll_latency_ms, *out_max_js_pause_time_ms, *out_max_css_pause_time_ms, *out_max_scroll_pause_time_ms);
    return true;
}


struct mg_context *mongoose = NULL;

bool check_url_returns_HTTP_200(char *host, int port, char *path) {
    assert(mongoose);
    char error_buffer[1000];
    error_buffer[0] = 0;
    struct mg_connection *connection = mg_download(host, port, /* use_ssl */ false, error_buffer, sizeof(error_buffer), "GET %s HTTP/1.0\r\n\r\n", path);
    // Mongoose stores the HTTP response code in the URI field of the mg_request_info struct.
    struct mg_request_info *response = mg_get_request_info(connection);
    bool received_200 = strcmp("200", response->uri) == 0;
    mg_close_connection(connection);
    return received_200;
}

static void report_latency(struct mg_connection *connection, const uint8_t magic_pattern[]) {
    double key_down_latency_ms = 0;
    double scroll_latency_ms = 0;
    double max_js_pause_time_ms = 0;
    double max_css_pause_time_ms = 0;
    double max_scroll_pause_time_ms = 0;
    char *error = "Unknown error.";
    if (!measure_latency(&key_down_latency_ms, &scroll_latency_ms, &max_js_pause_time_ms, &max_css_pause_time_ms, &max_scroll_pause_time_ms, &error, magic_pattern)) {
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
                    "{ \"keyDownLatencyMs\": %f, \"scrollLatencyMs\": %f, \"maxJSPauseTimeMs\": %f, \"maxCssPauseTimeMs\": %f, \"maxScrollPauseTimeMs\": %f}", key_down_latency_ms, scroll_latency_ms, max_js_pause_time_ms, max_css_pause_time_ms, max_scroll_pause_time_ms);
    }
}


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
        //
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
        // NOTE: If you are debugging using GDB or XCode, you may encounter signal SIGPIPE on this line. SIGPIPE is harmless and you should configure your debugger to ignore it. For instructions see here:
        // http://stackoverflow.com/questions/10431579/permanently-configuring-lldb-in-xcode-4-3-2-not-to-stop-on-signals
        // http://ricochen.wordpress.com/2011/07/14/debugging-with-gdb-a-couple-of-notes/
        usleep(1000 * 100);
    }
    mg_stop(mongoose);
    mongoose = NULL;
}
