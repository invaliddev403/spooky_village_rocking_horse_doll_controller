# 🕯️ Spooky Village Rocking Horse Doll Controller

An ESP32-C3–based Wi-Fi–enabled controller for the **spooky rocking horse doll**, capable of simulating button presses manually or at randomized intervals.  
Designed for standalone use (no external Wi-Fi required) — the device creates its own access point and web control panel.  
Includes **deep sleep power saving** for long battery life and a **hardware wake/escape pin** to exit sleep mode.

---

## ⚙️ Features

- 🧠 **Web Control Panel (SoftAP mode)**  
  - Hosted locally by the ESP32-C3.  
  - Accessible at `192.168.4.1` when connected to the AP `ESP32-Button`.  
  - Manual press control, randomized press configuration, and live status updates.  

- ⏱️ **Random Interval Mode (Deep Sleep)**  
  - Select random min/max press intervals in minutes.  
  - ESP32-C3 sleeps between activations, drastically extending battery life.  
  - Automatically wakes for each press.  

- 🔋 **Low Power Deep Sleep**  
  - Power consumption drops to tens of µA between activations.  
  - Perfect for battery operation.

- 🧷 **Hardware Wake / Exit Pin (GPIO9)**  
  - Pull **GPIO9** to GND to exit deep sleep and return to full Wi-Fi web control mode.  

- 🎭 **On-board Image & Web Styling**  
  - The included `doll_img.h` serves a spooky doll photo from program memory.  
  - Optional web customization via HTML/CSS in the sketch.

---

## 🪛 Hardware Connections

### Control Side
| ESP32-C3 Pin | Component | Description |
|---------------|------------|-------------|
| GPIO2 | AQV212 pin 1 (+) via 470 Ω resistor | Controls the relay input |
| GND | AQV212 pin 2 (–) | Relay input ground |
| GPIO9 | Button to GND | Wake/escape input |
| 3V3 / VIN | Power | Depending on your power source |

### Load Side (Simulated Button)
| AQV212 Pins | Function |
|--------------|-----------|
| 4 & 5 | Connected across the toy’s existing push-button leads |

---

## 🔋 Power Notes

- Typical ESP32-C3 active: ~100 mA  
- Deep sleep (timer): ~20–30 µA  
- The included 2000 mAh Li-Po can run **weeks to months** in deep-sleep random mode.  

---

## 🧰 File Overview

| File | Description |
|------|--------------|
| `Spooky_Doll_Controller.ino` | Main Arduino sketch (webserver + logic + deep sleep) |
| `doll_img.h` | JPEG image stored in PROGMEM (served at `/img`) |

---

## 📷 Serving the Doll Image

The doll image is embedded directly from `doll_img.h`.  
To replace it:

1. Convert a JPEG to a C header:
   ```bash
   xxd -i spooky.jpg > doll_img.h
   ```
2. Rename variables inside to:
   ```c
   const uint8_t DOLL_JPG[] PROGMEM = { ... };
   const size_t DOLL_JPG_LEN = sizeof(DOLL_JPG);
   ```

---

## 🌐 Web Interface

After flashing:

1. Connect to Wi-Fi network:  
   **SSID:** `ESP32-Button`  
   **Password:** `press1234`
2. Open a browser and go to **http://192.168.4.1/**
3. Use the control page to:
   - Trigger manual presses.
   - Configure random intervals.
   - Start/stop random deep-sleep mode.
4. To exit sleep mode, **hold GPIO9 low (to GND)** and reset or wait for next wake.

---

## 💡 Deep Sleep Behavior Summary

| Condition | Wi-Fi | Behavior |
|------------|--------|-----------|
| Random Mode (active) | OFF | Sleeps for random interval, wakes to press |
| Wake Pin LOW | OFF | Cancels random mode and reboots to full UI |
| Manual / Web Control | ON | Normal powered mode with UI active |

---

## 🧩 Recommended Parts

- **ESP32-C3 Mini** (AITRIP or Seeed Studio XIAO ESP32C3)  
- **AQV212 PhotoMOS relay** (or equivalent 3–32 V DC input SSR)  
- **470 Ω series resistor** on control pin  
- **100 kΩ pulldown** for stability  
- **2000 mAh 3.7 V Li-Po** battery  
- **Momentary pushbutton** (GPIO9 → GND for wake)

---

## ⚠️ Safety Note

- The AQV212 output is **isolated dry contact** rated for **60 V DC @ 1 A** —  
  do *not* use directly on mains AC circuits.
- Use low-voltage circuits only (e.g., toy button triggers, control lines).

---

## 🧙 Credits

Developed for fun (and fright).  
“**Spooky Village Rocking Horse Doll Controller**” — 2025, by invaliddev403 🎃
