// =============================================================================
//  ble_link.h - Bluetooth LE status/control link
//
//  Uses the "Nordic UART Service" layout, so any BLE terminal app works
//  (nRF Connect, Serial Bluetooth Terminal, Bluefruit Connect).
//    - The unit sends a one-line status every 2 s.
//    - Send the text  start  /  stop  /  status  to control it.
//  (The ESP32-C3 has BLE only; there is no classic Bluetooth serial.)
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

#if ENABLE_BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

extern volatile bool reqStart, reqStop;
String statusLine();

#define NUS_SERVICE "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"   // phone -> unit
#define NUS_TX      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"   // unit -> phone

static BLECharacteristic* bleTx = nullptr;
static volatile bool bleConnected = false;
static volatile bool bleSendNow = false;
static uint32_t bleLastMs = 0;

class GcpServerCb : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { bleConnected = true; }
  void onDisconnect(BLEServer* s) override { bleConnected = false; s->startAdvertising(); }
};

class GcpRxCb : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    String v = String(c->getValue().c_str());
    v.trim(); v.toLowerCase();
    // Only set flags here; the main loop does the actual work.
    if (v == "start") reqStart = true;
    else if (v == "stop") reqStop = true;
    bleSendNow = true;
  }
};

static void bleBegin() {
  BLEDevice::init(BLE_NAME);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new GcpServerCb());
  BLEService* svc = server->createService(NUS_SERVICE);
  bleTx = svc->createCharacteristic(NUS_TX, BLECharacteristic::PROPERTY_NOTIFY);
  bleTx->addDescriptor(new BLE2902());
  BLECharacteristic* rx = svc->createCharacteristic(
      NUS_RX, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->setCallbacks(new GcpRxCb());
  svc->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
}

static void bleLoop() {
  if (!bleConnected || !bleTx) return;
  if (!bleSendNow && millis() - bleLastMs < 2000) return;
  bleSendNow = false;
  bleLastMs = millis();
  String s = statusLine() + "\n";
  // A BLE notification carries 20 bytes unless a larger MTU was negotiated.
  for (size_t i = 0; i < s.length(); i += 20) {
    size_t n = s.length() - i; if (n > 20) n = 20;
    bleTx->setValue((uint8_t*)s.c_str() + i, n);
    bleTx->notify();
    delay(8);
  }
}

#else
static inline void bleBegin() {}
static inline void bleLoop() {}
#endif
