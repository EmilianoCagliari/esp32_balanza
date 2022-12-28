#include <Arduino.h>
#include <Wire.h>
// Display
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// Balanza - Load Cell
#include "soc/rtc.h"
#include "HX711.h"

// Bluetooth Low Energy
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// ------------------ Const and Define variables START ------------------

// Bluetooth
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Display Const
#define SCREEN_WIDTH 128 // OLED width,  in pixels
#define SCREEN_HEIGHT 64 // OLED height, in pixels

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN = 4;

// Global variable
HX711 scale;

// create an OLED display object connected to I2C
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ------------------ Const and Define variables END ------------------

// ------------------ Steps Functions INIT ------------------

void displayAndLoadCellInit()
{
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("failed to start SSD1306 OLED"));
    while (1)
      ;
  }
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  delay(1000);         // wait two seconds for initializing
  oled.clearDisplay(); // clear display

  oled.setTextSize(1);      // set text size
  oled.setTextColor(WHITE); // set text color
  oled.setCursor(0, 10);
}

void bluetoothInit()
{
  // Start BLE Device Init
  BLEDevice::init("ESP32 Balanza");

  // Creating Server
  BLEServer *pServer = BLEDevice::createServer();

  // Creating Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Adding Characteristics of Service
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE);

  pCharacteristic->setValue("Hello World says Neil");
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loadCellCalibration()
{

  // put your main code here, to run repeatedly:
  if (scale.is_ready())
  {

    

    scale.set_scale();

    // Serial.println("Tare... remove any weights from the scale.");
    Serial.println("");
    Serial.println("Peso de Tara... remueva cualquier peso de la balanza.");
    delay(500);
    scale.tare();
    // Serial.println("Tare done...");
    Serial.println("Peso de tara completo...");
    // Serial.print("Place a known weight on the scale...");
    Serial.print("Coloque un valor de peso conocido en la balanza...");
    delay(5000);
    Serial.print("Resultado: ");
    while (true)
    {
      long reading = scale.get_units(10);
      delay(100);
      
      Serial.print(reading);
    }

    // oled.println("Calibrando Peso Tara");
    // delay(500);
    // scale.tare();
    // oled.println("Calibrado completo...");
    // delay(500);
    // oled.clearDisplay();

    // oled.println("prueba de balanza"); // set text
    // oled.print("Resultado: ");
    // oled.display();

    // while (true)
    // {
    //   long reading = scale.get_units(10);
    //   delay(100);
    //   oled.print(reading);
    //   oled.display();
    // }
  }
  else
  {
    // Serial.println("HX711 not found.");
    Serial.println("HX711 No Encontrado.");
  }
  delay(1000);
}

// ------------------ Steps Functions END ------------------
void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);

  // Bluetooth Init Connection
  bluetoothInit();

  // initialize OLED display with I2C address 0x3C
  displayAndLoadCellInit();
}

void loop()
{
  loadCellCalibration();
}