/* encoder source: https://arduinogetstarted.com/tutorials/arduino-rotary-encoder 
 * program  : gtmans 2025
 * purpose  : Wireless Thermostat using Homewizard api to control a specific socket used by some heating appliance
 * parts    : D1-mini, SHT30 sensor shield or external via i2c connector (like M5-ENVIII), rotary switch, oled SSD1306 display
 * 
 * features : can show actual temp and humidity, Has adjustable SocketIP, wanted temperature, rotary step, minimal cummuting time, max. operating time 
 * 
 * Warning:
 * be aware that this device can turn a socket on without you being there 
 * use with care. Do not use unattended. Lock socket IP-address in your router (bind to MAC address) so it cannot change
 * 
 * v4.3
 * max_prog_time==0 means no max time-done
 * when pressed option to set cummuting time-done 
 * blink led at FULLSTOP-done
 * detect external i2c sensor at 0x44
 * 
 * v4.4
 * improved showing fault initiating display or sht30
 *
 * 2do v4.5
 * option to use humidity io. temperature to control socket (humidifier io stove) 
 * harmless bug: anti commuting still starts first time after commute_int is over
 * use of special board rotary switch with screen maybe other processort
 */
 //SETUP VARS fill in your values
unsigned long temp_start,chk_temp_int =   30000;  // check temp interval in milliseconds
unsigned long sock_start,chk_sock_int =   20000;  // check socket interval or socket is on or off
unsigned long repo_start,chk_repo_int =   15000;  // serial report status every ...
unsigned long comm_start,commute_int  =   60000;  // cummuting time: wait at least .. milliseconds to turn off or on again (60000=1 minute)
unsigned long prog_start,max_prog_time= 7200000;  // max. operation time (3600000=1hour, 0=none}
float         wanted=20,stap=0.2;                 // preferred temperature, step
// also change ipDevice  ="192.168.2.214" further on in this program near line 45

//WIFI
#include      "gewoon_secrets.h"  // your wifi settings
/* gewoon_secrets.h:    wifi passwords and IP adresses or fill in below values
const char* ssid      = "mySSID";        
const char* password  = "myWIFIpassword";*/
#include                <Arduino.h>
#include                <ESP8266WiFi.h>
#include                <ESP8266WiFiMulti.h>
#include                <ESP8266HTTPClient.h>
#include                <WiFiClient.h>
#include                <ArduinoJson.h>
StaticJsonDocument<200> doc;
ESP8266WiFiMulti        WiFiMulti;

String    ipDevice    = "192.168.2.214"; // IP-address of socket
String    Socket      = "http://"+ipDevice;
String    SocketStat  = "/api/v1/state";
String    SocketData  = "/api/v1/data";
int       httpCode;
bool                    error,PowerIsOn,LockIsOn,SockIsOn;
String    PowerON     = "{\"power_on\": true}";
String    PowerOFF    = "{\"power_on\": false}";
String                  putData,payload,errortxt;

//encoder

#include  <ezButton.h>      // the library to use for SW pin
#define   CLK_PIN       13  // encoder s1
#define   DT_PIN        12  // encoder s2
#define   SW_PIN        14  // encoder push button not essential
#define   DIRECTION_CW  0   // clockwise direction
#define   DIRECTION_CCW 1   // counter-clockwise direction
bool      pressed     = false;
int counter           = 0;
int direction         = DIRECTION_CW;
int                     CLK_state,prev_CLK_state;
ezButton button         (SW_PIN);  // create ezButton object that attach to pin 14

//oled
#include                <Wire.h>
#include                <Adafruit_SSD1306.h>
#include                <Adafruit_SHT31.h>
#define                 SCREEN_WIDTH  128
#define                 SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//temp    SHT30-setup
Adafruit_SHT31 sht31  = Adafruit_SHT31();

//various
unsigned long           heat_start,heat_stop,sock_stop,comm_stop,just_now;
unsigned int            flip;
float                   temp,hum;
bool                    FULLSTOP,delayed_action,commuting,showflip,do_heat;
int                     ACTION,ACTION2COME,PUTON=1,PUTOFF=2,NONE=3,OTHER=99;

