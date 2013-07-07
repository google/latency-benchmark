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

// This file contains the main platform-independent implementation of the
// benchmark server. It serves the files for the test and handles all
// communication with the browser test page, including sending input events,
// taking screenshots, and computing statistics.

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include "screenscraper.h"
#include "latency-benchmark.h"

int64_t last_draw_time = 0;
int64_t biggest_draw_time_gap = 0;

// Updates the given pattern with the given event data, then draws the pattern
// to the current OpenGL context.
void draw_pattern_with_opengl(uint8_t pattern[], int scroll_events,
                              int keydown_events, int esc_presses) {
  int64_t time = get_nanoseconds();
  if (last_draw_time > 0) {
    if (time - last_draw_time > biggest_draw_time_gap) {
      biggest_draw_time_gap = time - last_draw_time;
      debug_log("New biggest draw time gap: %f ms.",
          biggest_draw_time_gap / (double)nanoseconds_per_millisecond);
    }
  }
  last_draw_time = time;
  if (esc_presses == 0) {
    pattern[4 * 4 + 2] = TEST_MODE_JAVASCRIPT_LATENCY;
  } else {
    pattern[4 * 4 + 2] = TEST_MODE_ABORT;
  }
  // Update the pattern with the number of scroll events mod 255.
  pattern[4 * 5] = pattern[4 * 5 + 1] = pattern[4 * 5 + 2] = scroll_events;
  // Update the pattern with the number of keydown events mod 255.
  pattern[4 * 4 + 1] = keydown_events;
  // Increment the "JavaScript frames" counter.
  pattern[4 * 4 + 0]++;
  // Increment the "CSS animation frames" counter.
  pattern[4 * 6 + 0]++;
  pattern[4 * 6 + 1]++;
  pattern[4 * 6 + 2]++;
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  GLint height = viewport[3];
  glDisable(GL_SCISSOR_TEST);
  // Alternate background each frame to make tearing easy to spot.
  float background = 1;
  if (pattern[4 * 4 + 0] % 2 == 1)
    background = 0.8;
  glClearColor(background, background, background, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_SCISSOR_TEST);
  for (int i = 0; i < pattern_bytes; i += 4) {
    glClearColor(pattern[i + 2] / 255.0,
                 pattern[i + 1] / 255.0,
                 pattern[i] / 255.0, 1);
    glScissor(i / 4, height - 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
  }
}


// Parses the magic pattern from a hexadecimal encoded string and fills
// parsed_pattern with the result. parsed_pattern must be a buffer at least
// pattern_magic_bytes long.
bool parse_hex_magic_pattern(const char *encoded_pattern,
                             uint8_t parsed_pattern[]) {
  assert(encoded_pattern);
  assert(parsed_pattern);
  if (strlen(encoded_pattern) != hex_pattern_length) {
    return false;
  }
  bool failed = false;
  for (int i = 0; i < pattern_magic_bytes; i++) {
    // Read the pattern from hex. Every fourth byte is the alpha channel
    // with an expected value of 255.
    if (i % 4 == 3) {
      parsed_pattern[i] = 255;
    } else {
      int hex_index = (i - i / 4) * 2;
      assert(hex_index < hex_pattern_length);
      int current_byte;
      int num_parsed = sscanf(encoded_pattern + hex_index, "%2x",
                              &current_byte);
      failed |= 1 != num_parsed;
      parsed_pattern[i] = current_byte;
    }
  }
  return !failed;
}


// Encodes the given magic pattern into hexadecimal. encoded_pattern must be a
// buffer at least hex_pattern_length + 1 bytes long.
void hex_encode_magic_pattern(const uint8_t magic_pattern[],
                              char encoded_pattern[]) {
  assert(magic_pattern);
  assert(encoded_pattern);
  int written_bytes = 0;
  for (int i = 0; i < pattern_magic_bytes; i++) {
    // Skip alpha bytes.
    if (i % 4 == 3) continue;
    assert(written_bytes < hex_pattern_length - 1);
    snprintf(&encoded_pattern[written_bytes], 3, "%02hhX", magic_pattern[i]);
    written_bytes += 2;
  }
}


// This function works something like memmem, except that it expects needle to
// be 4-byte aligned in haystack (since each pixel is 4 bytes) and it ignores
// every fourth byte (starting with haystack[3]) because those bytes represent
// alpha.
static const uint8_t *find_BGRA_pixels_ignoring_alpha(const uint8_t *haystack,
  size_t haystack_length, const uint8_t *needle, size_t needle_length) {
  const uint8_t *haystack_end = haystack + haystack_length;
  for(const uint8_t *i = haystack; i < haystack_end - needle_length; i += 4) {
    for(size_t j = 0; j < needle_length; j++) {
      if (j % 4 == 3 /* alpha byte */ ||
          i[j] == needle[j] /* non-alpha byte */) {
        if (j == needle_length - 1) return i;
      } else {
        break;
      }
    }
  }
  return NULL;
}


// Locates the given pattern in the screenshot.
static bool find_pattern(const uint8_t magic_pattern[], screenshot *screenshot,
    size_t *out_x, size_t *out_y) {
  assert(out_x && out_y && screenshot->width && screenshot->height);
  const uint8_t *found = find_BGRA_pixels_ignoring_alpha(screenshot->pixels,
      screenshot->stride * screenshot->height, magic_pattern,
      pattern_magic_bytes);
  if (found) {
    size_t offset = (size_t)(found - screenshot->pixels);
    *out_x = (offset % screenshot->stride) / 4;
    *out_y = offset / screenshot->stride;
    return true;
  }
  return false;
}

// This struct holds the data communicated from the test page to the server in
// the test pattern.
typedef struct {
  int64_t screenshot_time;
  uint8_t javascript_frames;
  uint8_t key_down_events;
  uint8_t css_frames;
  uint8_t scroll_position;
  test_mode_t test_mode;
} measurement_t;

// This function takes a small screenshot at the specified position, checks for
// the magic pattern, and then fills in the measurement struct with data
// decoded from the pixels of the pattern. Returns true if successful, false
// if the screenshot failed or the magic pattern was not present.
static bool read_data_from_screen(uint32_t x, uint32_t y,
  const uint8_t magic_pattern[], measurement_t *out) {
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
  if (!find_pattern(magic_pattern, screenshot, &found_x, &found_y) ||
    found_x || found_y) {
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
  debug_log("javascript frames: %d, javascript events: %d, scroll position: %d"
      ", css frames: %d, test mode: %d", out->javascript_frames,
      out->key_down_events, out->scroll_position, out->css_frames,
      out->test_mode);
  return true;
}

// Each value reported in the measurement struct is tracked by a statistic
// struct that records the length of time between changes.
typedef struct {
  int64_t previous_change_time;  // The last time the value changed.
  int value;                     // The last value seen.
  int value_delta;               // The amount the value changed last time.
  int measurements;              // The number of measurements taken.
  // We want to know the amount of time it took the value to change, but we
  // can't take screenshots fast enough to pin it down exactly. When we see a
  // screenshot with a changed value, we can only tell that the value changed
  // sometime during the period between the current screenshot and the previous
  // one. This period may be tens of milliseconds. To deal with this we record
  // two times: the time of the previous screenshot, and the time of the current
  // one. These correspond to a lower and upper bound on the time when the value
  // actually changed. Ideally, screenshots will be frequent enough that the
  // difference between these times is small.
  // These variables hold the sum of the time for all measurements; divide by
  // the number of measurements to get the average time.
  int64_t lower_bound_time;
  int64_t upper_bound_time;
  // This records the longest length of time during which the value did not
  // change.
  int64_t max_lower_bound;
  char *name;
} statistic;


// Updates a statistic struct with a new value from a recent measurement.
static bool update_statistic(statistic *stat, int value, int64_t screenshot_time,
    int64_t previous_screenshot_time) {
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
  } else if (screenshot_duration > 20 * nanoseconds_per_millisecond &&
             lower_bound_time < 5 * nanoseconds_per_millisecond) {
    debug_log("%s: Ignoring measurement due to slow screenshot.", stat->name);
  } else {
    // Record the measurement.
    stat->measurements++;
    stat->upper_bound_time += screenshot_time - stat->previous_change_time;
    stat->lower_bound_time += lower_bound_time;
    if (lower_bound_time > stat->max_lower_bound) {
      debug_log("%s: updated max_lower_bound to %f", stat->name,
          lower_bound_time / (double)nanoseconds_per_millisecond);
      stat->max_lower_bound = lower_bound_time;
    }
  }
  stat->previous_change_time = screenshot_time;
  stat->value = value;
  stat->value_delta += change;
  return true;
}

// Returns the average upper bound time for a statistic, in milliseconds.
static double upper_bound_ms(statistic *stat) {
  double bound = stat->upper_bound_time / (double) stat->measurements /
      nanoseconds_per_millisecond;
  // Guard for NaN resulting from divide-by-zero.
  if (bound != bound)
    bound = 0;
  return bound;
}

// Returns the average upper bound time for a statistic, in milliseconds.
static double lower_bound_ms(statistic *stat) {
  double bound = stat->lower_bound_time / (double) stat->measurements /
      nanoseconds_per_millisecond;
  // Guard for NaN resulting from divide-by-zero.
  if (bound != bound)
    bound = 0;
  return bound;
}

// Initializes a statistic struct.
static void init_statistic(char *name, statistic *stat, int value,
    int64_t start_time) {
  memset(stat, 0, sizeof(statistic));
  stat->value = value;
  stat->previous_change_time = start_time;
  stat->name = name;
}


static const int64_t test_timeout_ms = 40000;
static const int64_t event_response_timeout_ms = 4000;
static const int latency_measurements_to_take = 50;

// Main test function. Locates the given magic pixel pattern on the screen, then
// runs one full latency test, sending input events and recording responses. On
// success, the results of the test are reported in the output parameters, and
// true is returned. If the test fails, the error parameter is filled in with
// an error message and false is returned.
bool measure_latency(
    const uint8_t magic_pattern[],
    double *out_key_down_latency_ms,
    double *out_scroll_latency_ms,
    double *out_max_js_pause_time_ms,
    double *out_max_css_pause_time_ms,
    double *out_max_scroll_pause_time_ms,
    char **error) {
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
  bool first_screenshot_successful = read_data_from_screen((uint32_t)x,
      (uint32_t) y, magic_pattern, &measurement);
  if (!first_screenshot_successful) {
    *error = "Failed to read data from test pattern.";
    return false;
  }
  if (measurement.test_mode == TEST_MODE_NATIVE_REFERENCE) {
    uint8_t *test_pattern = (uint8_t *)malloc(pattern_bytes);
    memset(test_pattern, 0, pattern_bytes);
    for (int i = 0; i < pattern_magic_bytes; i++) {
      test_pattern[i] = rand();
    }
    if (!open_native_reference_window(test_pattern)) {
      *error = "Failed to open native reference window.";
      return false;
    }
    bool return_value = measure_latency(test_pattern, out_key_down_latency_ms, out_scroll_latency_ms, out_max_js_pause_time_ms, out_max_css_pause_time_ms, out_max_scroll_pause_time_ms, error);
    if (!close_native_reference_window()) {
      debug_log("Failed to close native reference window.");
    };
    return return_value;
  }
  int64_t start_time = measurement.screenshot_time;
  previous_measurement = measurement;
  statistic javascript_frames;
  statistic css_frames;
  statistic key_down_events;
  statistic scroll_stats;
  init_statistic("javascript_frames", &javascript_frames,
      measurement.javascript_frames, start_time);
  init_statistic("key_down_events", &key_down_events,
      measurement.key_down_events, start_time);
  init_statistic("css_frames", &css_frames, measurement.css_frames, start_time);
  init_statistic("scroll", &scroll_stats, measurement.scroll_position,
      start_time);
  int sent_events = 0;
  int scroll_x = x + 40;
  int scroll_y = y + 40;
  int64_t last_scroll_sent = start_time;
  if (measurement.test_mode == TEST_MODE_SCROLL_LATENCY) {
    send_scroll_down(scroll_x, scroll_y);
    scroll_stats.previous_change_time = get_nanoseconds();
  }
  while(true) {
    bool screenshot_successful = read_data_from_screen((uint32_t)x,
        (uint32_t) y, magic_pattern, &measurement);
    if (!screenshot_successful) {
      *error = "Test window moved during test. The test window must remain "
          "stationary and focused during the entire test.";
      return false;
    }
    if (measurement.test_mode == TEST_MODE_ABORT) {
      *error = "Test aborted.";
      return false;
    }
    screenshots++;
    int64_t screenshot_time = measurement.screenshot_time;
    int64_t previous_screenshot_time = previous_measurement.screenshot_time;
    debug_log("screenshot time %f",
        (screenshot_time - previous_screenshot_time) /
            (double)nanoseconds_per_millisecond);
    update_statistic(&javascript_frames, measurement.javascript_frames,
        screenshot_time, previous_screenshot_time);
    update_statistic(&key_down_events, measurement.key_down_events,
        screenshot_time, previous_screenshot_time);
    update_statistic(&css_frames, measurement.css_frames, screenshot_time,
        previous_screenshot_time);
    bool scroll_updated = update_statistic(&scroll_stats,
        measurement.scroll_position, screenshot_time, previous_screenshot_time);

    if (measurement.test_mode == TEST_MODE_JAVASCRIPT_LATENCY) {
      if (key_down_events.measurements >= latency_measurements_to_take) {
        break;
      }
      if (key_down_events.value_delta > sent_events) {
        *error = "More events received than sent! This is probably a bug in "
            "the test.";
        return false;
      }
      if (screenshot_time - key_down_events.previous_change_time >
          event_response_timeout_ms * nanoseconds_per_millisecond) {
        *error = "Browser did not respond to keyboard input. Make sure the "
            "test page remains focused for the entire test.";
        return false;
      }
      if (key_down_events.value_delta == sent_events) {
        // We want to avoid sending input events at a predictable time relative
        // to frames, so introduce a random delay of up to 1 frame (16.67 ms)
        // before sending the next event.
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
        if (screenshot_time - scroll_stats.previous_change_time >
            event_response_timeout_ms * nanoseconds_per_millisecond) {
          *error = "Browser did not respond to scroll events. Make sure the "
              "test page remains focused for the entire test.";
          return false;
        }
        if (scroll_updated) {
          // We saw the start of a scroll. Wait for the scroll animation to
          // finish before continuing. We assume the animation is finished if
          // it's been 100 milliseconds since we last saw the scroll position
          // change.
          int64_t scroll_update_time = screenshot_time;
          int64_t scroll_wait_start_time = screenshot_time;
          while (screenshot_time - scroll_update_time <
                 100 * nanoseconds_per_millisecond) {
            screenshot_successful = read_data_from_screen((uint32_t)x,
                (uint32_t) y, magic_pattern, &measurement);
            if (!screenshot_successful) {
              *error = "Test window moved during test. The test window must "
                  "remain stationary and focused during the entire test.";
              return false;
            }
            screenshot_time = measurement.screenshot_time;
            if (screenshot_time - scroll_wait_start_time >
                nanoseconds_per_second) {
              *error = "Browser kept scrolling for more than 1 second after a "
                  "single scrollwheel event.";
              return false;
            }
            if (measurement.scroll_position != scroll_stats.value) {
              scroll_stats.value = measurement.scroll_position;
              scroll_update_time = screenshot_time;
            }
          }
          // We want to avoid sending input events at a predictable time
          // relative to frames, so introduce a random delay of up to 1 frame
          // (16.67 ms) before sending the next event.
          usleep((rand() % 17) * 1000);
          send_scroll_down(scroll_x, scroll_y);
          scroll_stats.previous_change_time = get_nanoseconds();
        }
    } else if (measurement.test_mode == TEST_MODE_PAUSE_TIME) {
      // For the pause time test we want the browser to scroll continuously.
      // Send a scroll event every frame.
      if (screenshot_time - last_scroll_sent >
          17 * nanoseconds_per_millisecond) {
        send_scroll_down(scroll_x, scroll_y);
        last_scroll_sent = get_nanoseconds();
      }
    } else if (measurement.test_mode == TEST_MODE_PAUSE_TIME_TEST_FINISHED) {
      break;
    } else {
      *error = "Invalid test type. This is a bug in the test.";
      return false;
    }

    if (screenshot_time - start_time >
        test_timeout_ms * nanoseconds_per_millisecond) {
      *error = "Timeout.";
      return false;
    }
    previous_measurement = measurement;
    usleep(0);
  }
  // The latency we report is the midpoint of the interval given by the average
  // upper and lower bounds we've computed.
  *out_key_down_latency_ms =
      (upper_bound_ms(&key_down_events) + lower_bound_ms(&key_down_events)) / 2;
  *out_scroll_latency_ms =
      (upper_bound_ms(&scroll_stats) + lower_bound_ms(&scroll_stats) / 2);
  *out_max_js_pause_time_ms =
      javascript_frames.max_lower_bound / (double) nanoseconds_per_millisecond;
  *out_max_css_pause_time_ms =
      css_frames.max_lower_bound / (double) nanoseconds_per_millisecond;
  *out_max_scroll_pause_time_ms =
      scroll_stats.max_lower_bound / (double) nanoseconds_per_millisecond;
  debug_log("out_key_down_latency_ms: %f out_scroll_latency_ms: %f "
      "out_max_js_pause_time_ms: %f out_max_css_pause_time: %f\n "
      "out_max_scroll_pause_time_ms: %f",
      *out_key_down_latency_ms,
      *out_scroll_latency_ms,
      *out_max_js_pause_time_ms,
      *out_max_css_pause_time_ms,
      *out_max_scroll_pause_time_ms);
  return true;
}
