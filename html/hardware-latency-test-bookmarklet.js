(function() {
if (window.latencyTestEmbedded) return;
window.latencyTestEmbedded = true;

var button = document.createElement('button');
button.style.cssText = 'position:fixed !important; z-index: 9999999 !important; color: black !important; background: white !important; border: 10px solid gray !important; padding: 10px !important; top: 100px !important; left: 100px; !important';
button.onclick = buttonPress;
button.textContent = 'Click here to focus page and show test area.';
document.body.appendChild(button);

function buttonPress() {
  document.body.removeChild(button);
  var testCanvas = document.createElement('canvas');
  testCanvas.style.cssText = 'position:fixed !important; z-index: 9999999 !important; border: 10px solid gray !important; top: 100px !important; left: 100px; !important';
  testCanvas.width = 300;
  testCanvas.height = 300;

  var gl = testCanvas.getContext('webgl') || testCanvas.getContext('experimental-webgl');
  if (!gl) {
    alert('Failed to initialize WebGL.');
    return;
  }
  document.body.appendChild(testCanvas);
  var color = [0, 0, 0];
  window.addEventListener('keydown', function(e) {
    if (e.keyCode == 66) {
      // 'B' for black.
      color = [0, 0, 0];
    }
    if (e.keyCode == 87) {
      // 'W' for white.
      color = [1, 1, 1];
    }
    if (e.keyCode == 84) {
      // 'T' for test.
      startTest();
    }
    e.preventDefault();
    e.stopPropagation();
    return false;
  }, true);

  var testRunning = false;

  function draw() {
    if (testRunning) {
      requestAnimationFrame(draw);
    }
    gl.clearColor(color[0], color[1], color[2], 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
  }
  draw();

  var results = [];
  function startTest() {
    if (testRunning) return;
    testRunning = true;
    requestAnimationFrame(draw);
    var request = new XMLHttpRequest();
    request.open('GET', 'http://localhost:5578/oculusLatencyTester?defeatCache=' + Math.random(), true);
    request.onreadystatechange = function() {
      if (request.readyState == 4) {
        if (request.status == 200) {
          console.log(request.response);
        }
        testRunning = false;
      }
    };
    request.send();
  }
}
})();
