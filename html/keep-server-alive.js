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
//document.write('<p id="serverStatus">Server status: <span id="statusSpan">Not connected</span></p>')
//var statusSpan = document.getElementById('statusSpan');
var running = false;
function sendKeepServerAliveRequest() {
  keepServerAliveRequest = new XMLHttpRequest();
  keepServerAliveRequest.open('GET', '/keepServerAlive?randomNumber=' + Math.random(), true);
  keepServerAliveRequest.onreadystatechange = function() {
    if (!running && keepServerAliveRequest.readyState < 4 && keepServerAliveRequest.readyState > 2) {
      running = true;
//      statusSpan.textContent = 'Running';
    }
    if (keepServerAliveRequest.readyState == 4) {
      running = false;
//      statusSpan.textContent = 'Not running';
      window.setTimeout(sendKeepServerAliveRequest, 250);
    }
  };
  keepServerAliveRequest.send();
}
sendKeepServerAliveRequest();
