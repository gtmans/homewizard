/*
 *  M5-Atom-IR-remote 2024 Mans Rademaker
 *  
 *  2DO
 *  - use button 4 something
 *  - some button show status of all sockets with blinking led
 *  
 *  based on: 
 *  - SimpleReceiver.cpp                                        (https://github.com/Arduino-IRremote/Arduino-IRremote)
 *  - M5-Atom-light + M5 IR-remote module + cheap IR-pad like   (https://www.tinytronics.nl/nl/communicatie-en-signalen/draadloos/infrarood/ir-sensor-module-met-afstandsbediening-en-batterij)
 *
 * press 1,2 or 3 to turn socket 1,2, or 3 on
 * press 4,5 or 6 to turn socket 1,2, or 3 off
 * press 0        to turn all sockets off
 * press vol- or vol+ to change volume on your Sony TV
 * 
 * use simplereceiver to discover IR codes to use                (https://github.com/Arduino-IRremote/Arduino-IRremote/blob/master/examples/SimpleReceiver/SimpleReceiver.ino)
 */
#include  <Arduino.h>
#include  <IRremote.hpp>          // include the library
#define   DECODE_NEC              // Includes Apple and Onkyo. To enable all protocols , just comment/disable this line.
#define   IR_RECEIVE_PIN      32  //grove pin
#define   IR_SEND_PIN         26  //grove pin

// cheap remote with NEC protocol(see example link in header)
#define CHM  0x45
#define CH   0x46
#define CHP  0x47
#define REV  0x44
#define FWD  0x40
#define PLA  0x43
#define VOM  0x07
#define VOP  0x15
#define ZERO 0x16
#define HUN  0x19
#define HUN2 0x0D
#define ONE  0x0C
#define TWO  0x18
#define THR  0x5E
#define FOU  0x08
#define FIV  0x1C
#define SIX  0x5A
#define SEV  0x42
#define EIG  0x52
#define NIN  0x4A
int lastcmd;
// Atom
#include "M5Atom.h"
int black       =     0x000000;
int red         =     0xff0000;
int green       =     0x00ff00;
int blue        =     0x0000ff;

//sockets
#define nrdev    3
String ipBase         = "192.168.1.";      //your socket IP base (FF.FF.FF.00)
String ipnames[nrdev] = {"droger","kachel1","kachel2"};
String ipSup  [nrdev] = {"23"   ,"21"     ,"22"   };
String PowerON        = "{\"power_on\": true}";
String PowerOFF       = "{\"power_on\": false}";
String putData;
String ipDevice;

//wifi
#include "my_secrets.h"
/* myn_secrets.h:  wifi passwords and IP adresses or fill in below values
const char* ssid    = "mySSID";        
const char* password= "myWIFIpassword";*/

#include              <Arduino.h>
#include              <WiFi.h>
#include              <WiFiMulti.h>
#include              <HTTPClient.h> 
#include              <ArduinoJson.h>
StaticJsonDocument    <1000> doc;

WiFiMulti wifiMulti;
HTTPClient http;
bool                  error;
int                   httpCode;
String                payload;

