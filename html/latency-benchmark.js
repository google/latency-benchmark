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

var delayedTests = [];

var info = function(test, text) {
  test.infoCell.textContent = text;
  runNextTest(test);
};

var pass = function(test, text) {
  test.resultCell.className += ' passed';
  test.resultCell.textContent = '✓';
  test.infoCell.textContent = text || '';
  runNextTest(test);
};
var fail = function(test, text) {
  test.resultCell.className += ' failed';
  test.resultCell.textContent = '✗';
  test.infoCell.textContent = text || '';
  runNextTest(test);
};
var error = function(test, text) {
  test.resultCell.className += ' testError';
  test.resultCell.textContent = '✗';
  test.infoCell.textContent = text || 'test error';
  runNextTest(test);
};

var checkName = function() {
  if (!this.toCheck)
    return error(this);
  if (this.hasOwnProperty('on') && !this.on)
    return fail(this);
  var on = this.on || window;
  var toCheck = this.toCheck;
  if (getPrefixed(toCheck, on) !== undefined)
    return pass(this);
  else
    return fail(this);
};

var checkWebP = function() {
  var image = new Image();
  var that = this;
  image.onload = image.onerror = function() {
    if (image.height == 1) {
      pass(that);
    } else {
      fail(that);
    }
  }
  image.src = 'data:image/webp;base64,UklGRjAAAABXRUJQVlA4ICQAAACyAgCdASoBAAEAL/3+/3+pqampqYH8SygABc6zbAAA/lAAAAA=';
};

var checkGLExtension = function() {
  if (!gl)
    return fail(this);
  if (!this.toCheck)
    return error(this);
  var prefixes = ['', 'WEBKIT_', 'MOZ_', 'O_', 'MS_'];
  for (var i = 0; i < prefixes.length; i++) {
    if (gl.getExtension(prefixes[i] + this.toCheck)) {
      return pass(this);
    }
  }
  return fail(this);
};

var worker = null;
var workerHandlers = {};
if (window.Worker) {
  worker = new Worker('worker.js');
  worker.onmessage = function(e) {
    var test = e.data.test;
    if (!workerHandlers[test])
      console.error('unhandled worker message type: ' + test);
    else
      workerHandlers[test](e);
  }
}
var spinDone = true;
workerHandlers.spin = function(e) {
  spinDone = true;
};

// We don't use getPrefixed here because we want to replace the unprefixed version with the prefixed one if it exists.
if (worker) {
  worker.postMessage = worker.webkitPostMessage || worker.mozPostMessage || worker.oPostMessage ||worker.msPostMessage || worker.postMessage;
}

var checkTransferables = function() {
  if (!worker || !window.ArrayBuffer)
    return fail(this);
  var buffer = new Float32Array([3, 4, 5]).buffer;
  var test = this;
  worker.onmessage = function(e) {
    var newBuffer = e.data.buffer;
    if (buffer != newBuffer && buffer.byteLength == 0 && newBuffer.byteLength == 12)
      return pass(test);
    return fail(test);
  };
  try {
    worker.postMessage({test: 'transferables', buffer: buffer}, [buffer]);
  } catch(e) {
    fail(this);
  }
};

var checkWorkerGC = function() {
  if (!worker) {
    return fail(this);
  }
}

var checkWorkerString = function() {
  var url = getPrefixed('URL', window);
  if (!url || !window.Worker || !window.ArrayBuffer || !window.ArrayBuffer || !window.Blob)
    return fail(this);
  var workerSource = '';
  var worker = null;
  try {
    var blob = new Blob([workerSource], { type: 'text/javascript' });
    var sourceUrl = url.createObjectURL(blob);
    worker = new Worker(sourceUrl);
  } catch(e) {
    return fail(this);
  }
  return pass(this);
}

