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

var keepServerAliveRequestsSent = 0;
var keepServerAliveRequest = null;
var serverStatusAlive = document.getElementById('serverStatusAlive');
var serverStatusDead = document.getElementById('serverStatusDead');
var running = false;
var pageNavigated = false;
window.onbeforeunload = function() {
  pageNavigated = true;
}
function sendKeepServerAliveRequest() {
  keepServerAliveRequest = new XMLHttpRequest();
  keepServerAliveRequest.open('GET', '/keepServerAlive?randomNumber=' + Math.random(), true);
  keepServerAliveRequest.onreadystatechange = function() {
    if (pageNavigated) return;
    if (!running && keepServerAliveRequest.readyState < 4 && keepServerAliveRequest.readyState > 2) {
      running = true;
      if (serverStatusAlive && serverStatusDead) {
        serverStatusAlive.style.display = 'block';
        serverStatusDead.style.display = 'none';
      }
    }
    if (keepServerAliveRequest.readyState == 4) {
      running = false;
      if (serverStatusAlive && serverStatusDead) {
        serverStatusAlive.style.display = 'none';
        serverStatusDead.style.display = 'block';
      }
      window.setTimeout(sendKeepServerAliveRequest, 250);
    }
  };
  keepServerAliveRequest.onprogress = function() {
    if (keepServerAliveRequest.readyState == 3) {
      var response = keepServerAliveRequest.response;
      var last = response.slice(response.length - 1);
      var latencyTester = last == '1';
      if (latencyTester && !/hardware-latency-test/.test(window.location.pathname)) {
        window.location.href = '/hardware-latency-test.html';
      }
      if (!latencyTester && /hardware-latency-test/.test(window.location.pathname)) {
        window.location.href = '/';
      }
    }
  }
  keepServerAliveRequest.send();
}
sendKeepServerAliveRequest();
