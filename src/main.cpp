#include <Arduino.h>
#include "HX711.h"
#include "soc/rtc.h"
#include <Pushbutton.h>

#include <WebSocketsClient.h>
#include <SocketIOclient.h>

#include <ArduinoJson.h>

#include <WiFiMulti.h>

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN = 4;

HX711 scale;
float reading;
float lastReading;

// REPLACE WITH YOUR CALIBRATION FACTOR
#define CALIBRATION_FACTOR 396.717

// Button
#define BUTTON_PIN 19
Pushbutton button(BUTTON_PIN);

WiFiMulti wifimulti;
SocketIOclient socketIO;

void hexdump(const void *mem, uint32_t len, uint8_t cols = 16)
{
    const uint8_t *src = (const uint8_t *)mem;
    Serial.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
    for (uint32_t i = 0; i < len; i++)
    {
        if (i % cols == 0)
        {
            Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
        }
        Serial.printf("%02X ", *src);
        src++;
    }
    Serial.printf("\n");
}

void socketIOEvent(socketIOmessageType_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case sIOtype_DISCONNECT:
        Serial.printf("[IOc] Disconnected!\n");
        break;
    case sIOtype_CONNECT:
        Serial.printf("[IOc] Connected to url: %s\n", payload);

        // join default namespace (no auto join in Socket.IO V3)
        socketIO.send(sIOtype_CONNECT, "/");
        break;
    case sIOtype_EVENT:
        Serial.printf("[IOc] get event: %s\n", payload);
        break;
    case sIOtype_ACK:
        Serial.printf("[IOc] get ack: %u\n", length);
        hexdump(payload, length);
        break;
    case sIOtype_ERROR:
        Serial.printf("[IOc] get error: %u\n", length);
        hexdump(payload, length);
        break;
    case sIOtype_BINARY_EVENT:
        Serial.printf("[IOc] get binary: %u\n", length);
        hexdump(payload, length);
        break;
    case sIOtype_BINARY_ACK:
        Serial.printf("[IOc] get binary ack: %u\n", length);
        hexdump(payload, length);
        break;
    }
}

void scaleInit()
{
    Serial.println("Initializing the scale");
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

    scale.set_scale(CALIBRATION_FACTOR); // this value is obtained by calibrating the scale with known weights; see the README for details
    scale.tare();                        // reset the scale to 0
}

void wifiInit()
{
    for (uint8_t t = 3; t > 0; t--)
    {
        Serial.printf("[SETUP] BOOT WAIT %d...\n", t);
        Serial.flush();
        delay(1000);
    }

    // disable AP
    if (WiFi.getMode() & WIFI_AP)
    {
        WiFi.softAPdisconnect(true);
    }

    wifimulti.addAP("MIWIFI_2G_3iyZ", "DmrTqfTw");

    // WiFi.disconnect();
    while (wifimulti.run() != WL_CONNECTED)
    {
        delay(100);
    }

    String ip = WiFi.localIP().toString();
    Serial.printf("[SETUP] WiFi Connected %s\n", ip.c_str());
}

void socketIoInit()
{
    // server address, port and URL
    socketIO.begin("192.168.1.133", 8081, "/socket.io/?EIO=4");

    // event handler
    socketIO.onEvent(socketIOEvent);
}

void socketIoSendData(float reading)
{

    // creat JSON message for Socket.IO (event)
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();

    // add evnet name
    // Hint: socket.on('event_name', ....
    array.add("mensaje");

    // add payload (parameters) for the event
    JsonObject param1 = array.createNestedObject();

    param1["Balanza"] = reading;
    // (uint32_t) weight;

    // JSON to String (serializion)
    String output;
    serializeJson(doc, output);

    // Send event
    socketIO.sendEVENT(output);

    // delay(2000);
}

void setup()
{
    Serial.begin(115200);
    setCpuFrequencyMhz(80);
    scaleInit();

    wifiInit();

    socketIoInit();
}

unsigned long messageTimestamp = 0;

void loop()
{

    socketIO.loop();
    uint64_t now = millis();
    

    if (button.getSingleDebouncedPress())
    {
        Serial.print("tare...");

        //Valor "bandera" para indicar que se esta calibrando la balanza.
        socketIoSendData(-9999);

        //Calibrando.
        scale.tare();
    }

    if (scale.wait_ready_timeout(200))
    {
        // reading = round(scale.get_units());
        reading = scale.get_units(5);
        // Serial.print("Weight: ");
        // Serial.println(reading, 2);
        lastReading = reading;

        if (now - messageTimestamp > 1000)
        {
            //Variable de control de tiempo
            messageTimestamp = now;

            // Control de lectura de la balanza entre -1gr y 1-gr se pone 0
            if(reading > -1 && reading < 1) {
                reading = 0;
            } 

            //Funcion de envío de data con el valor de la balanza.
            socketIoSendData(reading);
        }
    }
    else
    {
        Serial.println("HX711 not found.");
    }

    
}
