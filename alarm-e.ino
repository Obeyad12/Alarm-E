/*
  Alarm-E (Arduino Mega)
  Features:
  - DS3231 RTC (24h “international” time)
  - Alarm set with 3 buttons (UP, NEXT, SET). Long-press SET to enter/exit alarm-set mode.
  - Rear DISMISS button to stop beeping when alarm time is reached (re-arms for next day).
  - Ultrasonic obstacle avoidance:
      forward → if obstacle: stop → reverse → turn left ~90° → try again
  - H-bridge: left motors share one channel, right motors share the other.
    Forward: both sides forward. Turn-left: right fwd + left rev. Reverse: both rev.
*/

#include <Wire.h>
#include "RTClib.h"

RTC_DS3231 rtc;

// -------------------- PIN MAP (adjust if needed) --------------------
// H-bridge (one channel per side). Using enable+direction pair per side:
const int EN_LEFT   = 5;   // PWM enable for left motors (Mega PWM)
const int INL_A     = 22;  // Left direction A
const int INL_B     = 23;  // Left direction B

const int EN_RIGHT  = 6;   // PWM enable for right motors (Mega PWM)
const int INR_A     = 24;  // Right direction A
const int INR_B     = 25;  // Right direction B

// Ultrasonic
const int US_TRIG   = 30;
const int US_ECHO   = 31;

// Buzzer
const int BUZZER    = 8;

// Buttons (use INPUT_PULLUP; LOW = pressed)
const int BTN_UP      = 32; // increment selected field
const int BTN_NEXT    = 33; // move across hour/min/sec
const int BTN_SET     = 34; // enter/exit alarm-set mode (long press), short press to save
const int BTN_DISMISS = 35; // rear dismiss button for alarm

// -------------------- BEHAVIOUR TUNING --------------------
const int MOTOR_SPEED_FWD   = 200; // 0–255
const int MOTOR_SPEED_TURN  = 200; // turn speed
const int MOTOR_SPEED_REV   = 180;

const unsigned long REVERSE_MS   = 600;
const unsigned long TURN90_MS    = 550; // tune on your chassis for ~90°
const int OBSTACLE_CM            = 20;  // stop if object closer than this

const unsigned long DEBOUNCE_MS  = 60;
const unsigned long LONGPRESS_MS = 700;

// Buzzer
const int BEEP_FREQ     = 2000;     // Hz for tone()
const int BEEP_ON_MS    = 200;
const int BEEP_OFF_MS   = 150;

// -------------------- STATE --------------------
bool inAlarmSetMode = false;
int  fieldIndex = 0;       // 0=hour, 1=min, 2=sec
int  alarmHour = 7;        // default alarm time
int  alarmMin  = 0;
int  alarmSec  = 0;
bool alarmEnabled = true;
bool alarmRinging = false;

// Button bookkeeping
struct Btn {
  int pin;
  bool lastStable;
  unsigned long lastChangeMs;
  bool lastRead;
  unsigned long pressedAt;
  bool longSent;
};
Btn btnUp      = { BTN_UP,      HIGH, 0, HIGH, 0, false };
Btn btnNext    = { BTN_NEXT,    HIGH, 0, HIGH, 0, false };
Btn btnSet     = { BTN_SET,     HIGH, 0, HIGH, 0, false };
Btn btnDismiss = { BTN_DISMISS, HIGH, 0, HIGH, 0, false };

// -------------------- UTIL --------------------
void setLeft(int pwm, int dir) { // dir: 1=fwd, -1=rev, 0=stop
  if (dir > 0) { digitalWrite(INL_A, HIGH); digitalWrite(INL_B, LOW); }
  else if (dir < 0) { digitalWrite(INL_A, LOW); digitalWrite(INL_B, HIGH); }
  else { digitalWrite(INL_A, LOW); digitalWrite(INL_B, LOW); }
  analogWrite(EN_LEFT, (dir == 0 ? 0 : constrain(pwm, 0, 255)));
}

