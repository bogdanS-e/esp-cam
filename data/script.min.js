let ws = null;
const currentUrl = window.location.hostname;
const statusElement = document.getElementById('status');
const flashButton = document.getElementById("toggleFlash");
const frameSizeSelect = document.getElementById("frameSize");
const streamElement = document.getElementById('stream');

// UI functions
function handleRotationScreen() {
  const checkOrientation = () => {
    const message = document.getElementById('rotate-message');

    if (window.matchMedia("(orientation: portrait)").matches) {
      message.style.display = "block";

      return;
    }

    message.style.display = "none";
  }

  checkOrientation();
  window.addEventListener("resize", checkOrientation);
}

let lastStatus = null;
function showStatus(isConnected) {
  if (lastStatus === isConnected) {
    return;
  }

  lastStatus = isConnected;

  clearTimeout(statusElement._hideTimer);
  statusElement.classList.add("visible");

  if (isConnected) {
    statusElement.textContent = "🟢 Connected ✅";
    statusElement.classList.remove("disconnected");

    statusElement._hideTimer = setTimeout(() => {
      statusElement.classList.remove("visible");
    }, 3000);

    return;
  }

  statusElement.textContent = "🔴 Disconnected ❌";
  statusElement.classList.add("disconnected");
}

function updateWiFiIndicator(rssi) {
  const indicator = document.getElementById('wifiIndicator');
  const rssiValue = document.getElementById('rssiValue');

  if (!rssi) {
    indicator.style.display = 'none';

    return;
  }

  indicator.style.display = '';

  if (rssi <= 55) {
    indicator.className = 'excellent';
  } else if (rssi <= 75) {
    indicator.className = 'good';
  } else if (rssi <= 85) {
    indicator.className = 'weak';
  } else {
    indicator.className = 'weak poor';
  }

  rssiValue.textContent = `-${rssi}dBm`;
}

function changeControls(disable) {
  //document.querySelectorAll('.controller').forEach(btn => btn.disabled = disable);
}

async function checkCarConnection() {
  if (location.hostname === "car.local") {
    setInterval(async () => {
      try {
        const res = await fetch(window.location.href, { cache: "no-store" });

        if (res.ok) {
          location.reload();
        }
      } catch (e) {
        console.log(e);
      }
    }, 1000);
  }
}


// websocket initialization and handlers
function handleWebSocket() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    return;
  }

  ws = new WebSocket(`ws://${currentUrl}:82/ws`);

  let heartbeatInterval;
  let missedPongs = 0;
  const maxMissedPongs = 2;

  ws.onopen = () => {
    showStatus(true);
    changeControls(false);

    streamElement.src = `#`; //reinit stream src to force reload if connection were lost
    setTimeout(() => {
      streamElement.src = `http://${currentUrl}:81/stream`;
    }, 1000);

    heartbeatInterval = setInterval(() => {
      ws.send('ping');
      missedPongs++;

      if (missedPongs >= maxMissedPongs) {
        console.log('Too many missed pongs, reconnecting...');
        ws.close();
        ws.onclose();
      }
    }, 2000);
  };

  ws.onmessage = (event) => {
    if (event.data.startsWith("pong-")) {
      showStatus(true);

      missedPongs = 0;

      const rssi = parseInt(event.data.split("-")[1]);

      updateWiFiIndicator(rssi);

      return;
    }

    if (event.data.startsWith("Flash-")) {
      const state = event.data.split("-")[1];

      if (state === "ON") {
        flashButton.classList.remove("turned-off");
      } else if (state === "OFF") {
        flashButton.classList.add("turned-off");
      }
    }

    if (event.data.startsWith("FRAMESIZE-")) {
      const frameSize = event.data.split("-")[1];

      frameSizeSelect.value = frameSize;
    }

    if (event.data.startsWith("WIFI-")) {
      const toggleWifiModeButton = document.getElementById("toggleWifiMode");
      const acModeScreen = document.getElementById("ac-mode");

      const isStationMode = event.data.split("-")[1] === '1';
      const text = isStationMode ? "AP" : "ST";

      toggleWifiModeButton.removeAttribute("disabled");
      toggleWifiModeButton.textContent = text;

      toggleWifiModeButton.onclick = () => {
        if (isStationMode) {
          ws.sendData("reset");
          acModeScreen.classList.add("visible");
          checkCarConnection();

          return;
        }

        window.location.href = `${window.location.protocol}//${window.location.hostname}/wifi?`;
      };
    }
  }

  ws.onclose = () => {
    console.log('WebSocket disconnected');
    showStatus(false);
    changeControls(true);
    clearInterval(heartbeatInterval);
    setTimeout(handleWebSocket, 2000);
  };

  ws.onerror = (error) => {
    console.log('WebSocket error:', error);
    clearInterval(heartbeatInterval);
  };

  ws.sendData = (data) => {
    console.log(data);

    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(data);
    }
  }
}