void errorflash(int tim){
  for (int t=0;t<10;t++){
    digitalWrite        (LED_BUILTIN, LOW);       //on 
    delay               (tim);
    digitalWrite        (LED_BUILTIN, HIGH);      //off 
    delay               (tim);
  }
}

void setup() {
  Wire.begin            (D2, D1); // SDA op D2, SCL op D1
  Serial.begin          (115200);
  while(!Serial)        {delay(50);}
  Serial.println        ();
  Serial.println        (__FILE__);
  pinMode               (LED_BUILTIN, OUTPUT);    //declare LED as output (D4)    
  digitalWrite          (LED_BUILTIN, HIGH);      //off 
  pinMode               (CLK_PIN,     INPUT);
  pinMode               (DT_PIN,      INPUT);
  button.setDebounceTime(50);                     // set debounce time to 50 milliseconds
  prev_CLK_state  =     digitalRead(CLK_PIN);
  // OLED-initialisatie
  if (!display.begin    (0x3C, 0x3C)) { // Pas 0x3C aan als nodig
    Serial.println      (F("OLED niet gevonden"));
    errorflash          (1000);
    //for (;;);
  }
  // SHT30-init
  display.display       ();
  if (!sht31.begin      (0x44)) { // Pas 0x44 aan als nodig
    Serial.println      (F("SHT30 niet gevonden op 0x44"));
    errorflash          (500);
    if (!sht31.begin    (0x45)) { // Pas 0x44 aan als nodig
      Serial.println    (F("SHT30 niet gevonden op 0x45"));
      errorflash        (500);
    }
  }
  chk_temp              ();
  WiFiConnect           ();
  GetSocketStat         ();
  if (LockIsOn)         {Serial.println("Unlock socket in Homewizard app!");}
  else                  {Serial.println("ready2go");}
  temp_start      =     millis();
  prog_start      =     temp_start;
  repo_start      =     temp_start+chk_repo_int;
  sock_start      =     temp_start;
  heat_start      =     temp_start;
  heat_stop       =     temp_start;
  comm_start      =     temp_start;
  delayed_action  =     false;
  FULLSTOP        =     false;
  ACTION          =     NONE;
  ACTION2COME     =     OTHER;
  do_display           ();
}

void loop() { 
  chk_input();          // read rotary
                        // check temp every chk_temp_int millisecs
  if(millis()     -     temp_start>chk_temp_int){  // every chk_temp_int period
    chk_temp            ();                        // read temp
    temp_start    =     millis   ();
  }
                        // check socket every chk_sock_int millisecs
  if    (millis( )  -   sock_start>chk_sock_int){  // every chk_sock_int period 
    GetSocketStat       ();
    report              ();
    sock_start=millis   ();
    if (FULLSTOP)       {Serial.println("FULLSTOP max_prog_time reached!");}
  }
                        // report every chk_repo_int millisecs
  if    (millis( )  -   repo_start>chk_repo_int){  // every chk_sock_int period 
    report              ();
    repo_start=millis   ();
  }
  
  eval_temp();          // check cummuting (pendelen)
  
  if (commuting){       // prepare alternating asterix on oled screen
      flip++;           if  (flip>30000)  {flip=0;}  
      if  (flip <15000) {showflip=true;}else{showflip=false;}
      if  (flip==15000||flip==0){do_display();}
      if  (flip==15000) {Serial.print(String((millis()-comm_start)/1000)+"-");}
  } else  {
      if  (do_heat)     {showflip=true;}else{showflip=false;}
  }
  
  if (!FULLSTOP){       //autostop after max_prog_time (default 2 hours)
    if ((millis() -     prog_start>max_prog_time)&&max_prog_time!=0){
      FULLSTOP    =     true;
      PutSocketStat     (false);
      Serial.println    ("FULLSTOP max_prog_time reached!");
      while (0==0)      {digitalWrite(LED_BUILTIN,LOW);delay(500);digitalWrite(LED_BUILTIN,HIGH);delay(500);}
    } else {
        if (millis()-comm_start>commute_int||just_now-prog_start<commute_int){// commuting waitingtime ended or just started
//        if (ACTION==PUTON ){PutSocketStat(true) ;ACTION=NONE;ACTION2COME=NONE;Serial.println("ACTION2COME=NONE");}//on ****
//        if (ACTION==PUTOFF){PutSocketStat(false);ACTION=NONE;ACTION2COME=NONE;Serial.println("ACTION2COME=NONE");}//off
          if (ACTION==PUTON ){PutSocketStat(true) ;ACTION=NONE;Serial.println("SET ACTION2COME=NONE?");}//on ****
          if (ACTION==PUTOFF){PutSocketStat(false);ACTION=NONE;Serial.println("SET ACTION2COME=NONE?");}//off
        }
     }
  }     // not fullstop 
}       // end of loop

