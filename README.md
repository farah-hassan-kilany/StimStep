# StimStep
Embedded C++ firmware for StimStep

**StimStep** is an embedded system designed to assist foot-drop patients by detecting gait events in real-time and delivering Functional Electrical Stimulation (FES) to the Tibialis Anterior muscle. The system utilizes an MPU6050 gyroscope/accelerometer, an EMG sensor, and a custom pulse-generation circuit controlled by an ESP32 microcontroller. 

> 📌 This repository contains the complete embedded code used in the final prototype. It is provided for academic reference **only** and not licensed for reuse or redistribution.

---

## System Features

- **Gait Detection:** Using MPU6050 data-ready interrupts and threshold-based walking detection algorithm.
- **Non-Blocking Biphasic FES Pulse Generation:** Biphasic stimulation using GPIO-controlled op-amp circuitry with real-time triggering.
- **EMG Feedback Acquisition:** Analog signal acquisition from the Tibialis Anterior muscle for detecting muscle contraction effectiveness.
- **Wi-Fi Sync:** Sends session data (step count, EMG logs, device status) to the Laravel backend via HTTP.
- **Web Server API** for real-time monitoring and control
- **Safety Logic** to prevent overstimulation and ensure patient safety

---

## Core Components

- **Microcontroller:** ESP32 (Wi-Fi enabled)
- **Sensors:**
  - MPU6050 (gyroscope + accelerometer)
  - EMG sensor on Tibialis Anterior muscle
- **Stimulation:** Biphasic FES via circuit of op-amps 
- **Communication:** Wi-Fi (HTTP POST + WebServer)

---

## File Structure

- `StimStep.ino` – Main embedded firmware

---

## 📈 Functional Flow

1. **Initialization**: Sets up MPU6050, Wi-Fi, GPIO, and interrupts.
2. **Interrupt-Driven Gait Detection**: Detects walking via MPU6050 gyroscope threshold.
3. **FES Activation**: Triggers biphasic pulse train through GPIO.
4. **EMG Monitoring**: Reads analog signal and stores data.
5. **Data Sync**: Sends step counts and EMG logs to backend upon Wi-Fi availability.

---

## 🛡️ Safety Features

- Automatic timeout after each stimulation window
- Maximum daily step goal enforcement
- Hardware-level enable/disable button

---

## 📲 App Integration

The system connects with a web app (via RESTful API) that allows:
- Doctor login and patient registration
- Real-time or periodic monitoring of step count and EMG activation
- Adjustment and testing of FES parameters

---

## 🚫 License

This project is under a **No License** clause.  
> That means:
- ✅ You may **view** the code for educational or evaluation purposes.
- ❌ You may **not copy, modify, reuse, or distribute** any part of the codebase.

---

## Author

> **Farah Hassan Mohammed Kilany**
>  Biomedical Engineering Department, Misr University for Science and Technology
>  Embedded System Developer – StimStep Project (2025)    
> For academic viewing only.

---

## Demo and Usage

- Designed to operate continuously throughout the day.
- Stimulus automatically stops once the preset daily step count is reached.
- Settings (pulse width, frequency, goal steps) are configured by a clinician through the companion app. (future work)

---

## 📌 Disclaimer

This project is part of an undergraduate graduation project and **is not intended for medical or commercial use**. The repository is kept online for transparency and academic archiving purposes only.
