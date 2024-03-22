/* public https://github.com/gtmans/homewizard/blob/main/autosocketswitch/M5MatrixAutoSocketSwitch-git.ino private version M5MatrixAutoSocketSwitchv9.6 
 *  
 * (c) Mans Rademaker march 2024
 * 
 * measures 4 sockets, displays their consuption on a 5x5 matrix (M5Atom) and total
 * when total > 3600 it switches off 1 specific socket
 * consumption <240w no led, 240-480 2 leds, 480-720 2 leds, 720-960 3 leds, 960-12 4 leds, >1200 5 leds
 * socket     1200,960,720,480,240
 * total      3600-3500-3000-2500->2000 watt
 * 
 * matrix (example)
 * socket[0]  * * * *
 * socket[1]  * * *
 * socket[2]  * * 
 * socket[3]  * * * *
 * total      * * * * * *
 * 
 * matrix (translation table hor[] to vert[])
 *     vert[]            hor[]
 *  0  1  2  3  4   _ 20 15 10  5  0 
 *  5  6  7  8  9   U 21 16 11  6  1
 * 10 11 12 13 14   S 22 17 12  7  2
 * 15 16 17 18 19   B 23 18 13  8  3
 * 20 21 22 23 24     24 19 14  9  4
 *     |USB|
 */

#include "M5Atom.h"
bool deb=false;   //debug flag

//prowl
#define prowl 1
#ifdef  prowl
  #include            <EspProwl.h> 
  char  ProwlAPN[]    = "AutoSocketswitch";
  char  ProwlLoc[]    = "ERROR!";   
  bool  prowlsent[4]  = {false,false,false,false};
#endif

//matrix
int kleur;
int black       =     0x000000;
int red         =     0xff0000;
int green       =     0x00ff00;
int orange      =     0xfcb900;
int blue        =     0x0000ff;
int yellow      =     0xfff000;
int pink        =     0xff00ff;
int white       =     0xffffff;
int rescale;
int Sock[5];
int Demo[5]     =     {4,5,2,5,4};
int pixel,ercol;
int vert[25]    =     {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24};//pixelnr bottom=usb
int hor [25]    =     {20,15,10,5,0,21,16,11,6,1,22,17,12,7,2,23,18,13,8,3,24,19,14,9,4};//pixelnr   left=usb
int funpos;
//int fun [8]   =     {12, 8,12,16,12, 6,12,18};
int fun [8]     =     {12,12,12,12,12,12,12,12};
int flip;
int waitmsec    =     15000;          //msec to wait between polls

//wifi
#include              <M5Atom.h>
#include              <Arduino.h>
#include              <WiFi.h>
#include              <WiFiMulti.h>
#include              <HTTPClient.h>  //https://github.com/espressif/arduino-esp32/blob/master/libraries/HTTPClient/src/HTTPClient.h
#include              <ArduinoJson.h>
StaticJsonDocument    <1000> doc;

WiFiMulti wifiMulti;
HTTPClient http;

#include "gewoon_secrets.h"
/* gewoon_secrets.h:  wifi passwords and IP adresses or fill in below values
const char* ssid    = "mySSID";        
const char* password= "myWIFIpassword";
String ipBase       = "192.168.1.";      //your socket IP base (FF.FF.FF.00)
String ipnames  [4] = {"airco     ","radiator  ","heater    ","boiler    "};
String ipSup    [4] = {"050"       ,"051"       ,"100"       ,"101"};
char ProwlapiKey[51] ="aede009a8ae7a8e78e6a6ada8da8e6da8dadaec0";
*/

//sockets
int   turn_off_sock = 1;                           // turn off device 1 (radiator) if nessesairy
bool  auto_turn_on  = false;                       //(auto turn on sock when possible again)
int   min_power     = 2400;                        // min watts when to turn on
int   max_power     = 3600;                        // max watts when to turn off
int   dummy_power   = 999;      
int   boundries[5]  = {240,480,720,960,1200};      // FillMatrix boundry values for leds to turn on
//int totbounds[5]  = { 500,1000,1500,2000,2500};  // FillTotal  boundry values for leds to turn on
//int totbounds[5]  = {2000,2500,3000,3500,3600};  // FillTotal  boundry values for leds to turn on
int   totbounds[5]  = {1000,2000,3000,3500,3600};  // FillTotal  boundry values for leds to turn on
bool                  stopped,error;
String                payload,errortxt,ipDevice;
int                   sstrength,httpCode,DeviceNr;
float                 spower,tspower,tspowerold,spowerc;  // socket power, socket total power, socket total power old, socket power consumption
float                 sock_usage[5],sock_last_usage[5],tot_usage;
String                sock_status[5];
String  PowerON =     "{\"power_on\": true}";
String  PowerOFF =    "{\"power_on\": false}";
String  putData;