void setRight(int pwm, int dir) {
  if (dir > 0) { digitalWrite(INR_A, HIGH); digitalWrite(INR_B, LOW); }
  else if (dir < 0) { digitalWrite(INR_A, LOW); digitalWrite(INR_B, HIGH); }
  else { digitalWrite(INR_A, LOW); digitalWrite(INR_B, LOW); }
  analogWrite(EN_RIGHT, (dir == 0 ? 0 : constrain(pwm, 0, 255)));
}

void motorsStop()    { setLeft(0, 0); setRight(0, 0); }
void motorsForward() { setLeft(MOTOR_SPEED_FWD,  +1); setRight(MOTOR_SPEED_FWD,  +1); }
void motorsReverse() { setLeft(MOTOR_SPEED_REV,  -1); setRight(MOTOR_SPEED_REV,  -1); }
void turnLeft()      { setLeft(MOTOR_SPEED_TURN, -1); setRight(MOTOR_SPEED_TURN, +1); }

long readDistanceCM() {
  digitalWrite(US_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(US_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(US_TRIG, LOW);
  unsigned long duration = pulseIn(US_ECHO, HIGH, 30000UL); // 30ms timeout (~5m)
  if (duration == 0) return 9999; // timeout
  return (long)(duration / 58);   // HC-SR04 conversion
}

void beepPattern() {
  static unsigned long lastToggle = 0;
  static bool on = false;
  unsigned long now = millis();
  unsigned long interval = on ? BEEP_ON_MS : BEEP_OFF_MS;

  if (now - lastToggle >= interval) {
    lastToggle = now;
    on = !on;
    if (on) tone(BUZZER, BEEP_FREQ);
    else    noTone(BUZZER);
  }
}

bool timesEqual(DateTime now, int h, int m, int s) {
  return (now.hour() == h && now.minute() == m && now.second() == s);
}

// Simple debounced + long-press buttons
void updateBtn(Btn &b) {
  bool raw = digitalRead(b.pin);
  if (raw != b.lastRead) {
    b.lastChangeMs = millis();
    b.lastRead = raw;
  }
  if (millis() - b.lastChangeMs > DEBOUNCE_MS) {
    if (b.lastStable != raw) {
      b.lastStable = raw;
      if (raw == LOW) { // pressed (active LOW)
        b.pressedAt = millis();
        b.longSent = false;
      } else {
        // released
      }
    }
  }
}

bool shortPressed(Btn &b) {
  // fire on release if was pressed and not long
  static unsigned long releasedAt = 0; // not strictly needed per-button here
  if (b.lastStable == HIGH && b.pressedAt > 0) {
    unsigned long pressLen = millis() - b.pressedAt;
    bool wasShort = (pressLen >= DEBOUNCE_MS && pressLen < LONGPRESS_MS && !b.longSent);
    b.pressedAt = 0;
    return wasShort;
  }
  return false;
}

bool longPressed(Btn &b) {
  if (b.lastStable == LOW && !b.longSent && (millis() - b.pressedAt >= LONGPRESS_MS)) {
    b.longSent = true;
    return true;
  }
  return false;
}

// -------------------- ALARM SET MODE --------------------
void enterAlarmSet() {
  inAlarmSetMode = true;
  fieldIndex = 0;
  motorsStop();
  noTone(BUZZER);
  Serial.println(F("[MODE] Enter ALARM-SET (UP=inc, NEXT=field, SET=save/exit)"));
}
void exitAlarmSet(bool saved) {
  inAlarmSetMode = false;
  if (saved) {
    alarmEnabled = true;
    Serial.print(F("[ALARM] Saved -> "));
    Serial.print(alarmHour); Serial.print(':');
    Serial.print(alarmMin);  Serial.print(':');
    Serial.println(alarmSec);
  } else {
    Serial.println(F("[MODE] Exit ALARM-SET (no save)"));
  }
}

void incrementField() {
  if (fieldIndex == 0) { alarmHour = (alarmHour + 1) % 24; }
  else if (fieldIndex == 1) { alarmMin = (alarmMin + 1) % 60; }
  else { alarmSec = (alarmSec + 1) % 60; }
  Serial.print(F("[ALARM SET] ")); Serial.print(alarmHour);
  Serial.print(':'); Serial.print(alarmMin);
  Serial.print(':'); Serial.print(alarmSec);
  Serial.print(F("  (field=")); Serial.print(fieldIndex); Serial.println(')');
}

void nextField() {
  fieldIndex = (fieldIndex + 1) % 3;
  Serial.print(F("[ALARM SET] Select field ")); Serial.println(fieldIndex); // 0=H,1=M,2=S
}

// -------------------- OBSTACLE AVOIDANCE --------------------
void obstacleAvoidanceStep() {
  long cm = readDistanceCM();
  if (cm <= OBSTACLE_CM) {
    // stop → reverse → turn left 90°
    motorsStop(); delay(120);
    motorsReverse(); delay(REVERSE_MS);
    motorsStop(); delay(100);
    turnLeft(); delay(TURN90_MS);
    motorsStop(); delay(120);
  } else {
    motorsForward();
  }
}

// -------------------- SETUP/LOOP --------------------
void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(EN_LEFT,  OUTPUT);
  pinMode(INL_A,    OUTPUT);
  pinMode(INL_B,    OUTPUT);

  pinMode(EN_RIGHT, OUTPUT);
  pinMode(INR_A,    OUTPUT);
  pinMode(INR_B,    OUTPUT);

  pinMode(US_TRIG,  OUTPUT);
  pinMode(US_ECHO,  INPUT);

  pinMode(BUZZER,   OUTPUT);

  pinMode(BTN_UP,      INPUT_PULLUP);
  pinMode(BTN_NEXT,    INPUT_PULLUP);
  pinMode(BTN_SET,     INPUT_PULLUP);
  pinMode(BTN_DISMISS, INPUT_PULLUP);

  motorsStop();
  noTone(BUZZER);

  if (!rtc.begin()) {
    Serial.println(F("[RTC] DS3231 not found! Check wiring."));
  } else {
    if (rtc.lostPower()) {
      Serial.println(F("[RTC] Time lost—setting to compile time."));
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  Serial.println(F("Alarm-E ready."));
  Serial.print(F("Alarm set to ")); Serial.print(alarmHour); Serial.print(':'); Serial.print(alarmMin);
  Serial.print(':'); Serial.println(alarmSec);
}

void loop() {
  // Update buttons
  updateBtn(btnUp);
  updateBtn(btnNext);
  updateBtn(btnSet);
  updateBtn(btnDismiss);

  // Handle mode switching via long-press SET
  if (longPressed(btnSet)) {
    if (!inAlarmSetMode) enterAlarmSet();
    else exitAlarmSet(false); // cancel on long-press exit
  }

  if (inAlarmSetMode) {
    if (shortPressed(btnUp))   incrementField();
    if (shortPressed(btnNext)) nextField();
    if (shortPressed(btnSet))  { exitAlarmSet(true); } // save & exit on short press
    // Stay stopped while setting
    motorsStop();
    delay(10);
    return;
  }

  // Time & Alarm
  DateTime now = rtc.now();

  // Trigger alarm at exact match (H:M:S) if enabled and not already ringing
  if (alarmEnabled && !alarmRinging && timesEqual(now, alarmHour, alarmMin, alarmSec)) {
    alarmRinging = true;
    motorsStop(); // freeze movement while ringing
    Serial.println(F("[ALARM] RINGING! Press DISMISS to stop."));
  }

  // If ringing: beep until dismiss
  if (alarmRinging) {
    beepPattern();

    if (shortPressed(btnDismiss) || longPressed(btnDismiss)) {
      alarmRinging = false;
      noTone(BUZZER);
      // Re-arm for tomorrow at same time:
      alarmEnabled = true;
      Serial.println(F("[ALARM] Dismissed. Re-armed for next day."));
    }
    // Do not drive while ringing
    delay(5);
    return;
  }

  // Normal navigation
  obstacleAvoidanceStep();

  // Safety: if rear dismiss pressed during normal operation, you could toggle alarm enable
  if (longPressed(btnDismiss)) {
    alarmEnabled = !alarmEnabled;
    Serial.print(F("[ALARM] Toggle enable -> "));
    Serial.println(alarmEnabled ? F("ON") : F("OFF"));
  }

  delay(10);
}
