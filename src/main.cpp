#include <Arduino.h>
#include <FS.h>
#include <LiquidCrystal_I2C.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

#include "RTClib.h"

#define MAX_PRESSURE_ENTRIES 2016
const char *LOG_FILE = "/pressure_log.csv";

struct PressureEntry {
    float pressure;
    DateTime timestamp;
};

PressureEntry pressureLog[MAX_PRESSURE_ENTRIES];
int pressureIndex = 0;

const char *ssid = "ESP32-Clock";
const char *password = "12345678";

WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 20, 4);  // ← Updated to 20x4
RTC_DS3231 rtc;

String customMessage = "Welcome!";

void handleRoot() {
    DateTime now = rtc.now();
    char timeStr[6];
    char dateStr[11];
    sprintf(timeStr, "%02d:%02d", now.hour(), now.minute());
    sprintf(dateStr, "%04d-%02d-%02d", now.year(), now.month(), now.day());

    String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="es">
  <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Reloj ESP32</title>
      <style>
          body {
              font-family: Arial, sans-serif;
              text-align: center;
              margin: 0;
              padding: 20px;
              background-color: #f5f5f5;
          }
          h2 {
              color: #333;
          }
          form {
              background-color: #fff;
              padding: 15px;
              margin-bottom: 20px;
              border-radius: 8px;
              box-shadow: 0 0 10px rgba(0,0,0,0.1);
          }
          input[type="text"],
          input[type="time"],
          input[type="date"] {
              padding: 10px;
              font-size: 16px;
              width: 100%;
              margin-bottom: 10px;
              border: 1px solid #ccc;
              border-radius: 4px;
          }
          button {
              padding: 10px 20px;
              font-size: 16px;
              background-color: #007BFF;
              color: white;
              border: none;
              border-radius: 4px;
              cursor: pointer;
          }
          button:hover {
              background-color: #0056b3;
          }
      </style>
  </head>
  <body>
      <h2>Configurar Fecha y Hora</h2>
      <form action="/set-time" method="GET">
          <input type="date" name="date" value=")rawliteral" +
                  String(dateStr) + R"rawliteral(" required>
          <input type="time" name="time" value=")rawliteral" +
                  String(timeStr) + R"rawliteral(" required>
          <button type="submit">Actualizar Fecha y Hora</button>
      </form>

      <h2>Mensaje Personalizado</h2>
      <form action="/set-message" method="GET">
          <input type="text" name="message" maxlength="20" placeholder="Mensaje (máx. 20)" required>
          <button type="submit">Actualizar Mensaje</button>
      </form>

      <h2>Historial de Presión</h2>
      <table>
        <tr><th>Fecha</th><th>Hora</th><th>Presión (PSI)</th></tr>
    )rawliteral";

    for (int i = 0; i < pressureIndex; i++) {
        char row[128];
        DateTime t = pressureLog[i].timestamp;
        sprintf(row, "<tr><td>%04d-%02d-%02d</td><td>%02d:%02d</td><td>%.2f</td></tr>",
                t.year(), t.month(), t.day(), t.hour(), t.minute(), pressureLog[i].pressure);
        html += row;
    }

    html += R"rawliteral(
      </table>
      <br><br>
      <form action="/download" method="GET">
          <button type="submit">Descargar CSV</button>
      </form>
      <form action="/clear-log" method="GET" onsubmit="return confirm('¿Seguro que quieres borrar el historial?');">
          <button type="submit" style="background-color:red;">Borrar Historial</button>
      </form>
  </body>
  </html>
  )rawliteral";

    server.send(200, "text/html", html);
}

void saveLogToSPIFFS() {
    File file = SPIFFS.open(LOG_FILE, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open log file for writing");
        return;
    }

    for (int i = 0; i < pressureIndex; i++) {
        DateTime t = pressureLog[i].timestamp;
        file.printf("%04d-%02d-%02d,%02d:%02d,%.2f\n",
                    t.year(), t.month(), t.day(),
                    t.hour(), t.minute(), pressureLog[i].pressure);
    }

    file.close();
    Serial.println("Log saved to SPIFFS");
}

void loadLogFromSPIFFS() {
    File file = SPIFFS.open(LOG_FILE, FILE_READ);
    if (!file) {
        Serial.println("No previous log found.");
        return;
    }

    pressureIndex = 0;
    while (file.available() && pressureIndex < MAX_PRESSURE_ENTRIES) {
        String line = file.readStringUntil('\n');
        int y, m, d, h, min;
        float p;
        if (sscanf(line.c_str(), "%d-%d-%d,%d:%d,%f", &y, &m, &d, &h, &min, &p) == 6) {
            pressureLog[pressureIndex].pressure = p;
            pressureLog[pressureIndex].timestamp = DateTime(y, m, d, h, min, 0);
            pressureIndex++;
        }
    }

    file.close();
    Serial.printf("Loaded %d entries from SPIFFS\n", pressureIndex);
}