void  report(){
  Serial.print            ("STATUS SockIsOn=");
  if  (SockIsOn)          {Serial.print ("ON");}else{Serial.print("OFF");}
  Serial.print                          (" do_heat=");
  if  (do_heat)           {Serial.print ("ON");}else{Serial.print("OFF");}
  Serial.print                          (" ACTION=");
  if (ACTION==PUTOFF)     {Serial.print ("PUTOFF");}
  if (ACTION==PUTON)      {Serial.print ("PUTON");}
  if (ACTION==NONE)       {Serial.print ("NONE");}
  Serial.print                          (" DELAYED ACTION=");
  if (ACTION2COME==PUTOFF){Serial.print ("PUTOFF");}
  if (ACTION2COME==PUTON) {Serial.print ("PUTON");}
  if (ACTION2COME==NONE)  {Serial.print ("NONE");}
  if (ACTION2COME==OTHER) {Serial.print ("OTHER");}
  Serial.println                        ();
  Serial.print                          ("Temperatuur: ");
  Serial.print                          (temp);
  Serial.print                          (" gewenst: ");
  Serial.print                          (wanted);
  Serial.print                          ("°C");
  if (showflip==true)     {Serial.print ("*");}
  if (commuting)          {Serial.print (" commuting");}
  if (digitalRead         (LED_BUILTIN)==HIGH)
                          {Serial.print ("LED OFF");} else
                          {Serial.print ("LED ON");}
  Serial.println                        ();
  int W_end,C_int,C_sec;                 // 2 avoid using negative milliseconds
  C_int = commute_int/1000;
  C_sec = (millis()-comm_start)/1000;
  W_end = C_int-C_sec;
  Serial.print                          ("runningtime      = \t");
  Serial.print                          ((millis()-prog_start)/1000);
  Serial.print                          (" sec. ");//
  Serial.print                          ("    comm.interval= \t");
  Serial.print                          (C_int);
  Serial.println                        (" sec. ");
  Serial.print                          ("waiting ends in  = \t");
  Serial.print                          (W_end);
  Serial.print                          (" sec. ");
  Serial.print                          ("commutingtime    = \t");
  Serial.print                          (C_sec);
  Serial.println                        (" sec. ");    
}