var round = function(num, sigFigs) {
  if (num == 0)
    return (0).toFixed(sigFigs - 1);
  var digits = Math.floor(Math.log(num) / Math.LN10) + 1;
  var factor = Math.pow(10, digits - sigFigs);
  var digitsAfterDecimal = Math.max(0, -digits + sigFigs);
  return (Math.round(num / factor) * factor).toFixed(digitsAfterDecimal);
};

var requestServerTest = function(test, start, finish) {
  var request = new XMLHttpRequest();
  request.open('GET', 'http://localhost:5578/test?magicPattern=' + magicPatternHex, true);
  request.onreadystatechange = function() {
    if (request.readyState == 4) {
      if (request.status == 200) {
        finish(JSON.parse(request.response));
      } else if (request.status == 500) {
        error(test, request.response);
      } else {
        fail(test, 'couldn\'t contact server');
      }
    }
  };
  // Wait for the test pattern with its testMode value to be drawn to the screen.
  setTimeout(function() {
    request.send();
    setTimeout(start, 100);
  }, 200);
};

var inputLatency = function() {
  var test = this;
  testMode = TEST_MODES.JAVASCRIPT_LATENCY;
  requestServerTest(test, function() {}, function(response) {
    pass(test, (response.keyDownLatencyMs/(1000/60)).toFixed(1) + ' frames');
  });
};

var scrollLatency = function() {
  var test = this;
  testMode = TEST_MODES.SCROLL_LATENCY;
  requestServerTest(test, function() {}, function(response) {
    pass(test, (response.scrollLatencyMs/(1000/60)).toFixed(1) + ' frames');
  });
};

var testJank = function() {
  var test = this;
  var values = [];
  testMode = TEST_MODES.PAUSE_TIME;
  requestServerTest(test, function() {
    test.iteration = 0;
    test.finishedMeasuring = false;
    test.initialized = false;
    test.blocker();
    test.initialized = true;
    var callback = function() {
      if (test.finished)
        return;
      raf(callback);
      test.iteration++;
      test.blocker();
      if (test.finishedMeasuring) {
        testMode = TEST_MODES.PAUSE_TIME_TEST_FINISHED; // End the test.
      }
    };
    raf(callback);
  }, function(response) {
    pass(test, (response.maxJSPauseTimeMs/(1000/60)).toFixed(1) + ' frames JS pause time, ' + (response.maxCssPauseTimeMs/(1000/60)).toFixed(1) + ' frames CSS pause time ' + (response.maxScrollPauseTimeMs/(1000/60)).toFixed(1) + ' frames scroll pause time');
  });
};


var giantImageContainer = document.createElement('div');
var giantImages = [];
for (var i = 0; i < 100; i++) {
  var giantImage = document.createElement('img');
  giantImageContainer.appendChild(giantImage);
  giantImage.style.width = '1px'
  giantImage.style.height = '1px';
  giantImage.done = false;
  giantImage.failed = false;
  giantImage.onload = function() {
    this.done = true;
  }.bind(giantImage);
  giantImage.onerror = function() {
    this.done = true;
    this.failed = true;
  }.bind(giantImage);
  giantImages.push(giantImage);
}
giantImageContainer.style.position = 'fixed';
giantImageContainer.style.top = '5px';
giantImageContainer.style.left = '5px';
giantImageContainer.style.zIndex = 1;
document.body.appendChild(giantImageContainer);
// TODO: Detect unreported failed image loads (Mac Chrome) with screenshotting.
var loadGiantImage = function() {
  var test = this;
  if (test.iteration == 10) {
    for (var i = 0; i < giantImages.length; i++) {
      // Use a random number for each request to defeat caching. Change hosts every 4 images to defeat HTTP request throttling.
      giantImages[i].src = 'http://127.0.0.' + Math.floor(i / 2 + 1) + ':5578/1024.png?randomNumber=' + Math.random();
    }
  }
  var done = true;
  for (var i = 0; i < giantImages.length; i++) {
    if (!giantImages[i].done) {
      done = false;
      break;
    }
  }
  if (done) {
    test.finishedMeasuring = true;
    for (var i = 0; i < giantImages.length; i++) {
      if (giantImages[i].failed) {
        error(test, 'Image failed to load.');
        return;
      }
    }
  }
};

