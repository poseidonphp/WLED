#pragma once

#include "wled.h"

#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

// Pin defaults for Status Lights (change if desired)
#ifndef STATUSLIGHT_RED_PIN
  #ifdef ARDUINO_ARCH_ESP32
    #define STATUSLIGHT_RED_PIN 18
  #else //ESP8266 boards
    #define STATUSLIGHT_RED_PIN -1
  #endif
#endif

#ifndef STATUSLIGHT_BLUE_PIN
  #ifdef ARDUINO_ARCH_ESP32
    #define STATUSLIGHT_BLUE_PIN 19
  #else //ESP8266 boards
    #define STATUSLIGHT_BLUE_PIN -1
  #endif
#endif


class UsermodStatusLights : public Usermod {
private:
  // ---- Settings ----
  int8_t redPin  = STATUSLIGHT_RED_PIN;
  int8_t bluePin = STATUSLIGHT_BLUE_PIN;

  bool enableWifiCheck = true;

  // ---- Runtime state ----
  unsigned long lastBlinkMs = 0;
  bool blinkState = false;

  void setupPin(int8_t pin) {
    if (pin >= 0) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    }
  }

  void setLed(int8_t pin, bool on) {
    if (pin >= 0) digitalWrite(pin, on ? HIGH : LOW);
  }

  void updateWifiLed() {
    if (!enableWifiCheck || bluePin < 0) return;

    bool connected = (WiFi.status() == WL_CONNECTED);

    if (connected) {
      // solid blue
      setLed(bluePin, true);
    } else {
      // blink blue
      unsigned long now = millis();
      if (now - lastBlinkMs >= 500) { // 2Hz blink
        lastBlinkMs = now;
        blinkState = !blinkState;
      }
      setLed(bluePin, blinkState);
    }
  }

public:
  void setup() override {
    setupPin(redPin);
    setupPin(bluePin);
  }

  void loop() override {
    updateWifiLed();

    // Red LED behaviors (warnings / blown fuse) can be layered in later.
  }

  // Shows a little runtime info on the info JSON (optional)
  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray lines = user.createNestedArray("Status Lights");
    lines.add(String("WiFi: ") + (WiFi.status() == WL_CONNECTED ? "OK" : "NO"));
  }

  // Persist settings into cfg.json
  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject("Status Lights");
    top["red_pin"] = redPin;
    top["blue_pin"] = bluePin;
    top["wifi_check"] = enableWifiCheck;
  }

  // Load settings from cfg.json
  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root["Status Lights"];
    if (top.isNull()) return false;

    redPin = top["red_pin"] | redPin;
    bluePin = top["blue_pin"] | bluePin;
    enableWifiCheck = top["wifi_check"] | enableWifiCheck;

    return true;
  }

  // uint16_t getId() override { return USERMOD_ID_USERMOD_STATUS_LIGHTS; }
};

// You'll need to add a unique ID in usermod_list.cpp or use an existing "custom usermod id" slot.
// If you don't want to touch IDs yet, you can remove getId(); WLED will still run the mod,
// but some integrations rely on IDs.