void handleDownloadCSV() {
    File file = SPIFFS.open(LOG_FILE, FILE_READ);
    if (!file) {
        server.send(404, "text/plain", "No log available.");
        return;
    }

    server.streamFile(file, "text/csv");
    file.close();
}

void handleClearLog() {
    SPIFFS.remove(LOG_FILE);
    pressureIndex = 0;
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleSetMessage() {
    if (server.hasArg("message")) {
        customMessage = server.arg("message");
        lcd.setCursor(0, 3);
        lcd.print("                    ");  // clear line
        lcd.setCursor(0, 3);
        lcd.print(customMessage);
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleSetTime() {
    if (server.hasArg("date") && server.hasArg("time")) {
        String dateStr = server.arg("date");  // Format: "YYYY-MM-DD"
        String timeStr = server.arg("time");  // Format: "HH:MM"

        int year = dateStr.substring(0, 4).toInt();
        int month = dateStr.substring(5, 7).toInt();
        int day = dateStr.substring(8, 10).toInt();

        int hour = timeStr.substring(0, 2).toInt();
        int minute = timeStr.substring(3, 5).toInt();

        rtc.adjust(DateTime(year, month, day, hour, minute, 0));
        lcd.setCursor(0, 3);
        lcd.print("Fecha/Hora actual.");
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

// === Pressure Sensor ===
#define SENSOR_PIN 36  // GPIO36
#define CALIBRATION_SAMPLES 30
float OFFSET = 0.6116746902;
const float PSI_CONV = 0.0001450377;

void do_calibration(float knownPressurePsi) {
    int value = 0;
    for (size_t i = 0; i < CALIBRATION_SAMPLES; i++) {
        value += analogRead(SENSOR_PIN);
        delay(1000);
    }
    float averageValue = ((float)value / CALIBRATION_SAMPLES) * 3.3 / 4095.0;
    float knownPressurePa = knownPressurePsi / PSI_CONV;
    OFFSET = averageValue - (knownPressurePa / 300000.0);
    Serial.print("cal: ");
    Serial.println(OFFSET, 10);
}

float read_pressure_psi() {
    int raw = analogRead(SENSOR_PIN);
    float voltage = raw * 3.3 / 4095.0;
    float pressurePa = (voltage - OFFSET) * 300000.0;
    float pressurePsi = pressurePa * PSI_CONV;
    return pressurePsi;
}

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);

    lcd.begin(20, 4);
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        lcd.setCursor(0, 1);
        lcd.print("SPIFFS Error!");
    } else {
        loadLogFromSPIFFS();
    }
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("AP Starting...");

    if (!rtc.begin()) {
        lcd.setCursor(0, 1);
        lcd.print("RTC ERROR");
        while (1);
    }

    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    lcd.setCursor(0, 1);
    lcd.print("SSID: ");
    lcd.print(ssid);

    lcd.setCursor(0, 3);
    lcd.print(customMessage);

    server.on("/", handleRoot);
    server.on("/set-time", handleSetTime);
    server.on("/set-message", handleSetMessage);
    server.on("/download", handleDownloadCSV);
    server.on("/clear-log", handleClearLog);
    server.begin();
}

void loop() {
    server.handleClient();

    // Update time and date every second
    static unsigned long lastTimeUpdate = 0;
    if (millis() - lastTimeUpdate > 1000) {
        lastTimeUpdate = millis();
        DateTime now = rtc.now();

        char timeBuf[20];
        sprintf(timeBuf, "%02d/%02d/%02d %02d:%02d:%02d",
                now.day(), now.month(), now.year() % 100,
                now.hour(), now.minute(), now.second());

        lcd.setCursor(0, 0);
        lcd.print(timeBuf);
    }

    // Update pressure every second
    static unsigned long lastPressureUpdate = 0;
    if (millis() - lastPressureUpdate > 1000) {
        lastPressureUpdate = millis();

        float pressure = read_pressure_psi();

        lcd.setCursor(0, 2);
        lcd.print("                    ");  // clear line
        lcd.setCursor(0, 2);
        lcd.print("P: ");
        lcd.print(pressure, 2);
        lcd.print(" PSI");
    }

    static unsigned long lastPressureSave = 0;
    if (millis() - lastPressureSave > 600000) {  // 600000 ms = 10 minutes
        lastPressureSave = millis();

        if (pressureIndex < MAX_PRESSURE_ENTRIES) {
            pressureLog[pressureIndex].pressure = read_pressure_psi();
            pressureLog[pressureIndex].timestamp = rtc.now();
            pressureIndex++;
        } else {
            // Optional: shift left to keep the newest 144 entries
            for (int i = 1; i < MAX_PRESSURE_ENTRIES; i++) {
                pressureLog[i - 1] = pressureLog[i];
            }
            pressureLog[MAX_PRESSURE_ENTRIES - 1].pressure = read_pressure_psi();
            pressureLog[MAX_PRESSURE_ENTRIES - 1].timestamp = rtc.now();
        }

        saveLogToSPIFFS();
    }
}
