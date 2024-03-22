/*  
 * Aansturen Homewizard Energy Sockets dmv. api 
 * Mans Rademaker 2024 ism. ChatGPT voor M5StickCPlus
 * 
 * hier een draadloze remote voor meerdere socket op basis van 
 * - M5StickCPlus
 * - Arduino IDE omgeving
 * - benodigde bibliotheken (zie code)
 * 
 * You can turn wake up on motion (WOM) on and that works but battery dies within a day or two
 */
#include <M5StickCPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

//#define WOM 1

#ifdef WOM
  //wake up on motion
  #include <driver/rtc_io.h>  // from ESP-IDF
  int sleepsec = 30;
  void mpu6886_wake_on_motion_isr(void);  // declaration of ISR

  #define WOM_ATTACH_ISR
  volatile uint32_t g_wom_count       = 0;
  volatile uint32_t g_wom_last_millis = 0;
  void IRAM_ATTR mpu6886_wake_on_motion_isr(void) {
      g_wom_count++;
     g_wom_last_millis = millis();
  }
  /* Method to print the reason by which ESP32 has been awoken from sleep */
  void get_wakeup_reason_string(char *cbuf, int cbuf_len) {
      esp_sleep_wakeup_cause_t wakeup_reason;
      wakeup_reason = esp_sleep_get_wakeup_cause();
      switch (wakeup_reason) {
          case ESP_SLEEP_WAKEUP_EXT0:
              snprintf(cbuf, cbuf_len, "ext0");
              break;
          case ESP_SLEEP_WAKEUP_EXT1:
              snprintf(cbuf, cbuf_len, "ext1");
              break;
          case ESP_SLEEP_WAKEUP_TIMER:
              snprintf(cbuf, cbuf_len, "timer");
              break;
          case ESP_SLEEP_WAKEUP_TOUCHPAD:
              snprintf(cbuf, cbuf_len, "touchpad");
              break;
          case ESP_SLEEP_WAKEUP_ULP:
              snprintf(cbuf, cbuf_len, "ULP");
              break;
          default:
              snprintf(cbuf, cbuf_len, "%d", wakeup_reason);
              break;
      }
  }
  RTC_DATA_ATTR int bootCount = 0;
  #define WAKE_REASON_BUF_LEN 100
#endif


//WIFI configuration
#include "gewoon_secrets.h" //or fill in values below
/* gewoon_secrets.h: wifi passwords
const char* ssid     = "mySSID";        
const char* password = "myWIFIpassword";
String ipBase       = "192.168.2.";      //your socket IP base (FF.FF.FF.00)
String ipnames  [4] = {"dryer","washing","airco","heater"};
String ipSup    [4] = {"20","21","24","43"};
*/

WiFiClient client;
StaticJsonDocument<200> doc;

bool PowerIsOn, switchLock, buttonApressed, buttonBpressed, error;
int brightness;
String PowerON = "{\"power_on\": true}";
String PowerOFF = "{\"power_on\": false}";
String putData;

unsigned long startMillis;
unsigned long currentMillis;
const unsigned long period = 15000;

//https://docs.m5stack.com/en/core/m5stickc_plus
#define buttonA 37
#define buttonB 39

void setup() {  
  M5.begin();
  Serial.begin(115200);
  Serial.println();
  Serial.println(__FILE__);
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);

#ifdef WOM 
  char wake_reason_buf[WAKE_REASON_BUF_LEN];
  get_wakeup_reason_string(wake_reason_buf, WAKE_REASON_BUF_LEN);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("WOM:BOOT=%d,SRC=%s", bootCount, wake_reason_buf);
  Serial.printf("WOM:BOOT=%d,SRC=%s\n", bootCount, wake_reason_buf);
  M5.Lcd.setCursor(0, 20);
  M5.Lcd.printf("Battery: ");
  M5.Lcd.setCursor(0, 40);
  M5.Lcd.printf("V: %.3f v", M5.Axp.GetBatVoltage());
  M5.Lcd.setCursor(0, 60);
  M5.Lcd.printf("I: %.3f ma", M5.Axp.GetBatCurrent());
  M5.Lcd.setCursor(0, 80);
  M5.Lcd.printf("P: %.3f mw", M5.Axp.GetBatPower());
  #ifdef WOM_GPIO_DEBUG_TEST
    pinMode(GPIO_NUM_26, OUTPUT);
    pinMode(GPIO_NUM_36, INPUT);
  #endif  // #ifdef WOM_GPIO_DEBUG_TEST
  #ifdef WOM_ATTACH_ISR
    // set up ISR to trigger on GPIO35
    delay(100);
    pinMode(GPIO_NUM_35, INPUT);
    delay(100);
    attachInterrupt(GPIO_NUM_35, mpu6886_wake_on_motion_isr, FALLING);
  #endif  // #ifdef WOM_ATTACH_ISR
  // Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  // set up mpu6886 for low-power operation
  M5.Imu.Init();  // basic init
  M5.Imu.enableWakeOnMotion(M5.Imu.AFS_16G, 10); 
#endif

  pinMode(10, OUTPUT);     // GPIO 10 as LED pin
  pinMode(buttonA, INPUT); // GPIO 37 as buttonA pin
  pinMode(buttonB, INPUT); // GPIO 39 as buttonB pin

  M5.Lcd.fillScreen(BLACK); // Clear screen
  M5.Lcd.setRotation(3); // Rotate screen for M5StickCPlus
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(4);
  M5.Lcd.setCursor(60, 60); // Aanpassen aan de juiste positie
  M5.Lcd.print("INIT");

  WiFiConnect();

  startMillis = millis();
  GetSocketStat(DeviceNr);
}

