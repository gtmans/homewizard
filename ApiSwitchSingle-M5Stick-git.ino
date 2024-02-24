/*  
 * Aansturen Homewizard Energy Sockets dmv. api 
 * Mans Rademaker 2024 ism. ChatGPT voor M5StickCPlus
 * 
 * hier een draadloze remote voor 1 socket op basis van 
 * - M5StickCPlus
 * - Arduino IDE omgeving
 * - benodigde bibliotheken (zie code)
 */
#include <M5StickCPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "yourSSID";
const char* password = "yourPass";
String ipDevice = "192.168.2.70";//your socket IP

WiFiClient client;
StaticJsonDocument<200> doc;

bool PowerIsOn, switchLock, buttonpressed, error;
int brightness;
String PowerON = "{\"power_on\": true}";
String PowerOFF = "{\"power_on\": false}";
String putData;

unsigned long startMillis;
unsigned long currentMillis;
const unsigned long period = 15000;

void setup() {
  M5.begin();
  Serial.begin(115200);
  Serial.println();
  Serial.println(__FILE__);
  pinMode(10, OUTPUT); // GPIO 10 as LED pin
  pinMode(37, INPUT); // GPIO 37 as button pin

  M5.Lcd.fillScreen(BLACK); // Clear screen
  M5.Lcd.setRotation(3); // Rotate screen for M5StickCPlus
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(4);
  M5.Lcd.setCursor(60, 60); // Aanpassen aan de juiste positie
  M5.Lcd.print("INIT");

  WiFiConnect();

  startMillis = millis();
  GetSocketStat(0, ipDevice);
}

void loop() {
  currentMillis = millis();
  
  // Button A pressed
  if (digitalRead(37) == LOW && !buttonpressed) {
    Serial.println(">>Get Device");
    GetSocketStat(0, ipDevice);
    PowerIsOn = !PowerIsOn;
    PutSocketStat(0, ipDevice, PowerIsOn);
    buttonpressed = true; // Set buttonpressed flag
  }
  
  // Button A released
  if (digitalRead(37) == HIGH && buttonpressed) {
    buttonpressed = false; // Reset buttonpressed flag
  }

  if (currentMillis - startMillis >= period) {
    Serial.println();
    Serial.println("Checking stats!");
    GetSocketStat(0, ipDevice);
    startMillis = currentMillis;
  } else {
    delay(100);
  }
}

void PutSocketStat(int socknum, String ipAddress, bool ON) {
  Serial.print("PutSocketStat-");
  Serial.print(ipAddress);
  Serial.print("-");
  Serial.println(ON);
  error = false;

  if (ON) {
    putData = PowerON;
  } else {
    putData = PowerOFF;
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(client, "http://" + ipAddress + "/api/v1/state");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Content-Length", String(putData.length()));

    int httpResponseCode = http.PUT(putData);
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      if (httpResponseCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("Response payload:");
        Serial.println(payload);
      }
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      error = true;
    }

    http.end();
  }

  if (error) {
    SetScreenColor("blue");
  } else {
    if (PowerIsOn) {
      SetScreenColor("red");
    } else {
      SetScreenColor("green");
    }
  }
}

void GetSocketStat(int socknum, String ipAddress) {
  error = false;

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(client, "http://" + ipAddress + "/api/v1/state");

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      if (httpResponseCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("Response payload:");
        Serial.println(payload);

        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
          PowerIsOn = doc["power_on"];
          switchLock = doc["switch_lock"];
          brightness = doc["brightness"];

          Serial.println("power_on: " + String(PowerIsOn));
          Serial.println("switch_lock: " + String(switchLock));
          Serial.println("brightness: " + String(brightness));
        } else {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
        }
      }
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      error = true;
    }

    http.end();
  }

  if (error) {
    SetScreenColor("blue");
  } else {
    if (PowerIsOn) {
      SetScreenColor("red");
    } else {
      SetScreenColor("green");
    }
  }
}

void SetScreenColor(String color) {
  if (color == "green") {
    M5.Lcd.fillScreen(GREEN);
  } else if (color == "red") {
    M5.Lcd.fillScreen(RED);
  } else if (color == "blue") {
    M5.Lcd.fillScreen(BLUE);
  } else {
    M5.Lcd.fillScreen(BLACK);
  }  
  M5.Lcd.setTextColor(WHITE);
  String AanUit;
  if (color == "green"){M5.Lcd.setTextColor(BLACK);AanUit="UIT";} else {M5.Lcd.setTextColor(WHITE);AanUit="AAN";}
  if (color == "blue") {M5.Lcd.setTextColor(BLACK);AanUit="ONBEKEND";}
  M5.Lcd.setTextSize(4);

  // Plaats de tekst "DROGER"
  int x_droog = (M5.Lcd.width() - M5.Lcd.textWidth("DROOG")) / 2;
  M5.Lcd.setCursor(x_droog, 40);
  M5.Lcd.print("DROGER");

  // Plaats de tekst "AAN"
  int x_aan = (M5.Lcd.width() - M5.Lcd.textWidth(AanUit)) / 2;
  M5.Lcd.setCursor(x_aan, 80);
  M5.Lcd.print(AanUit);
}

void WiFiConnect() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(10, LOW); // GPIO 10 LED indicator
    delay(500);
    digitalWrite(10, HIGH);
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
}
