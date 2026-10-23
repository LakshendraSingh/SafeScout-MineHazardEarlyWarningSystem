# SafeScout a Mine Hazard Warning System

![Poster](https://github.com/LakshendraSingh/SafeScout-MineHazardEarlyWarningSystem/blob/main/Poster.png)

A Wi-Fi controlled rover built on two boards:

- **ESP32 (main controller)** – drives the motors, reads the sensors, hosts its own Wi-Fi hotspot and serves a web dashboard.
- **ESP32-CAM (AI-Thinker)** – joins the rover's hotspot and streams live MJPEG video to the dashboard.

Control it from any phone or laptop browser: drive manually, or switch to autonomous obstacle-avoidance mode.

---

## Features

- Private Wi-Fi hotspot (no router or internet needed)
- Mobile-friendly web dashboard with live camera feed
- Manual driving (forward, backward, left, right, stop)
- Autonomous mode with non-blocking obstacle avoidance (ultrasonic + IR)
- Live sensor readout: distance, IR obstacle, gas level, temperature, humidity
- Gas alarm (blinking red) when the reading goes above 60%
- Camera auto-registers its IP with the rover and auto-retries if the stream drops

---

## Project Files

| File | Board | Description |
|------|-------|-------------|
| `classlab_esp.ino` | ESP32 | Main rover firmware (motors, sensors, web server, avoidance logic) |
| `index_html.h` | ESP32 | Dashboard HTML/CSS/JS stored in flash (`PROGMEM`). Must sit in the same folder as `classlab_esp.ino` |
| `sketch_oct22b.ino` | ESP32-CAM | Camera firmware (joins the rover network, streams video on port 81) |

---

## Hardware

- ESP32 dev board
- ESP32-CAM (AI-Thinker)
- Dual H-bridge motor driver (e.g. L298N) + 2 DC motors
- HC-SR04 ultrasonic sensor
- DHT11 temperature/humidity sensor
- MQ-8 gas sensor (analog output)
- IR obstacle sensor module (digital, active LOW)
- Battery pack and a stable 5V supply for the ESP32-CAM

### Main ESP32 pin map

| Function | GPIO |
|----------|------|
| Motor A (left) IN1 / IN2 | 18 / 19 |
| Motor B (right) IN3 / IN4 | 21 / 22 |
| Ultrasonic TRIG / ECHO | 5 / 23 |
| DHT11 data | 4 |
| MQ-8 analog out | 34 |
| IR sensor digital out | 35 |

> **Tip:** The HC-SR04 ECHO pin outputs 5V. Use a voltage divider (e.g. 1 kΩ + 2 kΩ) to bring it down to 3.3V for the ESP32.

> **Tip:** Make sure all grounds (battery, motor driver, ESP32, sensors) are common.

---

## Software Setup

### 1. Arduino IDE

1. Install the **ESP32 board package** (Boards Manager → `esp32` by Espressif).
2. Install these libraries (Library Manager):
   - `DHT sensor library` (Adafruit)
   - `Adafruit Unified Sensor`
3. `WiFi`, `WebServer`, `HTTPClient`, `esp_camera` and `esp_http_server` come with the ESP32 core.

### 2. Flash the main ESP32

1. Put `classlab_esp.ino` and `index_html.h` in the same sketch folder (named `classlab_esp`).
2. Select your ESP32 board (e.g. *ESP32 Dev Module*) and the correct port.
3. Upload.

### 3. Flash the ESP32-CAM

1. Open `sketch_oct22b.ino`.
2. Board: **AI Thinker ESP32-CAM**. PSRAM: **Enabled**.
3. Connect via an FTDI adapter (or the programmer base), hold **IO0 to GND** while uploading, then remove it and press reset.

---

## Usage

1. Power on the main ESP32. It creates the hotspot:
   - **SSID:** `ESP32_Rover_Network`
   - **Password:** `12345678`
2. Power on the ESP32-CAM. It joins that network and registers itself with the rover every 5 seconds.
3. Connect your phone/laptop to `ESP32_Rover_Network`.
4. Open **http://192.168.4.1** in a browser.
5. The camera feed appears automatically once the camera has registered.

### Dashboard controls

- **Switch Mode** toggles between Manual and Autonomous.
- **FWD / REV / LEFT / RIGHT** drive the rover (Manual mode only).
- **STOP** halts the motors.

> Manual commands latch: the rover keeps moving until you press **STOP** or send another command.

---

## Autonomous Mode

In autonomous mode the rover drives forward and checks the sensors every 60 ms. If the ultrasonic distance is under **25 cm** or the IR sensor detects an obstacle, it runs this sequence (non-blocking, so the web server stays responsive):

| Step | Action | Duration |
|------|--------|----------|
| 1 | Stop | 150 ms |
| 2 | Reverse | 300 ms |
| 3 | Stop | 150 ms |
| 4 | Turn right | 400 ms |
| 5 | Resume driving | – |

Tune the thresholds and timings in `handleObstacleAvoidance()`.

---

## Motor Direction Tuning

If the rover drives the wrong way, adjust these defines at the top of `classlab_esp.ino`:

```cpp
#define INVERT_MOTOR_A false
#define INVERT_MOTOR_B true    // motors are mounted mirrored
#define SWAP_LEFT_RIGHT false
```

- FWD drives backward → set `INVERT_MOTOR_A true` and `INVERT_MOTOR_B false`
- LEFT and RIGHT are swapped → set `SWAP_LEFT_RIGHT true`

---

## HTTP API

All endpoints are served by the main ESP32 at `http://192.168.4.1`.

| Endpoint | Description |
|----------|-------------|
| `GET /` | Dashboard page |
| `GET /api/data` | JSON: `dist`, `ir`, `gas`, `temp`, `hum`, `auto`, `cam` |
| `GET /control?dir=forward\|backward\|left\|right\|stop` | Drive command (ignored in auto mode) |
| `GET /toggle-mode` | Switch between manual and autonomous |
| `GET /cam-register` | Called by the ESP32-CAM to report its IP |

The camera stream itself is served by the ESP32-CAM at `http://<camera-ip>:81/stream`.

Example `/api/data` response:

```json
{"dist":42.3,"ir":false,"gas":12,"temp":28.0,"hum":65.0,"auto":false,"cam":"192.168.4.2"}
```

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Dashboard says "Waiting for camera..." | Check the ESP32-CAM has 5V power and has joined the network. Open the Serial Monitor (115200) to see its IP. |
| "Camera init failed" in Serial | Re-seat the camera ribbon cable and use a stronger 5V supply. |
| Camera keeps rebooting / browns out | Power it from a dedicated 5V source, not a weak USB port. |
| Stream works on the camera IP but not in the dashboard | Your device must be connected to the rover's hotspot, not mobile data. |
| Distance always reads 400 cm | No echo received. Check TRIG/ECHO wiring and the voltage divider. |
| Temp/humidity stuck at 0 | Check DHT11 wiring and pull-up. Values keep their last good reading if a read fails. |
| Motors don't move | Check motor driver power and enable pins (ENA/ENB jumpers or tied HIGH). |

---

## Notes

- The gas value is the raw ADC reading scaled to 0–100%. It is **not** calibrated to ppm. The MQ-8 also needs a warm-up period before readings settle.
- Change `AP_PASS` (and `ROVER_PASS` in the camera sketch) from the default before using the rover anywhere shared. Both must match.
- The camera sketch assumes the rover's IP is `192.168.4.1` (the ESP32 soft-AP default).
- Sensors on the main board are read every 400 ms; the dashboard polls at the same rate.

---

## License

Add your preferred license here (e.g. MIT).
