#include <Arduino.h>
#include <Wire.h>

// Config File
#include <config.h>

// Timers
#include <Ticker.h>

// Display
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Balanza
#include "HX711.h"

#include "soc/rtc.h"
#include <Pushbutton.h>

// WebSocket
#include <WebSocketsClient.h>
#include <SocketIOclient.h>

// Json
#include <ArduinoJson.h>

// Wifi
#include <WiFiMulti.h>

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN = 4;

// Scale Const
HX711 scale;
float reading;
float lastReading;

// Display Const
#define SCREEN_WIDTH 128 // OLED width,  in pixels
#define SCREEN_HEIGHT 64 // OLED height, in pixels
#define OLED_RESET -1    // OLED RESET

String msg;
unsigned long messageTimestamp = 0;

// REPLACE WITH YOUR CALIBRATION FACTOR
#define CALIBRATION_FACTOR 396.717

// Button
#define BUTTON_PIN 19
Pushbutton button(BUTTON_PIN);

// DeepSleep Button and Timer
#define BUTTON_PIN_BITMASK 0x200000000;
RTC_DATA_ATTR int dpTimeCount = 0; // Value saved in RTC (Deep Sleep Mode)
static int SLEEP_TIME = 300;       // SLEEP TIME IN SECONDS

Ticker timerSleep; // Timer Instance

// Wifi
WiFiMulti wifimulti;
boolean wifiConn = false;

// Socket
SocketIOclient socketIO;
boolean socketConn = true;
boolean roomJoined = false;
String roomName = "scale_device";

// create an OLED display object connected to I2C
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =============  Funciones ==============

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
    Serial.println((String) "SocketIoEvent Type: " + (String)type + (String) "%s\n");

    switch (type)
    {
    case sIOtype_DISCONNECT:
        Serial.printf("[IOc] Disconnected!\n");
        socketConn = false;
        break;
    case sIOtype_CONNECT:
        Serial.printf("[IOc] Connected to url: %s\n", payload);
        socketConn = true;
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

void displayString(String msg)
{
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 15);
    oled.print(msg);
    oled.display();
}

void wifiInit()
{
    for (int t = 3; t > 0; t--)
    {
        Serial.printf("[SETUP] BOOT WAIT %d...\n", t);
        msg = "Booting... \nplease wait. ";
        displayString(msg);
        Serial.flush();
        delay(1000);
    }

    // disable AP
    if (WiFi.getMode() & WIFI_AP)
    {
        WiFi.softAPdisconnect(true);
    }

    wifimulti.addAP(WIFI_SSID, WIFI_PASSWORD);
    // WiFi.disconnect();
    while (wifimulti.run() != WL_CONNECTED)
    {
        wifiConn = false;
        delay(100);
    }

    wifiConn = true;
    String ip = WiFi.localIP().toString();
    Serial.printf("[SETUP] WiFi Connected %s\n", ip.c_str());

    msg = "[SETUP] WiFi Connected " + ip;
    displayString(msg);
}

void socketIoInit()
{
    // server address, port and URL
    socketIO.begin(socketIO_HOST, socketIO_PORT, socketIO_URL); // Removido 3er argumento de conexion "/socket.io/?EIO=4"

    // server Auth headers
    // socketIO.setExtraHeaders("token": "AQUI VA EL JWT");

    // event handler
    socketIO.onEvent(socketIOEvent);
}

// 08:5b:d6:0b:0c:9d

void socketIoSendData(String event_name, void *data, size_t dataSize, const String &optionalData = "")
{

    // create JSON message for Socket.IO (event)
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();

    // Event name
    // Hint: socket.on('event_name', ....
    array.add(event_name);

    // add payload (parameters) for the event

    // [
    //     "Evento",
    //     "data" : {
    //         {"data": 0},
    //         {"room": "nested"}
    //     }
    // ]

    // Type of data is recieved
    if (dataSize == sizeof(float)) // Is float type
    {
        JsonObject param1 = doc.createNestedObject();
        float *floatData = static_cast<float *>(data);
        // Aquí puedes enviar *floatData a través de socket.io como un número flotante.
        float dataFloat = *floatData;

        param1["data"] = dataFloat;

        Serial.println("Data OptionalData: " + optionalData);
        if (optionalData.length() > 0)
        {
            param1["room"] = optionalData;
        }
    }
    else if (dataSize > 0) // Is String type
    {
        JsonObject param1 = doc.createNestedObject();

        String *stringData = static_cast<String *>(data);
        // Aquí puedes enviar *stringData a través de socket.io como una cadena.
        String dataString = *stringData;
        param1["room"] = dataString;
    }

    // Armado del array de objetos de datos

    // Serial.println("Data Socket Enviado:" + (String)reading);

    // (uint32_t) weight;

    // JSON to String (serializion)
    String output;
    serializeJson(doc, output);

    Serial.println("JSON OUTPUT:" + output);
    // Send event
    socketIO.sendEVENT(output);

    // delay(2000);
}

