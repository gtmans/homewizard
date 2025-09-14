//Homewizard PIB gtmans sept 2025

#include    <M5Stack.h>
#include    <WiFi.h>
#include    "gewoon_secrets.h"
/* WiFi and token settings
const char* ssid = "mynetwork";
const char* password = "secret";
const char* host = "192.168.2.80"; // IP van je batterij
const char* token ="ABCDFLAF32716C1EB6226984E9060112"; // je ontvangen token
*/
#include    "icons.h"           // make your own images using https://www.online-utility.org/image/convert/to/XBM
#include    <WiFiClientSecure.h>
#include    <HTTPClient.h>
#include    <ArduinoJson.h>
WiFiClient  client;
StaticJsonDocument      <1250> doc;
#include    <EEPROM.h>
#include    "time.h"
//time
const char* ntpServer = "pool.ntp.org";
char currentDate[7] = "000000"; // DDMMYY + nulterminatie

// Functie die datum ophaalt en global currentDate bijwerkt
bool wachtTotDatumOfReset(struct tm * timeinfo, uint32_t timeout_ms = 30000) {
  uint32_t start = millis();
  while             (!getLocalTime(timeinfo, 1000)) {
    if              (millis() - start > timeout_ms) {
      Serial.println("\nGeen geldige datum, reset!");
      delay         (100);
      ESP.restart   ();
      return        false; 
    }
    Serial.print    (".");
  }
  // Datum string vullen
  sprintf           (currentDate, "%02d%02d%02d", timeinfo->tm_mday, timeinfo->tm_mon + 1, (timeinfo->tm_year + 1900) % 100);
  return            true;
}

void wachtOpDatumOfReset() {
  struct tm tijd;
  Serial.print      ("Wachten op geldige datum...");
  wachtTotDatumOfReset(&tijd, 30000); // 30s timeout en reset bij mislukking
  sprintf           (currentDate, "%02d%02d%02d", tijd.tm_mday, tijd.tm_mon+1, (tijd.tm_year+1900)%100);
  Serial.print      ("\nDatum ontvangen: ");
  Serial.println    (currentDate);
}

//eeprom
#define EEPROM_SIZE 32
#define ADDR_DATE   0     // 6 bytes voor DDMMYY als string (bv "130925")
#define ADDR_FLOAT1 8     // float 1
#define ADDR_FLOAT2 12    // float 2

// api parameters
struct BatteryStatus {
  float energy_import_kwh;
  float energy_export_kwh;
  float power_w;
  float voltage_v;
  float current_a;
  float frequency_hz;
  int   state_of_charge_pct;
  int   cycles;
};
BatteryStatus battery;
  //P1
  float topower,acpower,tipower;
  bool no_data,error;

// Batterij parameters
int     cap=2700,vulling;
float   batteryPercentage;
float   oldPercentage;
float   avPerc=0.00,avPerc_ever=0.00;
float   kWhIn ,kWhIn_ever ,kWhIn_start;
float   kWhOut,kWhOut_ever,kWhOut_start;
bool    isCharging;
bool    isDischarging;
bool    ColorChange;
float   time_to_full,time_to_empty;
String  tijdString;

String  formatTimeHHMM(float time_in_minutes) {
  int total_minutes = (int)time_in_minutes;
  int hours = total_minutes / 60;
  int minutes = total_minutes % 60;
  String result = "";
  if (hours < 10) {
    result += '0';  // leidende nul voor uren < 10
  }
  result += String(hours) + ':';
  if (minutes < 10) {
    result += '0';  // leidende nul voor minuten < 10
  }
  result += String(minutes);
  return result;
}
  
