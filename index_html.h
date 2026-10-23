#pragma once
#include <pgmspace.h>

const char HTML_PAGE[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Rover Dashboard</title>
  <style>
    body { font-family: Arial, sans-serif; background: #0f172a; color: #f8fafc; text-align: center; margin: 0; padding: 15px; }
    h2 { color: #38bdf8; margin-bottom: 5px; }
    .stream-box { max-width: 500px; min-height: 60px; margin: 10px auto; background: #000; border-radius: 10px; overflow: hidden; border: 2px solid #334155; color: #94a3b8; padding: 5px; }
    .stream-box img { width: 100%; height: auto; display: block; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(110px, 1fr)); gap: 10px; max-width: 550px; margin: 15px auto; }
    .card { background: #1e293b; padding: 10px; border-radius: 10px; border: 1px solid #334155; }
    .val { font-size: 1.2em; font-weight: bold; color: #4ade80; margin-top: 5px; }
    .btn-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; max-width: 300px; margin: 15px auto; }
    button { background: #3b82f6; color: white; border: none; padding: 15px; font-size: 1.1em; border-radius: 8px; font-weight: bold; cursor: pointer; touch-action: manipulation; }
    button:active { background: #1d4ed8; }
    .stop-btn { background: #ef4444; }
    .toggle-btn { background: #8b5cf6; width: 100%; max-width: 300px; padding: 12px; margin-top: 10px; }
    .mode-box { margin: 10px 0; font-weight: bold; color: #facc15; }
    .alarm { color: #ef4444 !important; animation: blink 1s infinite; }
    @keyframes blink { 50% { opacity: 0.5; } }
  </style>
</head>
<body>
  <h2>ESP32 ROVER DASHBOARD</h2>
  <div class="mode-box">MODE: <span id="mode-text">MANUAL</span></div>

  <div class="stream-box">
    <div id="cam-status">Waiting for camera...</div>
    <img id="cam" style="display:none;" alt="Camera stream" />
  </div>

  <div class="grid">
    <div class="card">Sonar<div class="val" id="dist">-- cm</div></div>
    <div class="card">IR Sensor<div class="val" id="ir">--</div></div>
    <div class="card">MQ-8 Gas<div class="val" id="gas">-- %</div></div>
    <div class="card">Temp<div class="val" id="temp">-- &deg;C</div></div>
    <div class="card">Humidity<div class="val" id="hum">-- %</div></div>
  </div>

  <button class="toggle-btn" onclick="fetch('/toggle-mode')">Switch Mode (Auto/Manual)</button>

  <h3 style="margin-top:20px;">Manual Driving Controls</h3>
  <div class="btn-grid">
    <div></div>
    <button onclick="sendCmd('forward')">&#9650;<br>FWD</button>
    <div></div>
    <button onclick="sendCmd('left')">&#9664;<br>LEFT</button>
    <button class="stop-btn" onclick="sendCmd('stop')">STOP</button>
    <button onclick="sendCmd('right')">&#9654;<br>RIGHT</button>
    <div></div>
    <button onclick="sendCmd('backward')">&#9660;<br>REV</button>
    <div></div>
  </div>

  <script>
    var currentCam = '';
    var camImg = document.getElementById('cam');
    var camStatus = document.getElementById('cam-status');

    camImg.onload = function() {
      camImg.style.display = 'block';
      camStatus.style.display = 'none';
    };

    camImg.onerror = function() {
      camImg.style.display = 'none';
      camStatus.style.display = 'block';
      camStatus.innerText = 'Camera stream lost, retrying...';
      var ip = currentCam;
      if (ip) {
        setTimeout(function() {
          if (ip === currentCam) {
            camImg.src = 'http://' + ip + ':81/stream?t=' + Date.now();
          }
        }, 2000);
      }
    };

    function sendCmd(cmd) { fetch('/control?dir=' + cmd); }

    function updateData() {
      fetch('/api/data')
        .then(function(res) { return res.json(); })
        .then(function(data) {
          document.getElementById('dist').innerText = data.dist + ' cm';

          var irElem = document.getElementById('ir');
          irElem.innerText = data.ir ? 'DETECTED' : 'CLEAR';
          irElem.style.color = data.ir ? '#ef4444' : '#4ade80';

          var gasElem = document.getElementById('gas');
          gasElem.innerText = data.gas + ' %';
          if (data.gas > 60) {
            gasElem.classList.add('alarm');
          } else {
            gasElem.classList.remove('alarm');
          }

          document.getElementById('temp').innerText = data.temp + ' \u00B0C';
          document.getElementById('hum').innerText = data.hum + ' %';
          document.getElementById('mode-text').innerText = data.auto ? 'AUTONOMOUS' : 'MANUAL';

          if (data.cam && data.cam !== currentCam) {
            currentCam = data.cam;
            camStatus.innerText = 'Connecting to camera at ' + data.cam + '...';
            camImg.src = 'http://' + data.cam + ':81/stream';
          }
        })
        .catch(function(e) {});
    }
    setInterval(updateData, 400);
  </script>
</body>
</html>
)HTMLPAGE";
