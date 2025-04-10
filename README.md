# SmartPill-Dose ESP32 IoT Project

## Project Overview

SmartPill-Dose is an IoT-based smart medication dispensing system built on ESP32. The system connects to a Firebase Realtime Database to retrieve medication schedules, tracks pill dispensing, and provides real-time status updates.

## Repository Structure

```plaintext
smartpill-dose/
├── esp32/
│   └── iot/
│       ├── src/
│       │   └── [main.cpp](http://_vscodecontentref_/0)              # Main application entry point
│       ├── components/
│       │   ├── [firebase_Esp_connect.cpp](http://_vscodecontentref_/1)     # Simple Firebase connection test
│       │   ├── [firebase_medication_store.cpp](http://_vscodecontentref_/2) # Firebase medication storage implementation
│       │   ├── [firebaseresponse.cpp](http://_vscodecontentref_/3)          # Firebase stream handling implementation
│       │   └── [realtime.cpp](http://_vscodecontentref_/4)                  # NTP real-time clock implementation
│       ├── include/                  # Header files (if any)
│       └── [platformio.ini](http://_vscodecontentref_/5)            # PlatformIO configuration
```
