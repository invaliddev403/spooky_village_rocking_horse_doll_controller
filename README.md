# Spooky Village Rocking Horse Doll Controller 🕯️🎠

This project controls a *rocking horse doll* using an **ESP32-C3** microcontroller and a **Panasonic AQV212 solid-state relay**.  
It supports both **web-based manual control** and **randomized press activation** intervals, mimicking autonomous doll motion.

---

## 🧩 Hardware Overview

| Component | Description | Notes |
|------------|-------------|-------|
| **ESP32-C3** | Main microcontroller | Wi-Fi capable, deep-sleep support |
| **Panasonic AQV212** | Solid-state relay (SSR) | Controls the doll’s internal button |
| **Momentary button** | Wake-up button | Pulls `GPIO4` low to wake device |
| **Power source** | USB or 5V | Ensure stable supply for relay |

### 🔌 Pin Connections

#### AQV212 (Panasonic Solid-State Relay)

| AQV212 Pin | Function | Connects To | Notes |
|-------------|-----------|-------------|-------|
| **1** | Input (+) | ESP32-C3 `GPIO2` through 470Ω resistor | Relay control signal |
| **2** | Input (−) | GND | Input return |
| **3** | — | *No connection (NC)* | Leave unconnected |
| **4** | Output (load terminal 1) | Doll button wire 1 | Controls the button circuit |
| **5** | Output (load terminal 2) | Doll button wire 2 | Controls the button circuit |
| **6** | — | *No connection (NC)* | Leave unconnected |

> ✅ According to the Panasonic AQV212 datasheet, **pin 3 is a no-connect**.  
> Only **pins 4, 5, and 6** are the output terminals that replace the physical button.

### ESP32-C3 GPIO Assignments

| GPIO | Function | Description |
|------|-----------|-------------|
| `GPIO2` | Relay output | Drives AQV212 input pin 1 |
| `GPIO4` | Wake-up button | Wakes ESP32-C3 from deep sleep when pulled LOW |
| `A0` | Analog noise source | Used to seed RNG (optional) |

---

## ⚙️ Software Features

- **Local web server UI** (served via AP mode: `ESP32-Button`, pass `press1234`)
- **Manual button press** trigger from web page
- **Configurable randomized interval** between activations
- **Deep-sleep** between triggers for ultra-low power use
- **Wake pin** (GPIO4) for instant manual override (escape from sleep)

---

## 🌐 Web Interface

Open a browser to `http://192.168.4.1` after connecting to the AP.

### Pages
- `/` → Main control dashboard (HTML/JS UI)
- `/img` → Doll image
- `/api/status` → JSON status
- `/api/press` → Manual press
- `/api/config` → Update timing/press settings
- `/api/toggle` → Enable/disable randomized mode

---

## 💤 Deep Sleep Behavior

- When **enabled**, ESP32-C3 enters deep sleep for a random time between `min` and `max` minutes.  
- It wakes automatically via **timer** or manually via **GPIO4 LOW**.
- On wake:
  - Performs a simulated button press using AQV212
  - Returns to deep sleep for the next random interval

Holding the wake button (GPIO4 → GND) at power-up will **cancel sleep** and start normal Wi-Fi/AP mode.

---

## 🛠️ Building & Flashing

1. Open `spooky_village_rocking_horse_doll_controller.ino` in **Arduino IDE**
2. Select **ESP32-C3 Dev Module**
3. Compile & upload
4. Connect to `ESP32-Button` Wi-Fi network
5. Browse to `http://192.168.4.1`

---

## 📄 License

**MIT License**  
Copyright © 2025

---

## 💡 Notes

- Keep AQV212 output wires **isolated** from ESP32-C3 logic circuits.
- GPIO4 must be an **RTC IO** pin to function as a wake source (it is on the ESP32-C3).  
- Tested with **Panasonic AQV212** — other SSRs with similar pinouts should work fine.

---

🕯️ *Built for the Spooky Village installation project — 2025 by invaliddev403.*