// car control functions
function handleCarMovement() {
  const DATA_SEND_INTERVAL = 100;
  const controllers = document.querySelectorAll('.movement-controller');
  const intervals = {};
  const activeKeys = new Set();
  const keyMap = { w: "forward", s: "backward", a: "left", d: "right" };

  const moveCar = () => {
    const directions = Array.from(activeKeys);
    let output = "";

    if (directions.length === 1) {
      output = directions[0];
    } else if (directions.length === 2) {
      const x = directions.find(direction => direction === "forward" || direction === "backward");
      const y = directions.find(direction => direction === "left" || direction === "right");

      if (x && y) {
        output = `${x}-${y}`;
      }
    }

    if (output) {
      ws.sendData(output);

      return
    }

    ws.sendData("stop");
  }

  const startAction = (elementId) => {
    if (intervals[elementId]) {
      return;
    }

    activeKeys.add(elementId);

    const activeElement = document.getElementById(elementId);

    if (activeElement) {
      activeElement.classList.add("active");
    }

    moveCar();

    intervals[elementId] = setTimeout(function repeat() {
      moveCar();
      intervals[elementId] = setTimeout(repeat, DATA_SEND_INTERVAL);
    }, DATA_SEND_INTERVAL);
  }

  const stopAction = (elementId) => {
    if (!intervals[elementId]) {
      return;
    }

    activeKeys.delete(elementId);

    const activeElement = document.getElementById(elementId);

    if (activeElement) {
      activeElement.classList.remove("active");
    }

    clearTimeout(intervals[elementId]);
    delete intervals[elementId];

    moveCar();
  }

  const attachHandlers = () => {
    // mouse
    controllers.forEach(elementId => {
      elementId.addEventListener("mousedown", () => startAction(elementId.id));
      elementId.addEventListener("mouseup", () => stopAction(elementId.id));
      elementId.addEventListener("mouseleave", () => stopAction(elementId.id));
    });

    // keyboard
    document.addEventListener("keydown", e => {
      const elementId = keyMap[e.key.toLowerCase()];
      if (elementId) startAction(elementId);
    });
    document.addEventListener("keyup", e => {
      const elementId = keyMap[e.key.toLowerCase()];
      if (elementId) stopAction(elementId);
    });

    // multitouch
    const joystickWrapper = document.querySelector(".joystick-wrapper");
    joystickWrapper.addEventListener("touchstart", e => {
      e.preventDefault();

      for (const { target } of e.changedTouches) {
        const controller = target.closest(".movement-controller");

        if (controller) {
          startAction(controller.id);
        }
      }
    });
    joystickWrapper.addEventListener("touchend", e => {
      e.preventDefault();

      for (const { target } of e.changedTouches) {
        const controller = target.closest(".movement-controller");

        if (controller) {
          stopAction(controller.id);
        }
      }
    });
    joystickWrapper.addEventListener("touchcancel", e => {
      e.preventDefault();

      for (const { target } of e.changedTouches) {
        const controller = target.closest(".movement-controller");

        if (controller) {
          stopAction(controller.id);
        }
      }
    });
  }

  attachHandlers();
}

