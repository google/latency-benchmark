var goodJank = document.getElementById('goodJank');
var badJank = document.getElementById('badJank');
var goodLatency = document.getElementById('goodLatency');
var badLatency = document.getElementById('badLatency');

var nextJank = 0;
function setupDragging(div, latencyMs, jankMs) {
  div.style.overflow = 'hidden';
  div.style.cursor = 'move';
  div.firstElementChild.style.position = 'relative';
  var mouseX = 0;
  var mouseY = 0;
  var dragX = 0;
  var dragY = 0;
  div.onmousedown = function(e) {
    if (e.touches && e.touches[0]) {
      e.clientX = e.touches[0].pageX;
      e.clientY = e.touches[0].pageY;
    }
    mouseX = e.clientX;
    mouseY = e.clientY;
    window.onmousemove = function(e) {
      if (e.touches && e.touches[0]) {
        e.clientX = e.touches[0].pageX;
        e.clientY = e.touches[0].pageY;
      }
      var now = Date.now();
      if (now > nextJank) {
        if (now - nextJank < 1000) {
          var jankEnd = Date.now() + jankMs;
          while (Date.now() < jankEnd);
        }
        nextJank = Date.now() + 400;
      }
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
      if (latencyMs) {
        window.setTimeout(updateDrag, latencyMs);
      } else {
        updateDrag();
      }
      e.stopPropagation()
      return false;
    }
    window.ontouchmove = window.onmousemove;
    window.onmouseup = function(e) {
      window.onmousemove = null;
      window.ontouchmove = null;
      e.stopPropagation()
      return false;
    }
    window.ontouchend = window.onmouseup;
    e.stopPropagation();
    return false;
  }
  div.ontouchstart = div.onmousedown;
}

setupDragging(goodLatency, 0, 0);
setupDragging(badLatency, 10/60 * 1000, 0);

setupDragging(goodJank, 0, 0);
setupDragging(badJank, 0, 10/60 * 1000);
