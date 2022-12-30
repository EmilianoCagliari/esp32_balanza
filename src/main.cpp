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
#include <BLE2902.h>

// ------------------ Const and Define variables START ------------------

// Bluetooth
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define bleServerName "LOADCELL_ESP32"

BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharacteristic;

bool deviceConnected = false;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

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


// Setup callbacks onConnect and onDisconnect
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    Serial.print("Connected");
  };
  void onDisconnect(BLEServer *pServer)
  {
    Serial.print("Disconnected");
    deviceConnected = false;
    pServer->getAdvertising()->start();
  }
};
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

void initLoadCell()
{
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  if (!scale.is_ready())
  {
    Serial.println("HX711 No Encontrado.");
  }
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

  //--------------------- LoadCell Configuration and INIT --------------------------
  initLoadCell();

  //--------------------- Bluetooth Configuration and INIT --------------------------
  // Create the BLE Device
  BLEDevice::init(bleServerName);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristics and Create a BLE Descriptor
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());
  
  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop()
{

  if (deviceConnected)
  {
    if ((millis() - lastTime) > timerDelay)
    {
      scale.set_scale();
      scale.tare();
      long reading = scale.get_units(10);

      char txString[8];
      dtostrf(reading, 1, 0, txString);
      
      pCharacteristic->setValue(txString);

      pCharacteristic->notify();

      Serial.println("Enviando Valor: " + String(txString) );
      // loadCellCharacteristics.setValue(txValue);
      // Serial.print("Resultado: ");
      // Serial.println(reading);

      // loadCellCharacteristics.notify();
      lastTime = millis();
      // txValue++;
    }
  }

  // loadCellCalibration();
}