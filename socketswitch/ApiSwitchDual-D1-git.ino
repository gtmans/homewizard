/*
 * Aansturen Homewizard Energy Sockets dmv. api 
 * Mans Rademaker 2024 ism. ChatGPT
 * 
 * hier een draadloze remote voor 2 sockets met gekleurde leds op basis van 
 * - D1 mini
 * - stukje Neopixel (2 pixels)
 * - 2 pushbuttons
 * - weerstand 470k (300-500k)
 * - condensator 1000 uF
 * - Arduino IDE omgeving
 * - benodigde bibliotheken (zie code)
 */

#include    "gewoon_secrets.h"  // or fill in your values below
//const     char* ssid     =    "myssid";        
//const     char* password =    "mypass";
//String    Devices[2]={"192.168.2.48","192.168.2.61"};

#include    <ESP8266WiFi.h>
#include    <ESP8266HTTPClient.h>
#include    <ArduinoJson.h>
WiFiClient  client;
StaticJsonDocument<200> doc;    // JSON-document om de reactie te parseren
#include    <Adafruit_NeoPixel.h>
#define     LED_PIN   D1
#define     LED_COUNT 2
#define     button1   D5 
#define     button2   D6 
Adafruit_NeoPixel     pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

bool        PowerIsOn,switchLock,buttonpressed,error;
int         brightness;
String      PowerON  = "{\"power_on\": true}";
String      PowerOFF = "{\"power_on\": false}";
String      putData,ipAddress;

unsigned long       startMillis;
unsigned long       currentMillis;
const unsigned long period = 15000;// test every 15 seconds

void setup() {
  Serial.begin    (115200);
  while           (!Serial);
  Serial.println  ();
  Serial.println  (__FILE__);//name of this doc
  pinMode         (LED_BUILTIN, OUTPUT);
  pinMode         (button1, INPUT);
  pinMode         (button2, INPUT); 
  WifiConnect     ();

  pixels.begin    ();       // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.show     ();       // Turn OFF all pixels ASAP
  pixels.setBrightness(15); // Set BRIGHTNESS to about 1/5 (max = 255)

  // set initial state leds
  GetSocketStat   (0);  
  GetSocketStat   (1);
  startMillis  =  millis();  //initial start time
}

void loop() {
  currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  buttonpressed = digitalRead(button1);
  if (buttonpressed){
    Serial.println    (">>Get Socket 0");
    GetSocketStat     (0);
    PowerIsOn=!PowerIsOn;
    PutSocketStat     (0,PowerIsOn);
  }
  buttonpressed =   digitalRead(button2);
  if (buttonpressed){
    Serial.println    (">>Get Socket 1");
    GetSocketStat     (1);
    PowerIsOn=!PowerIsOn;
    PutSocketStat     (1,PowerIsOn);
  }

  if (currentMillis - startMillis >= period) { //test whether the period has elapsed
    Serial.println    ();
    Serial.println    ("Checking stats!");
    GetSocketStat     (0);
    GetSocketStat     (1);    
    startMillis =     currentMillis; 
  } else              {delay(100);}
}

void PutSocketStat(int socknum,bool ON){ 
  ipAddress =         Devices[socknum];  
  Serial.print        ("PutSocketStat-");
  Serial.print        (ipAddress);
  Serial.print        ("-");
  Serial.println      (ON);
  error=false;
  if (ON)             {putData=PowerON;}else{putData=PowerOFF;}
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Start HTTP PUT request
    http.begin        (client, "http://" + String(ipAddress) + "/api/v1/state");
    http.addHeader    ("Content-Type", "application/json");    
    // Set content length
    http.addHeader    ("Content-Length", String(putData.length()));
    // Send the PUT request and handle the response
    int httpResponseCode = http.PUT(putData);
    if (httpResponseCode > 0) {
      Serial.print    ("HTTP Response code: ");
      Serial.println  (httpResponseCode);
      if (httpResponseCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("Response payload:");
        Serial.println(payload);
      }
    } else { //device offline == -1
      Serial.print    ("Error code: ");
      Serial.println  (httpResponseCode);
      error=true;
    }
    // End HTTP connection
    http.end();
  }
    if(error){SetLeds(socknum,"blue");}else{if(PowerIsOn==true){SetLeds(socknum,"red");}else{SetLeds(socknum,"green");}}
}

void GetSocketStat(int socknum){   
  ipAddress =         Devices[socknum];
  error=false;
  Serial.print        ("GetSocketStat-");
  Serial.println      (ipAddress);
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Start HTTP GET request
    http.begin(client, "http://" + ipAddress + "/api/v1/state");    
    // Send the GET request and handle the response
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.print    ("HTTP Response code: ");
      Serial.println  (httpResponseCode);
      if              (httpResponseCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("Response payload:");
        Serial.println(payload);       
        // Parse JSON response
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.f_str());
        } else {
          // Extract values from JSON response
          PowerIsOn   = doc["power_on"];
          switchLock  = doc["switch_lock"];
          brightness  = doc["brightness"];
          // Do something with the extracted values
          Serial.println("power_on: " + String(PowerIsOn));
          Serial.println("switch_lock: " + String(switchLock));
          Serial.println("brightness: " + String(brightness));
        }
      }
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      error=true;
    }    
    // End HTTP connection
    http.end();
  }  
  if(error){SetLeds(socknum,"blue");}else{if(PowerIsOn==true){SetLeds(socknum,"red");}else{SetLeds(socknum,"green");}}
}

void SetLeds(int lednr,String ledclr)   {
  if (ledclr=="green"){pixels.setPixelColor  (lednr, pixels.Color(0,255,0));}
  if (ledclr=="red")  {pixels.setPixelColor  (lednr, pixels.Color(255,0,0));}
  if (ledclr=="blue") {pixels.setPixelColor  (lednr, pixels.Color(0,0,255));}
  if (ledclr=="none") {pixels.setPixelColor  (lednr, pixels.Color(0,0,0));}
  pixels.show         ();   // Send the updated pixel colors to the hardware.
}

void WifiConnect() {
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.println(ssid);
        while (WiFi.status() != WL_CONNECTED) {
          digitalWrite(LED_BUILTIN, LOW);
          delay       (500);
          digitalWrite(LED_BUILTIN, HIGH);
          delay       (500);
          Serial.print(".");
        }
  Serial.println("");
  Serial.println("WiFi connected");
}