//various
bool                  do_blink,my_error,no_data,no_temp,no_log,missed,all_off;
unsigned long         waitingtime = millis();

void setup() {
    M5.begin          (true, false, true);// Initialiseren van Atom-Matrix (seriële poort, LED).
    delay             (50);
    ConnectAtom       ();                 // connect 2 wifi
    //TestHTTPAtom    ();                 // test HTTP
    //DemoMatrix      ();                 // display demo bars
    //delay           (5000);
    M5.dis.fillpix    (black);
    #ifdef  prowl
      InitProwl();
    #endif
}

void loop() {
    Serial.print      ("Collecting data ");
    missed    =       false;   
    DeviceNr  =       0;
    tot_usage =       0;
    all_off   =       true;
    M5.dis.drawpix    (fun[funpos],black);    
    funpos++;if       (funpos==8){funpos=0;}
    for (int s=0;s<4;s++){
      GetSocketDataAtom2(s);
      tot_usage+=sock_usage[s];
      if (sock_usage[s]>=240){all_off=false;};
      FillMatrix      (sock_usage[s],s);
      if(deb){
        Serial.print  (ipBase+ipSup[s]+" ");
        Serial.print  (ipnames[s]);
        Serial.print  (sock_usage[s]);
        Serial.print  (" watt");
        Serial.print  (sock_status[s]);
        Serial.print  (Sock[s]);
        Serial.println(" dots");
      }  
    }
      M5.dis.fillpix  (black);
      if   (all_off)  {                   //power all sockets under 240w
//    M5.dis.drawpix  (hor[12],green);    //green point middle screen
      M5.dis.drawpix  (fun[funpos],green);//green point almost middle screen    
    } else {
      FillTotal       (tot_usage);        // fill values
      DispMatrix      ();                 // display bars
    }
    Serial.println    ();    
    for               (int i=0;i<4;i++){  // serial print collected data
        ipDevice =    ipBase  + ipSup[i];
        Serial.printf ("Sock%d = %-*s %-13s %8.2f watt %s", i, 12, ipnames[i].c_str(), ipDevice.c_str(), sock_usage[i], sock_status[i].c_str());
        for           (int x=0;x<Sock[i];x++){Serial.print(" *");}
        Serial.println();
    }
    Serial.printf     ("Total = %-*s %-13s %8.2f watt", 12, " ", " ", tot_usage);
    Serial.println    (" van " + String   (max_power)+" watt");
    if                (missed&&dummy_power>0){Serial.println("* dummy " + String(dummy_power) + " watt");Serial.println();}

    if (tot_usage >=  max_power){         // switch off socket if > max_power
      Serial.println  ("tot_usage >= max_power");  
      Serial.println  (String(tot_usage)+">="+ String(max_power));  
      Serial.println  ("TURNING OFF device " + String(turn_off_sock)+" "+ipnames[turn_off_sock]);  
      PutSocketStat   (turn_off_sock, false);
    }

    if (stopped){
      if (auto_turn_on){                  // switch on socket if < max_power and auto_turn_on is true
        if  (tot_usage <= min_power){
          Serial.println  ("tot_usage <= min_power");  
          Serial.println  (String(tot_usage)+"<="+ String(min_power));  
          Serial.println  ("TURNING ON device  " + String(turn_off_sock)+" "+ipnames[turn_off_sock]);  
          PutSocketStat   (turn_off_sock, true);
          stopped   =     false;
        }
      }
    }
       
    if(deb){                             // if debug show value and lastvalue
      for             (int i=0;i<4;i++){
      Serial.println  ("Sock"+String(i)+"="+String(sock_usage[i])+" last="+String(sock_last_usage[i])+" = "+String(Sock[i])+" dots");
      }
    }
    // delay          (waitmsec);
    Serial.println    ("waiting ..."+String(waitmsec/1000)+" seconds");
    waitingtime =     millis  ();
    do { 
        if   (all_off){                // do some animation
          flip++;
          if (flip==15){
            M5.dis.drawpix(fun[funpos],black);
          }
          if (flip==30){
            M5.dis.drawpix(fun[funpos],green);
          flip=0;
          }
        }
        M5.update();  //  Read the press state of the key. 
        if                (M5.Btn.wasPressed()){
          Serial.println  ("TURNING ON device  " + String(turn_off_sock)+" "+ipnames[turn_off_sock]);  
          PutSocketStat   (turn_off_sock, true);
        } else  {         delay    (100);}
    } while (millis() -   waitingtime < waitmsec);
    Serial.println        ();
}   // end of loop