// Timer parameters
unsigned long               lastUpdateTime      = 0;
const   unsigned long       updateInterval      = 20000;// 3 times per minute
// Kleuren
uint16_t laadKleur[3]     = {TFT_MAGENTA,TFT_PURPLE   ,M5.lcd.color565(80, 0, 80)};
uint16_t ontlaadKleur[3]  = {TFT_GREEN  ,TFT_DARKGREEN,M5.lcd.color565(0 ,64, 0)};
uint16_t rustKleur[3]     = {TFT_WHITE  ,TFT_LIGHTGREY,TFT_DARKGREY};
uint16_t terugKleur[3]    = {TFT_RED,M5.lcd.color565(128,0,0),M5.lcd.color565(64,0,0)};
uint16_t zonKleur[3]      = {TFT_YELLOW,M5.lcd.color565(128,128,0),M5.lcd.color565(64,64,0)};
uint16_t emptyColor       = TFT_BLACK;
uint16_t Bcolor,BcolorLast;
int      brightness;

// Batterij dimensies
const int batteryX        = 10;   // Helemaal naar links
const int batteryY        = 30;
const int batteryWidth    = 100;  // Smaller dan helft van 320px
const int batteryHeight   = 180;
const int batteryBorder   = 6;    // Meer zwart rondom
// Batterij tip (bovenkant)
const int tipWidth        = 40;
const int tipHeight       = 15;
const int tipX            = batteryX + (batteryWidth - tipWidth) / 2;
const int tipY            = batteryY - tipHeight;
// Segmenten (10 rechthoekjes)
const int numSegments     = 10;
const int segmentGap      = 5+2; // Meer zwart tussen segmenten
const int segmentHeight   = (batteryHeight - (2 * batteryBorder) - ((numSegments - 1) * segmentGap)) / numSegments;
// Tekst posities
const int textX           = 130;
const int percentageY     = 60;
const int kWhInY          = 120;
const int kWhOutY         = 160;
const int statusY         = 200;
//various
int screen,screens=4;

//HTTP stuff
String IPServer   =    "192.168.2.11"; //address of external ESP8266 HTTPS server for Solaredge API (D1-mini)
String P1Server   =    "http://192.168.2.215/api/v1/data";
//constructing URLs from external HTTPS server (ESP8266) to Solaredge API
String ipDevice;
String SensorServer    = "http://"+IPServer;
String serverNameCur   = SensorServer + "/current";
String serverNameTod   = SensorServer + "/today";
String payload;
int    httpCode;
float  Sun_today;
float  Sun_now;
int    sunflip;

void get_batstat(bool first){
  bullet              (TFT_BLUE);
  WiFiClientSecure    client;
  client.setInsecure(); // overslaat certificaatcheck
  if (!client.connect (host, 443)) {
    Serial.println    ("Verbinding mislukt!");
    return;
  }
  String url =        "/api/measurement";
  String request =    String("GET ") + url + " HTTP/1.1\r\n" +
                      "Host: " + host + "\r\n" +
                      "Authorization: Bearer " + token + "\r\n" +
                      "X-Api-Version: 2\r\n" +
                      "Connection: close\r\n\r\n";
  client.print        (request);
  bool headers_ended =false;
  String jsonBody    ="";
  // Haal JSON-body
  while               (client.connected()) {
    String line =     client.readStringUntil('\n');
    if (line == "\r") {
      headers_ended = true;
      break;
    }
  }
  // Lees JSON-body
  while (client.available()) {
    char c = client.read();
    jsonBody += c;
  }
//Serial.println("Ontvangen JSON:");
  Serial.println(jsonBody);
  // https://api-documentation.homewizard.com/docs/v2/batteries
/*{"energy_import_kwh":7.938,"energy_export_kwh":4.085,
   "power_w":720.007,"voltage_v":235.283,"current_a":3.15,
   "frequency_hz":50.011,"state_of_charge_pct":95,"cycles":2}
*/
  // parse JSON
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonBody);
  if (error) {
    Serial.print(F("Fout bij parsen JSON: "));
    Serial.println(error.c_str());
    return;
  }
  battery.energy_import_kwh   = doc["energy_import_kwh"];
  battery.energy_export_kwh   = doc["energy_export_kwh"];
  battery.power_w             = doc["power_w"];
  battery.voltage_v           = doc["voltage_v"];
  battery.current_a           = doc["current_a"];
  battery.frequency_hz        = doc["frequency_hz"];
  battery.state_of_charge_pct = doc["state_of_charge_pct"];
  battery.cycles              = doc["cycles"];

  if (battery.power_w<0)        {isDischarging=true;} else {isDischarging=false;}
  isCharging              =     !isDischarging;
  batteryPercentage       =     battery.state_of_charge_pct;