void displayInit()
{
    if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ;
    }
}

void displayWeight(float weight)
{
    oled.clearDisplay();

    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 10);
    // oled static text
    oled.print("Wifi: ");
    oled.print((wifiConn) ? "ok " : "! ");
    oled.display();

    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(60, 10);
    // oled static text

    oled.print("Socket: ");
    oled.print((socketConn) ? "ok " : "! ");

    oled.display();

    if (weight == -9999)
    {

        oled.setTextSize(2);
        oled.setTextColor(WHITE);
        oled.setCursor(5, 30);
        // oled static text
        oled.println("CALIBRANDO...");
        oled.display();
    }
    else
    {
        oled.setTextSize(1);
        oled.setTextColor(WHITE);
        oled.setCursor(0, 30);
        // oled static text
        oled.println("Peso:");
        oled.display();

        oled.setCursor(0, 45);
        oled.setTextSize(2);
        oled.print(weight);
        oled.print(" ");
        oled.print("g");
        oled.display();
    }
}

void startDeepSleep()
{
    // displayString("Entrando en modo descanso, presione el boton izquierdo para despertar.");
    oled.ssd1306_command(SSD1306_DISPLAYOFF); // Apaga el display - Falta agregar un timer para visualizar el mensaje y luego apagar.
    esp_deep_sleep_start();
}

void deepSleepTimer()
{

    dpTimeCount++;
    // Serial.println((String) "Timer: " + (String)dpTimeCount);

    if (dpTimeCount == SLEEP_TIME)
    {
        // Envio de dato para desconectar el "room" creado en el backend
        socketIoSendData("event_leave", &roomName, sizeof(roomName));
        Serial.println((String) "Modo DeepSleep Iniciado");
        startDeepSleep();
    }
}

// ============ Funciones CORE del ESP32 Arduino ============

void setup()
{
    Serial.begin(115200);
    dpTimeCount = 0;

    // ===== START DEEP SLEEP =======

    /* Variable to WakeUp the ESP32 */
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 1); // 1 = High, 0 = Low

    // ===== END DEEP SLEEP =======

    setCpuFrequencyMhz(80);

    displayInit();

    scaleInit();

    wifiInit();

    socketIoInit();
}

void loop()
{

    // if (socketConn)
    // {
    //     socketIO.loop();
    // }

    socketIO.loop();

    uint64_t now = millis();

    if (socketConn)
    {
        if (!roomJoined)
        {
            // Envio de dato para crear el "room" en el backend
            socketIoSendData("event_join", &roomName, sizeof(roomName));
            roomJoined = true;
        }
    }

    // Sentencia de calibración de balanza al pulsar el boton derecho
    if (button.getSingleDebouncedPress())
    {
        Serial.print("tare...");

        displayWeight(-9999);

        if (socketConn)
        {
            float myFloat = -9999;
            // Valor "bandera" para indicar que se esta calibrando la balanza.
            socketIoSendData("event_message", &myFloat, sizeof(myFloat));
        }

        // Calibrando.
        scale.tare();
    }

    // Sentencia de accion para comprobar cada 2000ms (2 seg)
    if (now - messageTimestamp > 2000)
    {

        // Variable de control de tiempo
        messageTimestamp = now;

        reading = scale.get_units(5);

        lastReading = reading;

        // Control de lectura de la balanza entre -1gr y 1-gr se pone 0
        if (reading > -1 && reading < 1)
        {
            reading = 0;
        }

        // Si el registro es 0 y el Timer no esta activo se activa
        if (reading == 0 && !timerSleep.active())
        {
            timerSleep.attach(1, deepSleepTimer);
        }

        // Si el registro es DIFERENTE de 0 y el Timer esta activado se desactiva.
        if (reading != 0 && timerSleep.active())
        {
            Serial.println((String) "Detach Timer");
            dpTimeCount = 0;
            timerSleep.detach();
        }

        displayWeight(reading);
        // Funcion de envío de data con el valor de la balanza.
        if (socketConn && roomJoined)
        {
            // Evento de envio de dato del peso
            socketIoSendData("event_message", &reading, sizeof(reading), roomName);
        }
    }
}