void  DispMatrix(){
for (int x = 0; x < 5; x++) {
  for (int y = 0; y < Sock[x]; y++) {
    if (y < 3) {
      kleur   =   green;
    } else if     (y == 3) {
        kleur =   orange;
    } else if     (y >= 4) {
        kleur =   red;
    }
    if  (sock_status[x]=="*"){kleur = blue;}
    if  (x==4)               {kleur = red;}
    M5.dis.drawpix(((4-y) * 5) + x, kleur);
    }
  }
}

//int  boundries[4]   = {240,480,720,960,1200};   //FillMatrix values
void FillMatrix(int value,int line){
        if (value < boundries[0]) {
            Sock[line] = 0;
        } else if (value < boundries[1]) {
            Sock[line] = 1;
        } else if (value < boundries[2]) {
            Sock[line] = 2;
        } else if (value < boundries[3]) {
            Sock[line] = 3;
        } else if (value < boundries[4]) {
            Sock[line] = 4;
        } else {
            Sock[line] = 5;
        }
}

//int  totbounds[5]   = {1000,2000,3000,3500,3600};   //FillTotal  values
void FillTotal(int value){
        if (value < totbounds[0] ) {
            Sock[4] = 0;
        } else if (value < totbounds[1]) {
            Sock[4] = 1;
        } else if (value < totbounds[2]) {
            Sock[4] = 2;
        } else if (value < totbounds[3]) {
            Sock[4] = 3;
        } else if (value < totbounds[4]) {
            Sock[4] = 4;
        } else {
            Sock[4] = 5;
        }
}

// error_action         (er_color,er_blink,er_time);
// er_color see colortable under //matrix
// er_blink 0=noblink,  1=blink
//          >=50<100    noblink use pixel er_blink-50
//             >=100    link    use pixel er_blink-100
// er_time  time in seconds, 0 = infinite

void error_action       (int er_color, int er_blink,unsigned int er_time){  
   int num;
   if (er_blink>=100){
      num   =           er_blink-100;er_blink=1;
   } else {
    if (er_blink>=50 )  {
      num   =           er_blink-50;er_blink=0;
    }
   }
   pixel=hor[num];
   er_time    =         er_time*1000;
//   #ifdef  prowl
//       SendProwl        (errortxt);
//   #endif
   M5.dis.drawpix       (pixel,er_color);
   unsigned long startTime = millis();
   do {
    if (er_blink==1){
      delay (250);
      M5.dis.drawpix    (pixel,er_color);//1st pixel of pix
      delay (250);
      M5.dis.drawpix    (pixel,black);
    }  
   } while (millis()  - startTime<er_time||er_time==0);         // Wacht maximaal x seconden
   M5.dis.drawpix       (pixel,black);
}

void ConnectAtom(){
  M5.dis.drawpix        (12,blue);
  Serial.println        (">>>>> ConnectAtom()");
  wifiMulti.addAP       (ssid,password);            // Storage wifi configuration
  Serial.print          ("\nConnecting Wifi...\n"); // Serial port format output
  while                 (wifiMulti.run()!=WL_CONNECTED){M5.dis.drawpix(12,black);delay(250);Serial.print(".");M5.dis.drawpix(12,blue);delay (250);}
  Serial.println        ();
  M5.dis.drawpix        (12,black);  // BLACK
}