//if (batteryPercentage ==100){isCharging=false;isDischarging=false;}
  if (battery.power_w==0||battery.power_w==6||batteryPercentage ==100){isCharging=false;isDischarging=false;}
  if (first){
    kWhIn_start   =   battery.energy_import_kwh;
    kWhOut_start  =   battery.energy_export_kwh;
  } else {
    kWhIn         =   battery.energy_import_kwh-kWhIn_start;
    kWhOut        =   battery.energy_export_kwh-kWhOut_start;
  }
  avPerc          =   (kWhOut/kWhIn)*100;
//avPerc_ever     =   (kWhOut_start/kWhIn_start)*100;
  avPerc_ever     =   (battery.energy_export_kwh/battery.energy_import_kwh)*100;
  bullet              (TFT_BLACK);
}

void dumpinfo(){
  Serial.println      ("IN: \t"     +String(battery.energy_import_kwh,3));
  Serial.println      ("UIT:\t"     +String(battery.energy_export_kwh,3));
  Serial.println      ("POWER:\t"   +String(battery.power_w,0)+" Watt.");
  Serial.println      ("VULLING:\t" +String(battery.state_of_charge_pct)+"%");
  Serial.println      ("CYCLES:\t"  +String(battery.cycles));

  Serial.println      ("Average:\t" +String(avPerc,2)+"%");
  Serial.println      ("AverageEvr:\t" +String(avPerc_ever,2)+"%");

  Serial.print        ("cap: ");      Serial.println(cap);
  Serial.print        ("vulling: ");  Serial.println(vulling);
  Serial.print        ("battery.power_w: "); Serial.println(battery.power_w);
  Serial.println      (tijdString);  

  if (battery.power_w>0) {
    Serial.println    ("BAT IN :"+String(battery.power_w,0)+"w.");
  } else {
    Serial.println    ("BAT UIT:"+String(battery.power_w*-1)+"w.");
  }
  Serial.println      ("ZON IN :"+String(Sun_now,0)+"w.");
  //if (Sun_now<abs(acpower)-battery.power_w){}
}