void loop() {
  currentMillis = millis();
  
  // Button A pressed
  if (digitalRead(buttonA) == LOW && !buttonApressed) {
    Serial.println(">>Get Device");
    GetSocketStat(DeviceNr);
    PowerIsOn = !PowerIsOn;
    PutSocketStat(DeviceNr, PowerIsOn);
    buttonApressed = true; // Set buttonApressed flag
  }  
  // Button A released
  if (digitalRead(buttonA) == HIGH && buttonApressed) {
    buttonApressed = false; // Reset buttonApressed flag
  }

  // Button B pressed
  if (digitalRead(buttonB) == LOW && !buttonBpressed) {
    Serial.print  (">>Change Device from ");
    Serial.print  (DeviceNr);
    Serial.print  (" to ");
    DeviceNr++;if (DeviceNr>3){DeviceNr=0;}
    Serial.println(DeviceNr);
    GetSocketStat (DeviceNr);
  }  
  // Button B released
  if (digitalRead(buttonB) == HIGH && buttonBpressed) {
    buttonBpressed = false; // Reset buttonBpressed flag
  }

  if (currentMillis - startMillis >= period) {
    #ifdef WOM
      uint32_t since_last_wom_millis = millis() - g_wom_last_millis;
      Serial.println("No movement for" +String(since_last_wom_millis/1000)+" seconds");
      if (since_last_wom_millis > sleepsec*1000){go2sleep();}
    #endif
    Serial.println();
    Serial.println("Checking stats!");
    GetSocketStat(DeviceNr);
    startMillis = currentMillis;
  } else {
    delay(100);
  }

}

void PutSocketStat(int DevNr, bool ON) {
  ipDevice=ipBase+ipSup[DevNr];
  Serial.print(">>>>PutSocketStat-");
  Serial.print(ipnames[DevNr]);
  Serial.print("(");
  Serial.print(ipDevice);
  Serial.print(")-");
  Serial.println(ON);
  error = false;
  if (ON) {
    putData = PowerON;
  } else {
    putData = PowerOFF;
  }
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(client, "http://" + ipDevice + "/api/v1/state");
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

//void GetSocketStat(int socknum, String ipAddress) {
// GetSocketStat(DeviceNr);

void GetSocketStat(int DevNr) {
  ipDevice=ipBase+ipSup[DevNr];
  Serial.print  (">>>>>GetSocketStat-");
  Serial.print  (ipnames[DevNr]);
  Serial.print  ("(");
  Serial.print  (ipDevice);
  Serial.println(")");  
  error = false;
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(client, "http://" + ipDevice + "/api/v1/state");
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
          Serial.println("ipDevice: " + ipDevice);
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
  M5.Lcd.drawRect(10, 10   , 240-20, 135-20, BLACK);
//M5.Lcd.drawLine(40, 135/2, 240-40, 135/2 , BLACK);
  M5.Lcd.drawRect(40, 134/2, 240-80, 3 , BLACK);

  String AanUit=String(DeviceNr)+" ";
/*  
  if (color == "green"){M5.Lcd.setTextColor(BLACK);AanUit+="UIT";} else {M5.Lcd.setTextColor(WHITE);AanUit+="AAN";}
  if (color == "blue") {M5.Lcd.setTextColor(BLACK);AanUit+="ONBEKEND";}
*/
  if (color == "green"){M5.Lcd.setTextColor(BLACK);AanUit+="UIT";}
  if (color == "blue") {M5.Lcd.setTextColor(BLACK);AanUit ="ONBEKEND";}
  if (color == "red")  {M5.Lcd.setTextColor(WHITE);AanUit+="AAN";M5.Lcd.drawRect(10,10,240-20,135-20,WHITE);M5.Lcd.drawRect(40,134/2,240-80,3,WHITE);}

  M5.Lcd.setTextSize(3);
//M5.Lcd.setFont(&fonts::Orbitron_Light_24);
//M5.Lcd.setTextSize(1);
  
  // Plaats de tekst "DROGER"
  int x_droog = (M5.Lcd.width() - M5.Lcd.textWidth(ipnames[DeviceNr])) / 2;
  M5.Lcd.setCursor(x_droog, 40);
  M5.Lcd.print(ipnames[DeviceNr]);
//M5.Lcd.setTextDatum(top_center);
//M5.Lcd.drawString((ipnames[DeviceNr]);

  // Plaats de tekst "AAN"
  int x_aan = (M5.Lcd.width() - M5.Lcd.textWidth(AanUit)) / 2;
  M5.Lcd.setTextSize(4);
  M5.Lcd.setCursor(x_aan, 80);
  M5.Lcd.print(AanUit);
//M5.Lcd.setTextDatum(bottom_center);
//M5.Lcd.drawString(AanUit);

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
#ifdef WOM
  void go2sleep(){
    // disable all wakeup sources
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    // enable waking up on pin 35 (from IMU)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, LOW);  // 1 = High, 0 = Low

    // Go to sleep now
    Serial.printf("\nGoing to sleep now\n");
    M5.Axp.SetSleep();  // conveniently turn off screen, etc.
    delay(100);
    esp_deep_sleep_start();
    Serial.printf("\nThis will never be printed");
  }
#endif