var control = function() {
  var test = this;
  if (test.iteration == 180) {
    test.finishedMeasuring = true;
  }
};

var Tree = function(levels) {
  this.left = levels > 0 ? new Tree(levels - 1) : null;
  this.right = levels > 0 ? new Tree(levels - 1) : null;
};

var giantTree = null;
var smallTree = null;
var gcLoad = function() {
  var test = this;
  if (!test.initialized) {
    // Allocate tons of long-lived memory to make subsequent GCs slow.
    giantTree = new Tree(22);
    if (worker) {
      spinDone = false;
      worker.postMessage({test: 'spin', lengthMs: 20000});
    }
  } else {
    var start = getMs();
    smallTree = new Tree(14);
    test.value = getMs() - start;
  }
  var done = false;
  if (worker)
    done = spinDone;
  else
    done = test.iteration > 240;
  if (done) {
    giantTree = null;
    smallTree = null;
    // TODO: clenaup by causing GC to happen one last time
    test.finishedMeasuring = true;
  }
};

var cpuLoad = function(msWork, msTotal, workerLoad) {
  return function() {
    var test = this;
    test.units = '%';
    if (workerLoad && worker) {
      if (!test.initialized) {
        spinDone = false;
        worker.postMessage({test: 'spin', lengthMs: 10000});
      }
      test.finishedMeasuring = spinDone;
    } else {
      test.finishedMeasuring = test.iteration > 600;
    }
    var start = getMs();
    var iterations = 0;
    while (getMs() - msWork < start) {
      iterations++
      for (var i = 0; i < 10000; i++);
    }

    test.value = iterations;
    while (getMs() - msTotal < start);
  }
}