void GetSocketStat      (){ 
//Serial.print          (">>>>>GetSocketData ");
//Serial.print          (Socket);
//Serial.println        (SocketStat);
//Serial.print          (">>>>>");
  if (WiFiMulti.run()== WL_CONNECTED) {
    WiFiClient          client;
    HTTPClient          http;
    http.begin          (client,Socket+SocketStat);   // HTTP GET
    httpCode     =      http.GET();  
    if (httpCode ==     HTTP_CODE_OK) {
       payload   =      http.getString();
       //Serial.println (payload);
    } else {
       Serial.printf    ("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    } 
    DeserializationError error =    deserializeJson(doc, payload);
    if (error)          {
      Serial.print      ("deserializeJson() failed: ");
      Serial.println    (error.f_str());
    } else { 
      // get the data     
      PowerIsOn   =     doc["power_on"];
      LockIsOn    =     doc["switch_lock"];
    //Serial.println    ("power_on   : " + String(PowerIsOn));
    //Serial.println    ("switch_lock: " + String(LockIsOn));
      if (PowerIsOn)    {SockIsOn=true;}else{SockIsOn=false;}
    }
    http.end();
  }
}

void PutSocketStat      (bool ON){ 
  Serial.print          (">>>>>PutSocketStat ");
//Serial.print          (ON);
  if (ON){Serial.print("ON");}else{Serial.print("OFF");}
  Serial.print          (" ");
  Serial.print          (Socket);
  Serial.println        (SocketStat);
  Serial.print          (">>>>>");
/*
  Serial.print                          (" ACTION=");
  if (ACTION==PUTOFF)     {Serial.print ("PUTOFF");}
  if (ACTION==PUTON)      {Serial.print ("PUTON");}
  if (ACTION==NONE)       {Serial.print ("NONE");}
  Serial.print                          (" DELAYED ACTION=");
  if (ACTION2COME==PUTOFF){Serial.print ("PUTOFF");}
  if (ACTION2COME==PUTON) {Serial.print ("PUTON");}
  if (ACTION2COME==NONE)  {Serial.print ("NONE");}
  if (ACTION2COME==OTHER) {Serial.print ("OTHER");}
  Serial.println                        ();

  if (ACTION==ACTION2COME){ACTION2COME=NONE;}
  Serial.println ("SETTING ACTION2COME=NONE");
*/
  comm_start    =       millis();

  if (WiFiMulti.run()== WL_CONNECTED) {
    if (ON) {
      putData     =     PowerON;
      SockIsOn    =     true;
      do_heat     =     true;
      heat_start  =     comm_start;
      digitalWrite      (LED_BUILTIN,LOW );
      Serial.print      ("START heating");
    } else {
      putData     =     PowerOFF;
      SockIsOn    =     false;
      do_heat     =     false;
      heat_stop   =     comm_start;
      digitalWrite      (LED_BUILTIN,HIGH);
      Serial.print      ("STOP  heating");
    }
    WiFiClient          client;
    HTTPClient          http;
    http.begin          (client,Socket+SocketStat);   // HTTP PUT
    http.addHeader      ("Content-Type", "application/json");
    http.addHeader      ("Content-Length", String(putData.length()));
    httpCode            = http.PUT(putData);
    if (httpCode > 0 )  {
       payload   =      http.getString();
       Serial.println   (payload);
    } else {
      Serial.print      ("Error code: ");
      Serial.println    (httpCode);    
    } 
  http.end();
  }
}

void WiFiConnect() {
  WiFi.mode             (WIFI_STA);
  WiFiMulti.addAP       (ssid,password);
  while (WiFiMulti.run()!= WL_CONNECTED) {
    digitalWrite        (LED_BUILTIN, LOW);   //on
    delay(500);
    digitalWrite        (LED_BUILTIN, HIGH);  //off
    delay(500);
    Serial.print        (".");
  }
  Serial.println        ();
  Serial.println        ("WiFi connected");
}

void chk_temp (){
  temp = sht31.readTemperature();
  hum  = sht31.readHumidity();
  if (!isnan(temp) &&   !isnan(hum)) {
  /*  
    Serial.print        ("Temperatuur: ");
    Serial.print        (temp);
    Serial.print        ("°C, Vochtigheid: ");
    Serial.print        (hum);
    Serial.print        ("%");
    Serial.print        (" gewenst: ");
    Serial.print        (wanted);
    Serial.println      ("°C");
  */
  } else {
    Serial.println      ("Fout bij lezen SHT30");
  }
    eval_temp ();  
    do_display();
}

void eval_temp(){ 
    // commuting or not?
    just_now  = millis();
    if (just_now-comm_start<commute_int&&just_now-prog_start>commute_int){//commuting
      if (commuting==false){Serial.println("START anti-commuting program");}
      commuting = true;
    } else {//stop commuting
      if (commuting==true ){  // not commuting any more
        Serial.println("EINDE anti-commuting program");
        commuting = false;
        if (delayed_action==true){
          ACTION=ACTION2COME;
          delayed_action=false;
          Serial.print          ("vertraagd uitvoeren van ");
          Serial.print          ("ACTION = ");
          if (ACTION==PUTOFF)   {Serial.print ("PUTOFF ");}
          if (ACTION==PUTON)    {Serial.print ("PUTON  ");}
          if (ACTION==NONE)     {Serial.print ("NONE   ");}
          Serial.println        ();
        }
        do_display();        
      }
    } 
    // set action
    if (ACTION!=ACTION2COME){       // not just set by delayed action
      if (temp<wanted)              {ACTION= PUTON;} else      {ACTION=PUTOFF;};
      if (ACTION==PUTON && SockIsOn||ACTION==PUTOFF&&!SockIsOn){ACTION=NONE;}
    } else {
       //ACTION2COME=NONE;//***
       //Serial.println("SETTING ACTION2COME=NONE");
    }
    // if just started kill commuting
    if (millis()-prog_start<commute_int){commuting=false;}
    
    // check if action was asked but is delayed
    if ((commuting&&ACTION!=NONE)&&delayed_action==false){
        delayed_action=true;
        ACTION2COME = ACTION;
        ACTION      = NONE;
        Serial.print            ("TEMP:"+String(temp)+" WANTED:"+String(wanted));
        Serial.print            (" SETTING delayed action=");
        if (ACTION2COME==PUTOFF){Serial.println("PUTOFF");}
        if (ACTION2COME==PUTON) {Serial.println("PUTON");}
        if (ACTION2COME==NONE)  {Serial.println("NONE");}
    }
}

void do_display(){
  display.clearDisplay          ();
  display.setTextSize           (2);
  display.setTextColor          (SSD1306_WHITE);
  display.setCursor             (0,0);
  if (pressed){                 // rotary button pressed
    display.println             ("Interval");
    display.setTextSize         (3);
    display.print               (commute_int/1000);    
    display.println             (" sec");
    display.print               (hum);    
    display.println             ("%");   
  } else {
    display.println             ("Now/Wanted");
    display.setTextSize         (3);
    display.print               (temp);    
    display.print               ("c");//°C 
    if (temp<wanted)            {display.print("+");}else{display.println();} 
    display.print               (wanted);
    display.print               ("c");   
    if (showflip&&commuting)    {display.print("*");}else{display.println();}     
  }
  display.display               ();
}

void chk_input(){               // read rotary
  button.loop();                // MUST call the loop() function first
                                // read the current state of the rotary encoder's CLK pin
  CLK_state       =             digitalRead(CLK_PIN);
                                // If the state of CLK is changed, then pulse occurred
                                // React to only the rising edge (from LOW to HIGH) to avoid double count
  if (CLK_state   !=            prev_CLK_state && CLK_state == HIGH) {
                                // if the DT state is HIGH
                                // the encoder is rotating in counter-clockwise direction => decrease the counter
    if (digitalRead(DT_PIN) ==  HIGH) {
      counter--;
      direction   =             DIRECTION_CCW;
    } else {
                                // the encoder is rotating in clockwise direction => increase the counter
      counter++;
      direction   =             DIRECTION_CW;
    }
    //Serial.print              ("DIRECTION: ");
    
    if (direction ==            DIRECTION_CW){
//    Serial.print              ("Clockwise");
      if (pressed){
        if (commute_int<max_prog_time){commute_int +=10000;}
      } else {
        wanted      +=          stap;
      }
    } else {
//    Serial.print              ("Counter-clockwise");
      if (pressed){
        if (commute_int>0)      {commute_int -=10000;}       
        } else {
          wanted      -=        stap;
      }
    }   
    do_display();
  }
                                // save last CLK state
  prev_CLK_state    =           CLK_state;
  if (button.isPressed())       {
    Serial.print                ("The button is pressed and is now ");
    pressed         =           !pressed;
    if (pressed)                {Serial.println("ON");}else{Serial.println("OFF");}
    do_display();
  }
}

