var goodScroll = document.getElementById('goodScroll');
var badScroll = document.getElementById('badScroll');
var goodDrag = document.getElementById('goodDrag');
var badDrag = document.getElementById('badDrag');

badScroll.onscroll = function() {
  console.log('scrolled');
}

function setupScrolling(div) {
  var content = '';
  for (var i = 0; i < 20; i++) {
    content += div.innerHTML;
  }
  div.innerHTML = content;
}

setupScrolling(goodScroll);
setupScrolling(badScroll);

var lastScroll = 0;
var nextJank = 0;
badScroll.onscroll = function(e) {
  var now = Date.now();
  if (now > nextJank) {
    if (now - nextJank < 3000) {
      var jankEnd = Date.now() + 333;
      while (Date.now() < jankEnd);
    }
    nextJank = Date.now() + 500;
  }
}

function setupDragging(div, latencyMs) {
  div.style.overflow = 'hidden';
  div.style.cursor = 'move';
  div.firstElementChild.style.position = 'relative';
  var mouseX = 0;
  var mouseY = 0;
  var dragX = 0;
  var dragY = 0;
  div.onmousedown = function(e) {
    mouseX = e.clientX;
    mouseY = e.clientY;
    window.onmousemove = function(e) {
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
    window.onmouseup = function(e) {
      window.onmousemove = null;
      e.stopPropagation()
      return false;
    }
    e.stopPropagation();
    return false;
  }
}

setupDragging(goodDrag, 0);
setupDragging(badDrag, 150);