var element = document.documentElement;
var style = element.style;
var table = document.createElement('table');
table.id = 'tests';
document.body.appendChild(table);
var tests = [
  { name: 'Keydown latency', test: inputLatency },
  { name: 'Scroll latency', test: scrollLatency },
  { name: 'Control', test: testJank, blocker: control },
  { name: 'Image loading', test: testJank, blocker: loadGiantImage },

  // These tests work, but are disabled for now to focus on the latency test.
  // { name: 'requestAnimationFrame', test: checkName, toCheck: 'requestAnimationFrame' },
  // { name: 'Canvas 2D', test: checkName, toCheck: 'HTMLCanvasElement' },
  // { name: 'WebGL', test: checkName, toCheck: 'createShader', on: gl },
  // { name: 'WebGL compressed textures', test: checkGLExtension, toCheck: 'WEBGL_compressed_texture_s3tc'},
  // { name: 'WebGL floating-point textures', test: checkGLExtension, toCheck: 'OES_texture_float' },
  // { name: 'WebGL depth textures', test: checkGLExtension, toCheck: 'WEBGL_depth_texture' },
  // { name: 'WebGL anisotropic filtering', test: checkGLExtension, toCheck: 'EXT_texture_filter_anisotropic' },
  // { name: 'WebGL standard derivatives', test: checkGLExtension, toCheck: 'OES_standard_derivatives' },
  // { name: 'WebP images', test: checkWebP },
  // { name: 'Typed Arrays', test: checkName, toCheck: 'ArrayBuffer' },
  // { name: 'Blob', test: checkName, toCheck: 'Blob' },
  // { name: 'DataView', test: checkName, toCheck: 'DataView' },
  // { name: 'Web Workers', test: checkName, toCheck: 'Worker' },
  // { name: 'Can create a worker from a string', test: checkWorkerString },
  // { name: 'Transferable typed arrays', test: checkTransferables },
  // { name: 'CSS transforms', test: checkName, toCheck: 'transform', on: style },
  // { name: 'CSS animations', test: checkName, toCheck: 'animation', on: style },
  // { name: 'CSS transitions', test: checkName, toCheck: 'transition', on: style },
  // { name: 'CSS 3D', test: checkName, toCheck: 'perspective', on: style },
  // { name: 'fullscreen', test: checkName, toCheck: 'requestFullScreen', on: element },
  // { name: 'mouse lock', test: checkName, toCheck: 'requestPointerLock', on: element },
  // { name: 'high-precision timers', test: checkName, toCheck: 'now', on: window.performance },
  // { name: 'Web Audio', test: checkName, toCheck: 'AudioContext'},
  // { name: 'Web Sockets', test: checkName, toCheck: 'WebSocket' },
  // { name: 'WebRTC camera/microphone', test: checkName, toCheck: 'getUserMedia', on: navigator },

  // These tests don't work yet.
  // { name: 'WebRTC peer connection', test: checkName, toCheck: 'PeerConnection' },
  // { name: 'WebRTC peer connection binary data' },
  // { name: 'WebRTC peer connection UDP' },
  // { name: 'gamepad', test: checkName, toCheck: 'Gamepads', on: navigator },
  // { name: 'input event timestamps' },
  // { name: 'input latency with CPU load' },
  // { name: 'input latency with GPU load' },
  // { name: 'JavaScript doesn\'t block CSS animation' },
  // { name: 'JavaScript doesn\'t block scrolling' },
  // { name: 'Canvas 2D doesn\'t block JavaScript' },
  // { name: 'Touch events' },
  // { name: 'Device orientation' }
  // { name: 'GC variability', test: testJank, blocker: gcLoad },
  // { name: 'Work per frame, low load', test: testJank, blocker: cpuLoad(8, 8) },
  // { name: 'Work per frame, background load', test: testJank, blocker: cpuLoad(8, 8, true) },
  // { name: 'Work per frame, high load', test: testJank, blocker: cpuLoad(8, 14) },
  // { name: 'Worker GC doesn\'t affect main page', test: testJank, blocker: workerGCLoad },
  ];

for (var i = 0; i < tests.length; i++) {
  var test = tests[i];
  var row = document.createElement('tr');
  var nameCell = document.createElement('td');
  var resultCell = document.createElement('td');
  var infoCell = document.createElement('td');
  nameCell.className = 'testName';
  nameCell.textContent = test.name;
  resultCell.className = 'testResult';
  test.resultCell = document.createElement('div');
  resultCell.appendChild(test.resultCell);
  infoCell.className = 'testInfo';
  test.infoCell = infoCell;
  row.appendChild(nameCell);
  row.appendChild(resultCell);
  row.appendChild(infoCell);
  table.appendChild(row);
}

var nextTestIndex = 0;

var runNextTest = function(previousTest) {
  scrollTo(0, 0);
  if (previousTest && !previousTest.timedOut) {
    if (previousTest.finished) {
      previousTest.infoCell.textContent = "Error: test sent multiple results.";
      return;
    }
    previousTest.finished = true;
    if (previousTest != tests[nextTestIndex - 1]) {
      previousTest.infoCell.textContent = "Error: test sent results out of order";
      return;
    }
  }
  var testIndex = nextTestIndex++;
  if (testIndex >= tests.length) {
    return;
  }
  var test = tests[testIndex];
  test.infoCell.textContent = 'Testing... Do not move or deactivate the browser window.';
  setTimeout(function() { checkTimeout(test); }, 50000);
  if (test.test) {
    setTimeout(function() {
      try {
        test.test();
      } catch(e) {
        error(test, 'unhandled exception');
      }
    }, 1);
  } else {
    info(test, 'test not implemented');
  }
};
setTimeout(runNextTest, 100);

var checkTimeout = function(test) {
  if (!test.finished) {
    test.timedOut = true;
    test.finished = true;
    return error(test, "Timed out");
  }
}