void setup() {
  M5.begin();
  M5.Lcd.setRotation  (1);
  WiFi.begin          (ssid, password);
  bullet              (TFT_GREEN);
  while               (WiFi.status() != WL_CONNECTED) {
    delay             (500);
    Serial.print      (".");
  }
  Serial.println      ("WiFi verbonden!");
  bullet              (TFT_BLACK);
                      // Europe/Amsterdam: automatische zomertijd: CET-1CEST,M3.5.0/2,M10.5.0/3
  setenv              ("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1); tzset();
  configTime          (0, 0, ntpServer); 
  get_batstat(true);  // get kWhIn_start,kWhOut_start
  getdate();          // Haal datum en update kWhIn_start,kWhOut_start indien zelfde datum
  oldPercentage     = batteryPercentage;
  M5.Lcd.fillScreen   (TFT_BLACK);
  CatchTheSun         ();
  GetSocketP1         ();
  dumpinfo            ();
  screen=-1;
  showscreen          ();
} 

void getdate()        {// datum en initiele waarden evt uit eeprom
  EEPROM.begin        (EEPROM_SIZE);
  wachtOpDatumOfReset ();   // Datum ophalen uit NTP
  char eepromDate[7];     // Datum uit EEPROM lezen
  for (int i = 0; i < 6; i++) eepromDate[i] = EEPROM.read(ADDR_DATE + i);
  eepromDate[6]   =   '\0';
  Serial.print        ("Huidige datum: "); Serial.println(currentDate);
  Serial.print        ("EEPROM datum:  "); Serial.println(eepromDate);

  if (strcmp          (currentDate, eepromDate) != 0) {// Datum is anders, schrijf datum en floats naar EEPROM
    for               (int i = 0; i < 6; i++) EEPROM.write(ADDR_DATE + i, currentDate[i]);
    EEPROM.put        (ADDR_FLOAT1, kWhIn_start);
    EEPROM.put        (ADDR_FLOAT2, kWhOut_start);
    EEPROM.commit     ();
    Serial.print      ("Datum:");
    Serial.println    (currentDate);
    Serial.printf     ("kWhIn_start : %.2f\n", kWhIn_start);
    Serial.printf     ("kWhOut_start: %.2f\n", kWhOut_start);
    Serial.println    ("Update EEPROM: datum en floats zijn opgeslagen.");
  } else {            // Datum is gelijk, floats uit EEPROM gebruiken
    EEPROM.get        (ADDR_FLOAT1, kWhIn_start);
    EEPROM.get        (ADDR_FLOAT2, kWhOut_start);
    Serial.print      ("Startwaarden uit EEPROM voor datum:");
    Serial.println    (eepromDate);
    Serial.printf     ("kWhIn_start : %.2f\n", kWhIn_start);
    Serial.printf     ("kWhOut_start: %.2f\n", kWhOut_start);
  }
}

void calc_time(){
  vulling = (batteryPercentage * cap) / 100;        // Bereken huidige vulling in Wh
  if (isCharging && battery.power_w > 0) {          // Laadtijd, vermogen is positief
    time_to_full  = ((float)cap - vulling) / battery.power_w * 60.0;
  } else if (!isCharging && battery.power_w < 0) {  // Ontlaadtijd, vermogen is negatief dus omdraaien
    time_to_empty = (float)vulling / abs(battery.power_w) * 60.0;
  } else {                                          // Geen geldige situatie: zet tijd op 0 of '—'
  time_to_full    = time_to_empty = 0;
  }
}

void showfill(){
  if (isCharging) {
    Serial.print("Tijd tot accu VOL: ");
    Serial.print(time_to_full, 0);
    Serial.println(" minuten");
    tijdString = formatTimeHHMM(time_to_full);
    infoscreen (abs(battery.power_w),"w.",vulling,time_to_full);
  } else {
    Serial.print("Tijd tot accu LEEG: ");
    Serial.print(time_to_empty, 0);
    Serial.println(" minuten");
    tijdString = formatTimeHHMM(time_to_empty);
    infoscreen (abs(battery.power_w),"w.",vulling,time_to_empty);
  }
}

void loop() {
  M5.update(); 
  // Button handling
  if (M5.BtnA.wasPressed()) {
    brightness--;if (brightness<0){brightness=2;}
    showscreen();
  }
  if (M5.BtnB.wasPressed()) {
  //GetSocketP1();
    showscreen();
  }
  if (M5.BtnC.wasPressed()) {
    brightness++;if (brightness>2){brightness=0;}
    showscreen();
  }
  // Update batterij status
  if (millis() - lastUpdateTime > updateInterval) {
    lastUpdateTime = millis();
    oldPercentage = batteryPercentage;
    get_batstat     (false);
    dumpinfo  	    ();
    sunflip++;
    if (sunflip==2) {GetSocketP1();}
    if (sunflip==3) {CatchTheSun();sunflip=0;}
    showscreen();
  }
  delay(50);
}

void getColor(){
  // Bepaal huidige kleur
  BcolorLast  = Bcolor;
  if (isCharging) {
    Bcolor    = laadKleur[brightness];
  } else if     (isDischarging) {
    Bcolor    = ontlaadKleur[brightness];
  } else {
    Bcolor    = rustKleur[brightness];
  }
  if (BcolorLast!=Bcolor){ColorChange=true;}
  if (batteryPercentage==100){Bcolor = rustKleur[brightness];}
}

void drawBatteryFrame() {
  getColor();
  // if (!ColorChange){return;}
  /*
  // Wis batterij gebied volledig 
  M5.Lcd.fillRect(batteryX - 5, tipY - 5, 
                  batteryWidth + 10, 
                  batteryHeight + tipHeight + 10, TFT_BLACK);
  */
  // Teken batterij tip (positieve pool bovenop) in huidige kleur
  M5.Lcd.fillRect(tipX, tipY, tipWidth, tipHeight, Bcolor);
  M5.Lcd.drawRect(tipX, tipY, tipWidth, tipHeight, Bcolor);
  // Teken batterij hoofdlichaam - buitenste rand in huidige kleur
  M5.Lcd.fillRect(batteryX, batteryY, batteryWidth, batteryHeight, Bcolor);
  // Teken binnenrand (zwart voor segmenten)
  M5.Lcd.fillRect(batteryX + batteryBorder, batteryY + batteryBorder, 
                  batteryWidth - (2 * batteryBorder), 
                  batteryHeight - (2 * batteryBorder), emptyColor);
  // Teken binnen outline in huidige kleur
  M5.Lcd.drawRect(batteryX + batteryBorder, batteryY + batteryBorder, 
                  batteryWidth - (2 * batteryBorder), 
                  batteryHeight - (2 * batteryBorder), Bcolor);
}

void drawBatterySegments() {
  // Bepaal kleur op basis van status
  getColor();
  // if (!ColorChange){return;}
  // Bereken hoeveel segmenten gevuld moeten zijn
  int filledSegments = (int)((batteryPercentage / 100.0) * numSegments + 0.5);
  // Teken alle 10 segmenten
  for (int i = 0; i < numSegments; i++) {
    // Bereken Y positie (van onder naar boven vullen)
    int segmentIndex = numSegments - 1 - i; // Omgekeerde volgorde voor vullen van onder
    int segY = batteryY + batteryBorder + (segmentIndex * (segmentHeight + segmentGap));
    int segX = batteryX + batteryBorder + 3 +2;
    int segWidth = batteryWidth - (2 * batteryBorder) - 6 -4;
    // Bepaal of dit segment gevuld moet zijn
    bool isFilled = i < filledSegments;
    if (isFilled) {
      // Teken gevuld segment - alleen actieve kleur
      M5.Lcd.fillRect(segX, segY, segWidth, segmentHeight, Bcolor);
    } else {
      // Teken leeg segment
      M5.Lcd.fillRect(segX, segY, segWidth, segmentHeight, emptyColor);
    }
  }
}

void showscreen(){
  screen++;
  if      (screen==screens){screen=0;}
  M5.Lcd.fillScreen   (TFT_BLACK);
  switch  (screen){          
    case 0:                   // BATPIC 42%/6.5/3.4
      drawBatteryFrame();
      drawBatterySegments();
      updateDisplayTXT();
    break;
    case 1:                   // 500w/
      calc_time();
      showfill();
    break;
    case 2:
      screen2();
    break;
    case 3:
      screen3();
    break;
  }
}

void screen3()          {//dump
  M5.Lcd.fillScreen     (TFT_BLACK);
  M5.Lcd.setFreeFont    (&FreeSansBold18pt7b);
  M5.Lcd.setTextSize    (2);
  int TXTX      =       50;
  int TXTY[3]   =       {70,140,210};
  M5.Lcd.setTextColor   (laadKleur[brightness]);
  M5.Lcd.setCursor      (TXTX,TXTY[0]);
  M5.Lcd.print          (battery.energy_import_kwh,2);
  M5.Lcd.setTextColor   (ontlaadKleur[brightness]);
  M5.Lcd.setCursor      (TXTX,TXTY[1]);
  M5.Lcd.print          (battery.energy_export_kwh,2);
  TXTX      =           0;
  M5.Lcd.setTextColor   (rustKleur[brightness]);
  M5.Lcd.setCursor      (TXTX,TXTY[2]);
  M5.Lcd.print          ("%");
  M5.Lcd.print          (avPerc_ever);
  draw_triangle         (TXTX,TXTY[0],false,laadKleur[brightness]);
  draw_triangle         (TXTX,TXTY[1],true,ontlaadKleur[brightness]);
}

void draw_triangle      (int x, int y, bool rgt, uint16_t clr){
  int h=40,b=30;
  if (rgt){
    M5.Lcd.fillTriangle (x,y      ,x+b,y-(h/2),x,y-h,clr); 
  } else {
    M5.Lcd.fillTriangle (x,y-(h/2),x+b,y      ,x+b,y-h,clr); 
  }
}

void screen2()          {
  M5.Lcd.fillScreen     (TFT_BLACK);
  M5.Lcd.setFreeFont    (&FreeSansBold18pt7b);
  M5.Lcd.setTextSize    (2);
  int TXTX      =       0;
  int TXTY[3]   =       {60,60+70,60+140};
  uint16_t scol,bcol,acol;
  scol=zonKleur[brightness];
  if (Sun_now<(-1*acpower)){scol=TFT_ORANGE;}
  if (battery.power_w>0){bcol=laadKleur[brightness] ;}else{bcol=ontlaadKleur[brightness];}
  if (acpower>0)        {acol=terugKleur[brightness];}else{acol=ontlaadKleur[brightness];}
  M5.Lcd.drawXBitmap    (TXTX   ,TXTY[0]-50,logo9,logo9Width,logo9Height,scol);//SUN
  M5.Lcd.drawXBitmap    (TXTX+15,TXTY[1]-50,logoB,logoBWidth,logoBHeight,bcol);//BATTERY
  M5.Lcd.drawXBitmap    (TXTX   ,TXTY[2]-50,logoA,logoAWidth,logoAHeight,acol);//POWER
  M5.Lcd.setCursor      (TXTX+70,TXTY[0]);
  M5.Lcd.setTextColor   (scol);
  M5.Lcd.print          (Sun_now,0);
  M5.Lcd.setCursor      (TXTX+70,TXTY[1]);
  M5.Lcd.setTextColor   (bcol);
  int batp    =         abs(battery.power_w);
  M5.Lcd.print          (batp);
  M5.Lcd.setCursor      (TXTX+70,TXTY[2]);
  M5.Lcd.setTextColor   (acol);
  int acp     =         abs(acpower);
  M5.Lcd.print          (acp);
}

void infoscreen       (int i,String label,float f1,float f2){
  M5.Lcd.fillScreen   (TFT_BLACK);
  if (isCharging)     {M5.Lcd.setTextColor(laadKleur[brightness]);}else{M5.Lcd.setTextColor(ontlaadKleur[brightness]);}
  if (batteryPercentage==100){M5.Lcd.setTextColor(rustKleur[brightness]);}
  M5.Lcd.setFreeFont  (&FreeSansBold18pt7b);
  M5.Lcd.setTextSize  (2);
  int TXTX      =     0;
  int TXTX2     =     200;
  int TXTY[3]   =     {70,140,210};
  M5.Lcd.setCursor    (TXTX,TXTY[0]);
  M5.Lcd.print        (abs(i));
  M5.Lcd.setCursor    (TXTX2,TXTY[0]);
  M5.Lcd.print        (label);
  M5.Lcd.setCursor    (TXTX,TXTY[1]);
  M5.Lcd.print        (f1,0);
  M5.Lcd.setCursor    (TXTX2,TXTY[1]);
  M5.Lcd.print        (label);
  M5.Lcd.setCursor    (TXTX,TXTY[2]);
  if (batteryPercentage==100){
    M5.Lcd.print      ("VOL! ;-)");  
  } else { 
    if (batteryPercentage==0){
      M5.Lcd.print    ("LEEG :-(");
    } else {
    //String tijdString = formatTimeHHMM(time_to_empty);
      Serial.println    (tijdString);  // print bv. "6:57"
      M5.Lcd.print      (tijdString);
      M5.Lcd.setCursor  (TXTX2,TXTY[2]);
    //M5.Lcd.print      (" h.");
      M5.Lcd.print      ("h.");
    //M5.Lcd.print      (f2,0);
    //M5.Lcd.print      (" min.");
    }
  }
}

void updateDisplayTXT() {
  M5.Lcd.fillRect     (130,0,320-130,240,TFT_BLACK);  // Wis tekst gebied
  getColor();         // Bepaal kleur op basis van status (consistent met batterij)
  int TRIN[6]   =     {120,120    ,120+30,120-20  ,120+30,120+20};
  int TROT[6]   =     {120,190-20 ,120+30,190     ,120,190+20};
  int TXTX      =     160;
  int TXTY[3]   =     {70,140,210};
  M5.Lcd.setTextColor (Bcolor);  // Toon percentage in dezelfde kleur als batterij
  M5.Lcd.setFreeFont  (&FreeSansBold18pt7b);
  M5.Lcd.setTextSize  (2);
  M5.Lcd.setCursor    (TXTX,TXTY[0]);
  if                  (batteryPercentage==100){M5.Lcd.setTextColor(rustKleur[brightness]);}
  if                  (batteryPercentage >= 99) {
    M5.Lcd.printf     ("%.0f", batteryPercentage);
  } else {
    M5.Lcd.printf     ("%.0f%%", batteryPercentage);
  }
  M5.Lcd.setTextColor (laadKleur[brightness]);
  M5.Lcd.setCursor    (TXTX,TXTY[1]);
  M5.Lcd.printf       ("%.1f", kWhIn);
  M5.Lcd.setTextColor (ontlaadKleur[brightness]);
  M5.Lcd.setCursor    (TXTX,TXTY[2]);
  M5.Lcd.printf       ("%.1f", kWhOut);
  M5.Lcd.fillTriangle (TRIN[0],TRIN[1],TRIN[2],TRIN[3],TRIN[4],TRIN[5],laadKleur[brightness]); 
  M5.Lcd.fillTriangle (TROT[0],TROT[1],TROT[2],TROT[3],TROT[4],TROT[5],ontlaadKleur[brightness]); 
}

String httpGETRequest(String serverName) {
  WiFiClient client;
  HTTPClient http;
  http.begin             (client, serverName);
  httpCode     = http.GET();
  payload              = "--"; //?  
  if (httpCode>0) {
      //Serial.print     ("HTTP Response code: ");
      //Serial.println   (httpCode);
    payload            =  http.getString();
  }
  else {
    Serial.print          ("Error code: ");
    Serial.print          (httpCode);
    Serial.print          (" voor ");
    Serial.println        (serverName);
  }  
  // Free resources
  http.end();
  return payload;
}

void CatchTheSun(){
  bullet(TFT_YELLOW);
  String                    power;
  if  (WiFi.status()==      WL_CONNECTED ){     
    Serial.print            ("Sun now   ");
    power         =         httpGETRequest(serverNameCur);
    if (httpCode<0)         {return;}
    Sun_now       =         power.toFloat();
    Serial.println          (Sun_now);
    Serial.print            ("Sun today ");
    power         =         httpGETRequest(serverNameTod);
    Sun_today     =         power.toFloat();
    Serial.println          (Sun_today);
  }
  bullet(TFT_BLACK);
}

void bullet (uint16_t kleur){
  M5.Lcd.fillCircle         (300,20,10,kleur);
}

void GetSocketP1()      { 
  Serial.println        ("GetSocketP1()");
  error   = false;
  no_data = true;
  if (WiFi.status() ==    WL_CONNECTED) {
    HTTPClient http;
  //Serial.println        ("http.begin"+P1Server);
    http.begin            (client,P1Server);
    httpCode     =        http.GET();                                   // start connection and send HTTP header.
  //if                    (httpCode!=200){return;}
  //Serial.println        ("httpCode:"+String(httpCode));

    if (httpCode ==       HTTP_CODE_OK) {                             // file found at server. 
    //Serial.println      ("OK");
      payload =           http.getString();
    //Serial.println      ("Response payload:");
      Serial.println      (payload);
      DeserializationError error =    deserializeJson(doc, payload);
      if (error)          {
        Serial.print      ("deserializeJson() failed: ");
        Serial.println    (error.f_str());
      } else { 
      // get the data from P1 meter
        topower        =  doc["total_power_export_kwh"];
        acpower        =  doc["active_power_w"];
        tipower        =  doc["total_power_import_kwh"];
        Serial.println    ("active_power_w : "  + String(acpower));
        Serial.println    ("total_power in : "  + String(topower));
        Serial.println    ("total_power out: "  + String(tipower));
        no_data  =        false;
      }                   // end of no error  
    }                     // end of HTTP_CODE_OK
    http.end(); 
  }                       // end of WL_CONNECTED
}