# ESP32 with RFID Antenna and BLE Keyboard

This repository contains a portable RFID asset scanner built with an ESP32. It reads RFID cards/tags and wirelessly transmits the data as a Bluetooth Low Energy (BLE) keyboard input directly into spreadsheet applications.

## 🎥 Video Demonstration (prototype)

[Video Demonstration (prototype)](demo.mp4)

<p align="center">
  <video src="demo.mp4?raw=true" controls width="100%" style="max-width: 400px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1);">
    Your browser does not support the video tag.
  </video>
</p>

---

## 📸 Hardware Gallery (MVP)

Below is a breakdown of the 3D-printed enclosure assembly and internal component layout.

<table width="100%">
  <tr>
    <td align="center" width="33.33%" style="border: none; vertical-align: top;">
      <img src="electronics.jpg" alt="Internal components" width="100%" style="max-width: 250px; border-radius: 6px; box-shadow: 0 2px 5px rgba(0,0,0,0.15);"/>
      <br/>
      <sub><b>Internal Wiring</b><br>ESP32, RFID Reader v2.0 & LiPo Battery</sub>
    </td>
    <td align="center" width="33.33%" style="border: none; vertical-align: top;">
      <img src="front.jpg" alt="Enclosure Front" width="100%" style="max-width: 250px; border-radius: 6px; box-shadow: 0 2px 5px rgba(0,0,0,0.15);"/>
      <br/>
      <sub><b>Enclosure Front</b><br>OLED Display Cutout</sub>
    </td>
    <td align="center" width="33.33%" style="border: none; vertical-align: top;">
      <img src="back.jpg" alt="Enclosure Back" width="100%" style="max-width: 250px; border-radius: 6px; box-shadow: 0 2px 5px rgba(0,0,0,0.15);"/>
      <br/>
      <sub><b>Enclosure Back</b><br>RFID Antenna Panel View</sub>
    </td>
  </tr>
</table>

---

## 📱 ↦ 🖥️ UI Simulator (SDL + LVGL)

To accelerate interface development, the device UI was prototyped and tested on a desktop simulator using the **LVGL** graphics library and **SDL** framework.

<p align="center">
  <img src="SDL_simulator.png" alt="LVGL UI Simulator using SDL" width="100%" style="max-width: 700px; border-radius: 8px; box-shadow: 0 4px 10px rgba(0,0,0,0.2);"/>
</p>