void GetSocketDataAtom2 (int DevNr){ 
  ipDevice              = ipBase  + ipSup[DevNr];
  M5.dis.drawpix        (hor[DevNr*5],blue);
  if(deb){
    Serial.println      (">>>>> GetSocketDataAtom2()");
    Serial.print        (ipnames[DevNr]);
    Serial.print        ("(");
    Serial.print        (ipDevice);
    Serial.println      (")");  
  } else {
    Serial.print        (ipnames[DevNr]+" ");
  }
  error   = false;
  no_data = true;
  ercol   = red;
  if ((wifiMulti.run    () == WL_CONNECTED)) {                    // wait for WiFi connection.
    if(deb)             {Serial.print ("[HTTP] begin...\n");}
    unsigned long       startTime = millis();
    do {                // if errorcode !=200 retry a few times
      http.begin        ("http://" + ipDevice + "/api/v1/data");  // configure traged server and url. 
      if(deb)           {Serial.print       ("[HTTP] GET...\n");}
      httpCode       =  http.GET();                               // start connection and send HTTP header.
      if (httpCode==200){break;} 
      if(deb)           {Serial.print       ("HTTPcode:");Serial.print(httpCode);}
      //M5.update();    //testing 
      error_action      (pink,(5*DevNr)+100,5);   // blink 5 seconds pink color pix DevNr
      if(deb)           {Serial.println     (" retrying ...");}
    } while (millis() - startTime < 10000);                       // Wacht maximaal 10 seconden op een geldige connectie    
    if (httpCode > 0)   {                                           // httpCode will be negative on error. 
      if(deb)           {Serial.printf      ("[HTTP] GET... code: %d\n", httpCode);}
      if (httpCode ==   HTTP_CODE_OK) {                             // file found at server. 
        payload =       http.getString();
        if(deb)         {Serial.println     ("Response payload:");Serial.println(payload);}
        DeserializationError error = deserializeJson(doc, payload); // Parse JSON response
        if (error) {
          Serial.print  ("deserializeJson() failed: ");
          Serial.println(error.f_str());
        } else {                                             
          // Extract values from JSON response
          sstrength   = doc["wifi_strength"];
          spower      = doc["active_power_w"];
          tspower     = doc["total_power_import_t1_kwh"];
          if(deb)         {
            Serial.println("wifi_strength : "  + String(sstrength));
            Serial.println("total_power   : "  + String(tspower));
            Serial.println("active_power_w: "  + String(spower));
          } else {
            //Serial.println(String(spower)+" watt.");        
          }
          no_data  =    false;
        }
      #ifdef  prowl
              prowlsent [DevNr]=false;    // reset sent message flag
      #endif
      }      // end of HTTP_CODE_OK
    } else { // httpCode will be negative on error.
        error      =    true;
        //Serial.print  ("Error code: ");
        Serial.println  (httpCode);
      switch            (httpCode) {
        case 0:
        ercol     =     white;
        Serial.println  ("Verbindingsfout: Kan geen verbinding maken met de server.");
        errortxt  =     "Error code: 0 geen api?";
        break;
        case -1:
        ercol     =     pink;
        Serial.println  ("Verbindingsfout: Kan geen verbinding maken met de server.");
        errortxt  =     "Error code: -1 plaats socket in stopcontact!";
        break;
        case -5:
        ercol     =     blue;
        Serial.println  ("Timeout: Verbinding met de server is verlopen.");
        errortxt  =     "Error code: -5 haal socket uit stopcontact en plaats terug!";
        break;
        default:
        ercol     =     yellow;
        errortxt  =     "Onbekende fout: Er is een onverwachte HTTP fout opgetreden.";
        break;
      }
      Serial.println    (errortxt);
    }    
    // End HTTP connection
    http.end();
    }
    if(error){
      error_action      (ercol,(5*DevNr)+50,1);
      M5.dis.drawpix    (pixel,ercol);            // turn led on again
      sstrength   =     0;
      spower      =     0;
      tspower     =     0;
      missed      =     true;
      #ifdef  prowl
        String(prowltmp) = ipnames[DevNr];
        prowltmp    +=   errortxt;
        prowltmp    +=   " ";
        if (prowlsent   [DevNr]==false){        // not sent message before
          SendProwl     (prowltmp);
        }
        prowlsent       [DevNr]=true;
      #endif
    } else {
      error_action      (green,(5*DevNr)+50,1);
      M5.dis.drawpix    (pixel,green);            // turn led on again
    }
    if (no_data){
      sock_status       [DeviceNr] = "*";
      spower      =     sock_last_usage[DeviceNr];
      if  (spower==0)   {spower=999;}//fill in dummy value
    } else {
      sock_status       [DeviceNr] = "";
      sock_last_usage   [DeviceNr] = sock_usage[DeviceNr];
    }
    sock_usage          [DeviceNr] = spower;
    DeviceNr++;
}

void HorLine            (int line,int color){
  for (int h=0;h<5;h++) {
    M5.dis.drawpix      ((h*5)+line,color);
  }
}

void PutSocketStat      (int DevNr, bool ON){ 
  ipDevice              = ipBase  + ipSup[DevNr];
  Serial.println        (">>>>> PutSocketDataAtom2()");
  Serial.print          (ipnames[DevNr]);
  Serial.print          ("(");
  Serial.print          (ipDevice);
  Serial.println        (")");  
  error       =         false;
  if (ON) {
    putData   =         PowerON;
    HorLine             (DevNr,green);
  } else {
    putData   =         PowerOFF;
    HorLine             (DevNr,pink);
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
  if(error)             {HorLine(DevNr,red);}
}

#ifdef  prowl
void InitProwl(){
  Serial.println              (">>>>> InitProwl()");
  EspProwl.begin();
  EspProwl.setApiKey          (ProwlapiKey);    
  EspProwl.setApplicationName (ProwlAPN);
} 

void SendProwl(String data2send){
  Serial.print                (">>>>> SendProwl(");
  Serial.print                (data2send);
  Serial.println              (")");
  char    ProwlMsg            [data2send.length()+1];
  data2send.toCharArray       (ProwlMsg,data2send.length()+1);   
  Serial.print                ("ProwlMsg:");
  Serial.println              (ProwlMsg);
  httpCode        =           EspProwl.push(ProwlLoc,ProwlMsg, 0);
  Serial.print                ("Prowl httpCode:");
  Serial.println              (httpCode);
  Serial.print                ("Message sent to prowl");
}  
#endif
