# Alarm-E ⏰🤖  
*A modern alarm clock robot that makes sure you don’t just wake up but get up.*

## Overview
Alarm-E is a mobile alarm clock built on the Arduino Mega that drives around your room when the alarm goes off, forcing you to physically chase it down to turn it off.  
Unlike a regular alarm, it integrates real-time timekeeping, LCD display, obstacle detection, and autonomous navigation so it can move freely without getting stuck.

## Demo Videos
- 🎬 [Short version (17s)](https://youtu.be/4Ljw1DmDUfg) – straight to the point  
- 🎭 [Skit version (~47s)](https://youtu.be/VtErv5EelQ4) – a fun take showing Alarm-E in action  

*(Unlisted on YouTube but viewable via these links.)*

## Features
- **Real-time clock (RTC-DS3231):** keeps accurate time even when unplugged.  
- **LCD display:** shows the current time and alarm status in real-time.  
- **Custom alarm setting:** push buttons to set hours, minutes, and seconds in 24-hour format.  
- **Autonomous movement:**  
  - Moves forward until an obstacle is detected.  
  - On detection → stop → reverse → turn left ~90° → continue.  
- **Ultrasonic obstacle avoidance:** detects walls/objects and adapts its path.  
- **Motor control with H-bridge:** left and right motor groups wired for simple forward/turn logic.  
- **Alarm buzzer:** beeps continuously until dismissed.  
- **Rear dismiss button:** must be pressed physically on the robot to stop the alarm.  

## Tech Stack
- **Hardware:** Arduino Mega, RTC-DS3231 module, 16x2 LCD, HC-SR04 ultrasonic sensor, H-bridge motor driver, DC motors, buzzer, push buttons, 3D-printed chassis.  
- **Software:** Arduino C++ with iterative test–debug workflow (serial logging, targeted test cases).  
- **Tools:** Arduino IDE, oscilloscope/multimeter for motor signal validation.  

## How It Works
1. Set the time and alarm using the push buttons.  
2. Current time and alarm status are displayed on the LCD screen.  
3. When the alarm time is reached, the buzzer activates and the robot starts roaming.  
4. If it encounters an obstacle, it stops, reverses, and turns left before continuing.  
5. The alarm continues until the **rear dismiss button** is physically pressed.  

## Results
- Fully functional prototype tested in real environments.  
- Reliable alarm triggering with LCD status display and obstacle avoidance that prevents stalls.  
- Fun and effective way to **wake up and get moving**.  

## Next Steps
- Explore Bluetooth/mobile integration for wireless alarm setting.  
- Experiment with different obstacle avoidance patterns (random turns, right-turn logic).  

## Images
---
<img width="627" height="445" alt="alarme-6" src="https://github.com/user-attachments/assets/30d7d4e6-0d6c-4df0-ab05-c7200939dde0" />
<img width="1057" height="787" alt="alarme-4" src="https://github.com/user-attachments/assets/f75ba525-8721-4bc5-a3be-081910421575" />

### Author
Developed by **Obeyad A N M**, Computer Engineering student at Toronto Metropolitan University.  
📫 [LinkedIn](https://www.linkedin.com/in/obeyad-a-n-m-anowarul-6a6608236/) | [Portfolio](https://obeyadanm.netlify.app/) | [GitHub](https://github.com/Obeyad12)
