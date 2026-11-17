// =============================================================================
//  ui.h - STAGE 2: status LED and button            *** CURRENTLY DISABLED ***
//
//  Every line of LED/button code in this project starts with the marker  //UI
//  To enable it after the benchmark passes:
//    1. Edit > Find, search for "//UI " (with the trailing space), replace with
//       nothing, Replace All - in THIS tab and in the SmartGCP tab.
//    2. Check PIN_LED_R/G/B, PIN_BUTTON and LED_COMMON_ANODE in config.h.
//
//  Wiring assumed:
//    RGB LED (4 pins): R, G, B each through a ~220-330 ohm resistor to its GPIO;
//                      common pin to GND (common cathode) or 3V3 (common anode).
//    Button: momentary, between PIN_BUTTON and GND (internal pull-up is used).
//
//  Behaviour:
//    Green steady     logging, receiver has a position fix
//    Green blinking   logging, still waiting for a fix
//    Red steady       powered but not logging (not in use)
//    Red fast blink   fault: SD card missing/failing or no data from the UM980
//    Short press      start / stop logging
//    Hold 3 s         stop logging safely, then deep sleep (press again to wake)
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

//UI #include "esp_sleep.h"
//UI #include "driver/rtc_io.h"
//UI #include "gnss.h"
//UI #include "storage.h"
//UI
//UI extern volatile bool reqStart, reqStop;
//UI
//UI static void ledSet(bool r, bool g, bool b) {
//UI   // On a common-anode LED a LOW output turns the colour on.
//UI   digitalWrite(PIN_LED_R, (r != (bool)LED_COMMON_ANODE) ? HIGH : LOW);
//UI   digitalWrite(PIN_LED_G, (g != (bool)LED_COMMON_ANODE) ? HIGH : LOW);
//UI   digitalWrite(PIN_LED_B, (b != (bool)LED_COMMON_ANODE) ? HIGH : LOW);
//UI }
//UI
//UI static void uiBegin() {
//UI   pinMode(PIN_LED_R, OUTPUT);
//UI   pinMode(PIN_LED_G, OUTPUT);
//UI   pinMode(PIN_LED_B, OUTPUT);
//UI   pinMode(PIN_BUTTON, INPUT_PULLUP);
//UI   ledSet(true, false, false);                       // red: powered, not logging yet
//UI }
//UI
//UI static void uiUpdateLed() {
//UI   uint32_t now = millis();
//UI   bool fault = !sdOk || !gnssAlive() || session.writeErrors > 0;
//UI   if (fault)               ledSet((now / 125) % 2, false, false);    // red fast blink
//UI   else if (!session.active) ledSet(true, false, false);             // red steady
//UI   else if (gnssHasFix())    ledSet(false, true, false);             // green steady
//UI   else                      ledSet(false, (now / 500) % 2, false);  // green blink
//UI }
//UI
//UI static void uiSleep() {
//UI   sessionStop();                                    // close the file cleanly first
//UI   ledSet(false, false, false);
//UI   while (digitalRead(PIN_BUTTON) == LOW) delay(10); // wait for release
//UI   delay(50);
//UI   // NOTE: this sleeps the ESP32 only. The UM980 stays powered from the 5 V
//UI   // regulator unless the hardware switches that rail off.
//UI #if CONFIG_IDF_TARGET_ESP32C3
//UI   esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);
//UI #else
//UI   rtc_gpio_pullup_en((gpio_num_t)PIN_BUTTON);
//UI   rtc_gpio_pulldown_dis((gpio_num_t)PIN_BUTTON);
//UI   esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BUTTON, 0);
//UI #endif
//UI   esp_deep_sleep_start();                           // wakes = normal reboot
//UI }
//UI
//UI static void uiUpdateButton() {
//UI   static bool wasDown = false, longDone = false;
//UI   static uint32_t downAt = 0;
//UI   bool down = (digitalRead(PIN_BUTTON) == LOW);
//UI   uint32_t now = millis();
//UI   if (down && !wasDown) { downAt = now; longDone = false; }
//UI   if (down && !longDone && now - downAt >= BUTTON_LONG_MS) { longDone = true; uiSleep(); }
//UI   if (!down && wasDown && !longDone && now - downAt >= 40) {   // 40 ms debounce
//UI     if (session.active) reqStop = true; else reqStart = true;
//UI   }
//UI   wasDown = down;
//UI }
//UI
//UI static void uiLoop() {
//UI   uiUpdateLed();
//UI   uiUpdateButton();
//UI }
