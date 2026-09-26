#pragma once

#include "wled.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// Pin defaults for Fan Control (change if desired)
#ifndef FANCONTROL_FAN_PIN
  #ifdef ARDUINO_ARCH_ESP32
    #define FANCONTROL_FAN_PIN 33
  #else //ESP8266 boards
    #define FANCONTROL_FAN_PIN -1
  #endif
#endif

#ifndef FANCONTROL_TEMP_PIN
  #ifdef ARDUINO_ARCH_ESP32
    #define FANCONTROL_TEMP_PIN 23
  #else //ESP8266 boards
    #define FANCONTROL_TEMP_PIN -1
  #endif
#endif


class UsermodFanControl : public Usermod {
private:
  // ---- Settings ----
  int8_t fanPin  = FANCONTROL_FAN_PIN;
  int8_t tempPin = FANCONTROL_TEMP_PIN;

  std::unique_ptr<OneWire> oneWire;
  std::unique_ptr<DallasTemperature> sensors;
  bool tempSensorPresent = false;

  bool forceFanOn = false;

  uint16_t tempCheckIntervalSec = 60;
  bool enableOverheatCheck = true;

  // ---- Behavior tuning ----
  float fanOnC  = 50.0f;   // hard-coded threshold you mentioned (change later if desired)
  float fanOffC = 45.0f;   // hysteresis to avoid rapid cycling

  // ---- Runtime state ----
  unsigned long lastTempReadMs = 0;
  float lastTempC = NAN;
  bool fanOn = false;

  void setupPin(int8_t pin) {
    if (pin >= 0) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    }
  }

  float readTemperatureC() {
    if (!tempSensorPresent || !sensors) return NAN;

    sensors->requestTemperatures();
    float t = sensors->getTempCByIndex(0);

    // DS18B20 returns these when missing/faulty
    if (t == DEVICE_DISCONNECTED_C || t < -55 || t > 125) {
      return NAN;
    }

    return t;
  }

  void updateFanFromTemp() {
    if (fanPin < 0) return;

    // Force override: always keep fan on, regardless of temperature sensor
    if (forceFanOn) {
      if (!fanOn) {
        fanOn = true;
        digitalWrite(fanPin, HIGH);
      }
      return; // skip all other logic
    }

    // If overheat/temp control is disabled, ensure fan is off
    if (!enableOverheatCheck) {
      if (fanOn) {
        fanOn = false;
        digitalWrite(fanPin, LOW);
      }
      return;
    }

    // No sensor present -> do nothing (fan remains off)
    if (!tempSensorPresent) return;

    unsigned long now = millis();
    if (now - lastTempReadMs < (unsigned long)tempCheckIntervalSec * 1000UL) return;
    lastTempReadMs = now;

    float t = readTemperatureC();
    if (!isnan(t)) lastTempC = t;

    if (isnan(lastTempC)) return;

    if (!fanOn && lastTempC >= fanOnC) {
      fanOn = true;
      digitalWrite(fanPin, HIGH);
    } else if (fanOn && lastTempC <= fanOffC) {
      fanOn = false;
      digitalWrite(fanPin, LOW);
    }
  }

public:
  void setup() override {
    setupPin(fanPin);

    if (tempPin >= 0) {
      oneWire.reset(new OneWire(tempPin));
      sensors.reset(new DallasTemperature(oneWire.get()));
      sensors->begin();

      sensors->requestTemperatures();
      float t = sensors->getTempCByIndex(0);
      tempSensorPresent = (t > -55 && t < 125); // DS18B20 valid range
    }

    // Make sure initial state is sane
    if (fanPin >= 0) digitalWrite(fanPin, LOW);
  }

  void loop() override {
    updateFanFromTemp();
  }

  // Shows a little runtime info on the info JSON (optional)
  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray lines = user.createNestedArray("Fan Control");
    if (!isnan(lastTempC)) lines.add(String("Temp: ") + String(lastTempC, 1) + "C");
    if (fanPin >= 0)       lines.add(String("Fan: ") + (fanOn ? "ON" : "OFF"));
  }

  // Persist settings into cfg.json
  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject("Fan Control");
    top["fan_pin"] = fanPin;
    top["temp_pin"] = tempPin;
    top["temp_check_s"] = tempCheckIntervalSec;
    top["overheat_check"] = enableOverheatCheck;
    top["force_fan_on"] = forceFanOn;
  }

  // Load settings from cfg.json
  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root["Fan Control"];
    if (top.isNull()) return false;

    fanPin = top["fan_pin"] | fanPin;
    tempPin = top["temp_pin"] | tempPin;

    tempCheckIntervalSec = top["temp_check_s"] | tempCheckIntervalSec;
    enableOverheatCheck = top["overheat_check"] | enableOverheatCheck;

    forceFanOn = top["force_fan_on"] | forceFanOn;

    return true;
  }

  // uint16_t getId() override { return USERMOD_ID_USERMOD_FAN_CONTROL; }
};

// You'll need to add a unique ID in usermod_list.cpp or use an existing "custom usermod id" slot.
// If you don't want to touch IDs yet, you can remove getId(); WLED will still run the mod,
// but some integrations rely on IDs.