function handleFunctions() {
  const takePhotoButton = document.getElementById("takePhotoButton");

  const capturePhoto = async () => {
    takePhotoButton.disabled = true;

    try {
      const response = await fetch('/capture_photo');
      const blob = await response.blob();
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');

      a.href = url;
      a.download = `esp32_${Date.now()}.jpg`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      window.URL.revokeObjectURL(url);
    } catch (error) {
      console.error('Photo capture error:', error);
    }

    takePhotoButton.disabled = false;
  }

  const attachHandlers = () => {
    flashButton.addEventListener("click", () => ws.sendData("toggleFlash"));

    frameSizeSelect.addEventListener("change", () => {
      const selectedValue = frameSizeSelect.value;

      ws.sendData(`frameSize_${selectedValue}`);
    });

    takePhotoButton.addEventListener("click", capturePhoto);
  }
  attachHandlers();
}

function handleCameraDrag() {
  const drag = { x: 0, y: 0 };
  const dragArea = document.getElementById('stream');
  const resetButton = document.getElementById('resetCamera');
  const rangeX = document.getElementById('rangeX');
  const rangeY = document.getElementById('rangeY');
  const thumbX = document.getElementById('thumbX');
  const thumbY = document.getElementById('thumbY');
  const DRAG_SENSITIVITY = 0.8;
  const RANGE_OPACITY_TIMEOUT = 2000;
  const RANGE_OPACITY = 0.7;

  let timeout = null;
  let isDragging = false;
  let startX = 0;
  let startY = 0;

  const onDragChange = (x, y) => {
    drag.x = x;
    drag.y = y;

    const xPercent = (drag.x + 100) / 200;
    const yPercent = (drag.y + 100) / 200;

    thumbX.style.left = `${xPercent * 200}px`;
    thumbY.style.top = `${(1 - yPercent) * 200}px`;
    rangeX.style.opacity = RANGE_OPACITY;
    rangeY.style.opacity = RANGE_OPACITY;

    ws.sendData(`cameraDrag_${drag.x}_${drag.y}`);

    clearTimeout(timeout);
    timeout = setTimeout(() => {
      rangeX.style.opacity = '';
      rangeY.style.opacity = '';
    }, RANGE_OPACITY_TIMEOUT);
  }

  const handleCameraMove = (x, y) => {
    const dx = x - startX;
    const dy = y - startY;

    startX = x;
    startY = y;

    const newX = Math.round(Math.max(-100, Math.min(100, drag.x + dx * DRAG_SENSITIVITY)));
    const newY = Math.round(Math.max(-100, Math.min(100, drag.y - dy * DRAG_SENSITIVITY)));

    onDragChange(newX, newY);
  }

  const attachHandlers = () => {
    resetButton.addEventListener('click', () => onDragChange(0, 0));

    dragArea.addEventListener('mousedown', (e) => {
      e.preventDefault();

      isDragging = true;
      startX = e.clientX;
      startY = e.clientY;
      dragArea.style.cursor = 'grabbing';
    });

    window.addEventListener('mouseup', () => {
      isDragging = false;
      dragArea.style.cursor = 'grab';
    });

    window.addEventListener('mousemove', (e) => {
      if (!isDragging) return;

      handleCameraMove(e.clientX, e.clientY);
    });

    dragArea.addEventListener(
      'touchstart',
      (e) => {
        if (e.touches.length === 2) {
          e.preventDefault();

          isDragging = true;
          startX = e.changedTouches[0].clientX;
          startY = e.changedTouches[0].clientY;

          return;
        }

        isDragging = false;
      },
      { passive: false }
    );

    dragArea.addEventListener(
      'touchmove',
      (e) => {
        if (isDragging && e.touches.length === 2) {
          e.preventDefault();
          handleCameraMove(e.changedTouches[1].clientX, e.changedTouches[1].clientY);
        }
      },
      { passive: false }
    );

    dragArea.addEventListener('touchend', () => {
      isDragging = false;
    });
  }

  attachHandlers();
}


// init all function on page load
document.addEventListener("DOMContentLoaded", function () {
  changeControls(true);
  handleRotationScreen();
  handleWebSocket();
  handleCarMovement();
  handleFunctions();
  handleCameraDrag();
});