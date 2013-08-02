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

// Converts the given div's first child element into a draggable object by
// adding mouse/touch event handlers. Adds a configurable amount of latency
// and/or jank to the dragging. Latency is applied continuously, while jank is
// applied intermittently a couple of times per second.
function setupDragging(div, latencyMs, jankMs) {
  div.style.overflow = 'hidden';
  div.style.cursor = 'move';
  div.firstElementChild.style.position = 'relative';
  var mouseX = 0;
  var mouseY = 0;
  var dragX = 0;
  var dragY = 0;
  var nextJank = 0;
  div.onmousedown = function(e) {
    if (e.touches && e.touches[0]) {
      // Convert touch events to look like mouse events so the rest of the code
      // doesn't have to care about the difference.
      e.clientX = e.touches[0].pageX;
      e.clientY = e.touches[0].pageY;
    }
    mouseX = e.clientX;
    mouseY = e.clientY;
    // A drag is starting. Attach global event handlers.
    window.onmousemove = function(e) {
      if (e.touches && e.touches[0]) {
        // Convert touch events to look like mouse events so the rest of the
        // code doesn't have to care about the difference.
        e.clientX = e.touches[0].pageX;
        e.clientY = e.touches[0].pageY;
      }
      // If it's time to apply jank, apply it now.
      var now = Date.now();
      if (now > nextJank) {
        if (now - nextJank < 1000) {
          // Busy wait for the specified amount of jank time.
          var jankEnd = Date.now() + jankMs;
          while (Date.now() < jankEnd);
        }
        // Schedule the next occurrence of jank.
        nextJank = Date.now() + 400;
      }
      // Update the drag position and move the element.
      dragX += e.clientX - mouseX;
      dragY += e.clientY - mouseY;
      mouseX = e.clientX;
      mouseY = e.clientY;
      var newLeft = dragX + 'px';
      var newTop = dragY + 'px';
      var updateDrag = function() {
        div.firstElementChild.style.left = newLeft;
        div.firstElementChild.style.top = newTop;
      };
      // Apply latency to the movement of the element if requested.
      if (latencyMs) {
        window.setTimeout(updateDrag, latencyMs);
      } else {
        updateDrag();
      }
      e.stopPropagation();
      e.preventDefault();
      return false;
    }
    // Also listen to touch events.
    window.ontouchmove = window.onmousemove;
    window.onmouseup = function(e) {
      // The drag is ending. Remove all global event handlers.
      window.onmousemove = null;
      window.ontouchmove = null;
      window.onmouseup = null;
      window.ontouchend = null;
      e.stopPropagation();
      e.preventDefault();
      return false;
    }
    // Also listen to touch events.
    window.ontouchend = window.onmouseup;
    e.stopPropagation();
    e.preventDefault();
    return false;
  }
  // Also listen to touch events.
  div.ontouchstart = div.onmousedown;
}

setupDragging(document.getElementById('goodLatency'), 0, 0);
setupDragging(document.getElementById('badLatency'), 10/60 * 1000, 0);

setupDragging(document.getElementById('goodJank'), 0, 0);
setupDragging(document.getElementById('badJank'), 0, 10/60 * 1000);
