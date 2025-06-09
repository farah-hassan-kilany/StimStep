# StimStep
Embedded C++ firmware for StimStep

**StimStep** is an embedded system designed to assist foot-drop patients by detecting gait events in real-time and delivering Functional Electrical Stimulation (FES) to the Tibialis Anterior muscle. The system utilizes an MPU6050 gyroscope/accelerometer, an EMG sensor, and a custom pulse-generation circuit controlled by an ESP32 microcontroller. 

> 📌 This repository contains the complete embedded code used in the final prototype. It is provided for academic reference **only** and not licensed for reuse or redistribution.

---

## System Features

- **Gait Detection:** Using MPU6050 data-ready interrupts and threshold-based walking detection algorithm.
- **FES Pulse Generation:** Biphasic stimulation using GPIO-controlled op-amp circuitry with real-time triggering.
- **EMG Feedback Monitoring:** Analog signal acquisition for detecting muscle contraction effectiveness.
- **Wi-Fi Sync:** Sends session data (step count, EMG logs, device status) to the backend via HTTP.
- **Web Server Interface:** Allows basic mobile control during initial device setup and testing.

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

- `stimstep.ino` – Main embedded firmware

---

## 🚫 License

This project is under a **No License** clause.  
> That means:
- ✅ You may **view** the code for educational or evaluation purposes.
- ❌ You may **not copy, modify, reuse, or distribute** any part of the codebase.

---

## 🧾 Citation

> *StimStep*  
> Farah Hassan Mohammed Kilany, Biomedical Engineering Department, Misr university for science and technology, 2025  
> For academic viewing only.

---

## 🧪 Demo and Usage

- Designed to operate continuously throughout the day.
- Stimulus automatically stops once the preset daily step count is reached.
- Settings (pulse width, frequency, goal steps) are configured by a clinician through the companion app. (future work)

---

## 📌 Disclaimer

This project is part of an undergraduate graduation project and **is not intended for medical or commercial use**. The repository is kept online for transparency and academic archiving purposes only.