void setup() {
    M5.begin          (true, false, true);// Initialiseren van Atom-Matrix (seriële poort, LED).
    delay             (50);
    ConnectAtom       ();                 // connect 2 wifi
    IrReceiver.begin  (IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
    M5.dis.fillpix    (black);
}

void loop() {
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol != NEC||IrReceiver.decodedIRData.command==lastcmd) {//do not allow same button twice
//  if (IrReceiver.decodedIRData.protocol != NEC) {//allow same button twice
       IrReceiver.resume(); // Do it here, to preserve raw data for printing with printIRResultRawFormatted()
    } else {
      IrReceiver.resume(); // Early enable receiving of the next IR frame
      switch (IrReceiver.decodedIRData.command) {
        case VOM:
        Serial.println      ("VOL-  ontvangen!");
        IrSender.sendSony   (0x1, 0x13, 2, 12);     // use your own codes fot TV VOL-
        break;
      case VOP:
        Serial.println      ("VOL+  ontvangen!");   // use your own codes fot TV VOL+
        IrSender.sendSony   (0x1, 0x12, 2, 12);
        break;
      case ZERO:
        Serial.println      ("knop0 ontvangen!");
        for (int d=0;d<nrdev;d++){
        Serial.print        ("turning "+ipnames[d]+" OFF");
        PutSocketStat       (d,false);
        }
        break;
      case ONE:
        Serial.println      ("knop1 ontvangen!");
        Serial.print        ("turning "+ipnames[0]+" ON");
        PutSocketStat       (0,true);
        break;
      case TWO:
        Serial.println      ("knop2 ontvangen!");
        Serial.print        ("turning "+ipnames[1]+" ON");
        PutSocketStat       (1,true);
        break;
      case THR:
        Serial.println      ("knop3 ontvangen!");
        Serial.print        ("turning "+ipnames[2]+" ON");
        PutSocketStat       (2,true);
        break;
      case FOU:
        Serial.println      ("knop4 ontvangen!");
        Serial.print        ("turning "+ipnames[0]+" OFF");
        PutSocketStat       (0,false);
        break;
      case FIV:
        Serial.println      ("knop5 ontvangen!");
        Serial.print        ("turning "+ipnames[1]+" OFF");
        PutSocketStat       (1,false);
        break;
      case SIX:
        Serial.println      ("knop6 ontvangen!");
        Serial.print        ("turning "+ipnames[2]+" OFF");
        PutSocketStat       (2,false);
        break;
      default:
        Serial.println      ("NEC ontvangen:");
        Serial.println      (IrReceiver.decodedIRData.command,HEX);
      break;
      }
    } // end of protocol == NEC
  }   // end of IrReceiver.decode()
  lastcmd=IrReceiver.decodedIRData.command;
}

void PutSocketStat      (int DevNr, bool ON){ 
  ipDevice              = ipBase  + ipSup[DevNr];
  Serial.println        (">>>>> PutSocketStat()");
  Serial.print          (ipnames[DevNr]);
  Serial.print          ("(");
  Serial.print          (ipDevice);
  Serial.println        (")");  
  error       =         false;
  if (ON) {
    putData   =         PowerON;
    M5.dis.drawpix      (0,red);
  } else {
    putData   =         PowerOFF;
    M5.dis.drawpix      (0,green);
  }
  if ((wifiMulti.run    () == WL_CONNECTED)) {                    // wait for WiFi connection.
    HTTPClient http;
    http.begin          ("http://" + ipDevice + "/api/v1/state");   
    http.addHeader      ("Content-Type", "application/json");
    http.addHeader      ("Content-Length", String(putData.length()));
    httpCode = http.PUT (putData);
    if (httpCode > 0)   {
      Serial.print      ("HTTP Response code: ");
      Serial.println    (httpCode);
      if (httpCode ==   HTTP_CODE_OK) {
        String payload =http.getString();
        Serial.println  ("Response payload:");
        Serial.println  (payload);
      }
    } else {
    Serial.print        ("Error code: ");
    Serial.println      (httpCode);
    error = true;
    }
    http.end();
  }
  if(error)             {M5.dis.drawpix(0,blue);}
}

void ConnectAtom(){
  M5.dis.drawpix        (0,blue);
  Serial.println        (">>>>> ConnectAtom()");
  wifiMulti.addAP       (ssid,password);            // Storage wifi configuration
  Serial.print          ("\nConnecting Wifi...\n"); // Serial port format output
  while                 (wifiMulti.run()!=WL_CONNECTED){M5.dis.drawpix(0,black);delay(250);Serial.print(".");M5.dis.drawpix(0,blue);delay (250);}
  Serial.println        ();
  M5.dis.drawpix        (0,black);  // BLACK
}
