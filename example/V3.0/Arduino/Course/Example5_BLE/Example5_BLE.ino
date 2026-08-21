/*---------------------------------------------------------------
 * Bluetooth Low Energy server lesson
 * Advertise one service with a readable, writable, notifiable value.
 *--------------------------------------------------------------*/

#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"
#include <BLECharacteristic.h>


/*---------------------------------------------------------------
 * BLE object references
 * Retain access to the objects created during server initialization.
 *--------------------------------------------------------------*/

BLEAdvertising* pAdvertising = NULL;     // Controls packets that make the server discoverable.
BLEServer* pServer = NULL;               // Represents the local BLE server.
BLEService *pService = NULL;             // Groups the lesson characteristic under one service.
BLECharacteristic* pCharacteristic = NULL; // Stores the value exposed to BLE clients.

// Human-readable device name shown during BLE discovery.
#define bleServerName "ESP32SPI-BLE"

// Stable identifiers that allow a client to locate the lesson service and value.
#define SERVICE_UUID "6479571c-2e6d-4b34-abe9-c35116712345"
#define CHARACTERISTIC_UUID "826f072d-f87c-4ae6-a416-6ffdcaa02d73"

// Records whether a remote client currently has an active connection.
bool connected_state = false;


/*---------------------------------------------------------------
 * Track the connection state
 * Let the BLE stack update application state through callbacks.
 *--------------------------------------------------------------*/

class MyServerCallbacks: public BLEServerCallbacks {
  /**
   * @brief Record that a BLE client has connected.
   *
   * @param pServer Server that accepted the connection.
   * @return Nothing.
   * @note Called automatically by the BLE stack on connection.
   */
  void onConnect(BLEServer *pServer) {
    connected_state = true;
    Serial.println("Bluetooth connected");
  }

  /**
   * @brief Record that the current BLE client has disconnected.
   *
   * @param pServer Server whose connection ended.
   * @return Nothing.
   * @note Called automatically by the BLE stack on disconnection.
   */
  void onDisconnect(BLEServer *pServer) {
    connected_state = false;
    Serial.println("Bluetooth connection failed");
    BLEDevice::startAdvertising();
    Serial.println("Bluetooth ready");
  }
};


/*---------------------------------------------------------------
 * Build and advertise the BLE service
 * Create the server hierarchy before making it visible to clients.
 *--------------------------------------------------------------*/

/**
 * @brief Initialize the BLE server and begin advertising.
 *
 * The characteristic supports read, write, and notify operations so the
 * same value can demonstrate the three common client interaction patterns.
 *
 * @param None.
 * @return Nothing.
 * @note Called once by the Arduino framework after reset.
 */
void setup() {
  Serial.begin(115200);

  BLEDevice::init(bleServerName);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->setValue("ELECROW");

  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pService->start();
  pAdvertising->start();
  Serial.println("Bluetooth ready");
}

/**
 * @brief Leave BLE communication to the event-driven stack.
 *
 * @param None.
 * @return Nothing.
 * @note Called repeatedly after setup(); callbacks handle connection events.
 */
void loop() {
}
