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

#ifndef WLB_LATENCY_BENCHMARK_H_
#define WLB_LATENCY_BENCHMARK_H_

// The test mode is communicated from the test page to the server as one of the
// pixel values in the test pattern.
typedef enum {
  TEST_MODE_JAVASCRIPT_LATENCY = 1,
  TEST_MODE_SCROLL_LATENCY = 2,
  TEST_MODE_PAUSE_TIME = 3,
  TEST_MODE_PAUSE_TIME_TEST_FINISHED = 4
} test_mode_t;

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
    char **error);

#endif  // WLB_LATENCY_BENCHMARK_H_
