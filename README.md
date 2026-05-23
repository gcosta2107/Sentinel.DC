# Sentinel.DC — Smart Data Center Monitor

---

# Overview

Real-time monitoring of temperature, humidity, room occupancy, and vibration in server rooms, with local visual alerts (RGB LED + bar graph + OLED) and a Firebase-powered web dashboard.

# The problem

Server rooms and corporate mini data centers suffer from silent failures: thermal drift outside the ASHRAE A1 range, inadequate humidity, unauthorized physical access, and impacts/vibrations that can compromise sensitive hardware. Commercial DCIM (Data Center Infrastructure Management) solutions are expensive and rarely accessible to academic labs, small companies, or security research environments.

# Proposed solution

Sentinel.DC is a low-cost embedded system that combines environmental monitoring, people counting, and vibration detection on a single platform, with cloud synchronization via Firebase Realtime Database and a responsive web dashboard.

Beyond operational use, the project was designed to serve as a research platform for data center physical security — generating auditable trails (entries/exits, thermal spikes, impacts) that can be correlated with SIEM logs and forensic investigations.

The system continuously monitors:

- Temperature
- Humidity
- Room occupancy
- Vibrations / impacts

All collected data is processed locally on an ESP32 and synchronized to Firebase Realtime Database, enabling remote visualization through a responsive web dashboard.

The platform also provides immediate local alerts using:

- RGB LED status indicators
- OLED display
- Critical temperature alarm

---

# Architecture

```text
┌────────────────────┐
│      Sensors       │
│--------------------│
│ HTU21D             │
│ APDS-9960          │
│ Vibration Sensor   │
└─────────┬──────────┘
          │
          ▼
┌────────────────────┐
│       ESP32        │
│--------------------│
│ Local Processing   │
│ Threshold Analysis │
│ Firebase Sync      │
│ OLED Interface     │
│ RGB Alert System   │
└─────────┬──────────┘
          │
   Wi-Fi / Internet
          │
          ▼
┌────────────────────┐
│ Firebase Realtime  │
│     Database       │
└─────────┬──────────┘
          │
          ▼
┌────────────────────┐
│   Web Dashboard    │
│--------------------│
│ Live Metrics       │
│ Alerts             │
│ Historical Data    │
│ Occupancy Tracking │
└────────────────────┘
```

---

# 🔌 Hardware Components

| Component | Purpose |
|---|---|
| ESP32 | Main microcontroller |
| HTU21D | Temperature & humidity monitoring |
| APDS-9960 | Gesture recognition & people counting |
| OLED SSD1306 | Local display |
| RGB LED | Visual alarm system |
| Buzzer / Relay | Critical alarm output |
| Vibration Sensor | Tamper / impact detection |

---

# Pin Mapping

| Device | Pin |
|---|---|
| SDA | D21 |
| SCL | D22 |
| RGB Red | D19 |
| RGB Green | D23 |
| RGB Blue | D18 |
| OLED Button | D27 |
| Buzzer / Relay | D33 |

---

# Firebase Structure

```json
{
  "ambiente": {
    "temperatura": 24.5,
    "umidade": 55.2
  },
  "pessoas": {
    "entradas": 10,
    "saidas": 7,
    "total": 3
  },
  "alarme": {
    "ativo": true,
    "estado": "CRITICO",
    "temp_alarme": 36.7,
    "historico": {
      "1716230000": {
        "temperatura": 36.7,
        "timestamp": 1716230000
      }
    }
  }
}
```

---

# OLED Interface

The OLED display rotates between 3 monitoring screens:

1. Temperature & humidity
2. Occupancy statistics
3. Alarm & system status

Screen switching is controlled through the button connected to pin D27.

---

# Alert Logic

| Status | Color |
|---|---|
| Normal | Green |
| Warning | Yellow |
| Critical | Red |

The system automatically changes LED color according to environmental conditions and configured thresholds.

---

# Research Perspective

- Detecting unauthorized access outside business hours
- Identifying thermal spikes during infrastructure failures
- Creating environmental timelines during incident response

---

# PlatformIO Dependencies

```ini
lib_deps =
    mobizt/Firebase ESP32 Client@^4.4.17
    adafruit/Adafruit SSD1306@^2.5.9
    adafruit/Adafruit GFX Library@^1.11.9
    sparkfun/SparkFun APDS9960 RGB and Gesture Sensor@^1.4.3
    sparkfun/SparkFun HTU21D Humidity and Temperature Sensor Breakout@^1.1.3
```

---

# Monitoring Metrics

| Metric | Purpose |
|---|---|
| Temperature | Detect overheating conditions |
| Humidity | Prevent electrostatic or condensation risks |
| Occupancy | Track room access and usage |
| Vibration | Detect impacts, movement, or tampering |
