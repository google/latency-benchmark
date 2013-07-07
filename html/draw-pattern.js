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

// We draw a pattern on the screen, encoding information in the colors that can be read by the server in screenshots. We can encode three bytes per pixel, ignoring the alpha channel. The pattern starts with a "magic" identification number, then encodes information about the state of the page.

// Make the page background a repeating gradient from rgb(0, 0, 0) to rgb(255, 255, 255). This allows the server to read the page's scroll position (mod 255). This background will be almost entirely covered by other content, leaving only one pixel visible for the server to read.
document.body.style.backgroundImage = 'url("gradient.png")';
document.body.style.backgroundSize = '1px ' + 256 / window.devicePixelRatio + 'px';
document.body.style.height = '1000000px';

var testContainer = document.createElement('div');
setPrefixed('transformOrigin', 'top left', testContainer.style);
setPrefixed('transform', 'scale(' + (1 / window.devicePixelRatio) + ')', testContainer.style);
setPrefixed('position', 'fixed', testContainer.style);
setPrefixed('top', '0px', testContainer.style);
setPrefixed('left', '1px', testContainer.style);
setPrefixed('width', '1000%', testContainer.style);
setPrefixed('height', '10px', testContainer.style);
setPrefixed('overflow', 'hidden', testContainer.style);

var canvas2d = document.createElement('canvas');
var context2D = canvas2d.getContext('2d');
var canvasGL = document.createElement('canvas');
setPrefixed('position', 'absolute', canvasGL.style);
setPrefixed('top', '0px', canvasGL.style);
setPrefixed('left', '0px', canvasGL.style);
canvasGL.width = 500;
canvasGL.height = 1;
var gl = null;
try {
  gl = canvasGL.getContext('webgl') ||
       canvasGL.getContext('experimental-webgl') ||
       canvasGL.getContext('moz-webgl') ||
       canvasGL.getContext('o-webgl') ||
       canvasGL.getContext('ms-webgl') ||
       canvasGL.getContext('webkit-3d') ||
       canvasGL.getContext('3d');
} catch (e) {}
if (!gl) {
  var notgl = canvasGL.getContext('2d');
}
testContainer.appendChild(canvasGL);


var gradientImage = document.createElement('img');
gradientImage.src = 'gradient.png';
setPrefixed('position', 'absolute', gradientImage.style);
setPrefixed('top', '0', gradientImage.style);
setPrefixed('left', '0', gradientImage.style);
setPrefixed('animationDuration', '4.26666667s', gradientImage.style);  // 1 pixel per second
setPrefixed('animationName', 'gradientImage', gradientImage.style);
setPrefixed('animationTimingFunction', 'linear', gradientImage.style);
setPrefixed('animationIterationCount', 'infinite', gradientImage.style);
setPrefixed('transformOrigin', '0px 0px 0px', gradientImage.style);

var keyframesCss = '@{prefix}keyframes gradientImage {' +
                   'from {{prefix}transform: translate(6px, 0px); }' +
                   'to {{prefix}transform: translate(6px, -255px); }}';
var keyframesCssWithPrefix = '';
for (var i = 0; i < cssPrefixes.length; i++) {
  keyframesCssWithPrefix += keyframesCss.replace(/{prefix}/g, cssPrefixes[i]);
}
var newStyleSheet = document.createElement('style');
newStyleSheet.textContent = keyframesCssWithPrefix;
document.head.appendChild(newStyleSheet);
testContainer.appendChild(gradientImage);
var rightBlocker = document.createElement('div');
var bottomBlocker = document.createElement('div');
rightBlocker.style.position = 'absolute';
rightBlocker.style.left = '7px';
rightBlocker.style.top = '0px';
rightBlocker.style.background = 'black';
rightBlocker.style.width = '100%';
rightBlocker.style.height = '100%';
bottomBlocker.style.position = 'absolute';
bottomBlocker.style.left = '0px';
bottomBlocker.style.top = '1px';
bottomBlocker.style.background = 'black';
bottomBlocker.style.width = '100%';
bottomBlocker.style.height = '100%';
testContainer.appendChild(rightBlocker);
testContainer.appendChild(bottomBlocker);
document.body.appendChild(testContainer);
var leftBlocker = document.createElement('div');
leftBlocker.style.position = 'fixed';
leftBlocker.style.left = '0px';
leftBlocker.style.top = '0px';
leftBlocker.style.background = 'black';
leftBlocker.style.width = '1px';
leftBlocker.style.height = '10px';
document.body.appendChild(leftBlocker);

var zPresses = 0;
window.onkeydown = function(e) {
  if (e.keyCode == 90) {
    zPresses++;
  }
  // If Esc is pressed, abort the current test.
  if (e.keyCode == 27) {
    testMode = TEST_MODES.ABORT;
  }
};


var frames = 0;
var patternPixels = 5;
var patternBytes = patternPixels * 3;
var randomByte = function() {
  return (Math.random() * 256) | 0;
}
var magicPattern = [ 138, 54, 5, 45, 2, 197, randomByte(), randomByte(), randomByte(), randomByte(), randomByte(), randomByte() ];
var byteToHex = function(byte) {
  var hex = byte.toString(16);
  if(hex.length == 1) hex = '0' + hex;
  return hex;
};
var magicPatternHex = '';
for (var i = 0; i < magicPattern.length; i++) {
  magicPatternHex += byteToHex(magicPattern[i]);
}
var raf = getPrefixed('requestAnimationFrame', window) || function(callback) { window.setTimeout(callback, 16.67); };
var patternBuffer = new ArrayBuffer(patternBytes);
var patternByteArray = new Uint8Array(patternBuffer);
patternByteArray.set(magicPattern);
var testMode = 0;
var TEST_MODES = {
  JAVASCRIPT_LATENCY: 1,
  SCROLL_LATENCY: 2,
  PAUSE_TIME: 3,
  PAUSE_TIME_TEST_FINISHED: 4,
  NATIVE_REFERENCE: 5,
  ABORT: 6,
}
var callback = function() {
  raf(callback);
  frames++;
  patternByteArray[magicPattern.length + 0] = frames;
  patternByteArray[magicPattern.length + 1] = zPresses;
  patternByteArray[magicPattern.length + 2] = testMode;
  if (gl) {
    gl.clearColor(0, 0, 0, 0);
    gl.disable(gl.SCISSOR_TEST);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.enable(gl.SCISSOR_TEST);
    for (var i = 0; i < patternByteArray.length - 2; i += 3) {
      gl.scissor(i / 3, 0, 1, 100);
      gl.clearColor(patternByteArray[i + 2] / 255, patternByteArray[i + 1] / 255, patternByteArray[i + 0] / 255, 1);
      gl.clear(gl.COLOR_BUFFER_BIT);
    }
  } else {
    notgl.clearRect(0, 0, 10000, 10000);
    for (var i = 0; i < patternByteArray.length - 2; i += 3) {
        notgl.fillStyle = "rgb(" + patternByteArray[i + 2] + ',' + patternByteArray[i + 1] + ',' + patternByteArray[i + 0] + ')';
        notgl.fillRect(i / 3, 0, 1, 1);
    }
  }
};
callback();
