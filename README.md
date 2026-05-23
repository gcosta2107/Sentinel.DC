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
  "alarme": {
    "ativo": false,
    "combinado": {
      "ativo": false,
      "estado": "normal",
      "historico": {
        "19545": {
          "datetime": "23/05/2026 15:22:44",
          "pessoas": 6,
          "temperatura": 31.57191,
          "tipo": "temp_alta+umid_baixa",
          "umidade": 38.69299
        },
        "38891": {
          "datetime": "23/05/2026 15:15:26",
          "pessoas": 6,
          "temperatura": 29.9417,
          "tipo": "temp_alta+umid_baixa",
          "umidade": 39.95184
        },
        "42345": {
          "datetime": "23/05/2026 15:04:54",
          "pessoas": 8,
          "temperatura": 27.88248,
          "tipo": "temp_alta+umid_alta",
          "umidade": 72.42255
        },
        "50255": {
          "datetime": "23/05/2026 14:52:57",
          "pessoas": 4,
          "temperatura": 27.3784,
          "tipo": "temp_alta+umid_alta",
          "umidade": 80.56311
        },
        "114212": {
          "datetime": "23/05/2026 15:16:41",
          "pessoas": 6,
          "temperatura": 30.1133,
          "tipo": "temp_alta+umid_baixa",
          "umidade": 39.82214
        },
        "232943": {
          "datetime": "23/05/2026 15:18:40",
          "pessoas": 6,
          "temperatura": 30.4994,
          "tipo": "temp_alta+umid_baixa",
          "umidade": 39.07446
        },
        "367667": {
          "datetime": "23/05/2026 15:50:39",
          "pessoas": 2,
          "temperatura": 27.18535,
          "tipo": "temp_alta+umid_alta",
          "umidade": 71.75116
        },
        "430582": {
          "datetime": "23/05/2026 14:59:18",
          "pessoas": 8,
          "temperatura": 27.64653,
          "tipo": "temp_alta+umid_alta",
          "umidade": 56.44659
        },
        "432662": {
          "datetime": "23/05/2026 15:51:44",
          "pessoas": 2,
          "temperatura": 27.02447,
          "tipo": "temp_alta+umid_alta",
          "umidade": 74.36041
        },
        "694314": {
          "pessoas": 5,
          "temperatura": 28.69758,
          "timestamp": 694,
          "umidade": 76.59583
        },
        "734103": {
          "pessoas": 5,
          "temperatura": 29.77009,
          "timestamp": 734,
          "umidade": 71.13318
        }
      },
      "tipo": "nenhum"
    },
    "estado": "normal",
    "nivel": "aviso",
    "temp_alarme": 26.55256,
    "temp_atual": 28.30076,
    "temp_estado": "quente",
    "umid_atual": 45.13983,
    "umid_estado": "ok"
  },
  "ambiente": {
    "temperatura": 28.30076,
    "ultima_leitura": "23/05/2026 15:56:02",
    "umidade": 45.42212
  },
  "button": false,
  "config": {
    "pessoas_max": 5,
    "temp_cold": 18,
    "temp_crit": 35,
    "temp_warn": 27,
    "umid_max": 55,
    "umid_min": 40
  },
  "led": true,
  "pessoas": {
    "alerta_lotacao": false,
    "entradas": 3,
    "historico": {
      "lotacao": {
        "13604": {
          "datetime": "23/05/2026 15:04:25",
          "maximo": 5,
          "total": 8
        },
        "14156": {
          "datetime": "23/05/2026 15:22:39",
          "maximo": 5,
          "total": 6
        },
        "27717": {
          "datetime": "23/05/2026 15:06:18",
          "maximo": 5,
          "total": 6
        },
        "29710": {
          "datetime": "23/05/2026 15:15:17",
          "maximo": 5,
          "total": 6
        },
        "134274": {
          "datetime": "23/05/2026 14:54:21",
          "maximo": 5,
          "total": 6
        },
        "141645": {
          "maximo": 5,
          "timestamp": 141,
          "total": 6
        }
      }
    },
    "maximo": 5,
    "saidas": 0,
    "total": 3,
    "ultimo_gesto": "23/05 15:53:15"
  },
  "sistema": {
    "ultimo_envio": "23/05/2026 15:55:58"
  },
  "test": {
    "int": 1
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
