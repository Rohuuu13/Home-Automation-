#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <FS.h>  // Include the SPIFFS file system
#include <WiFiUdp.h>
#include <NTPClient.h>



//-------------time kiping --------------------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // IST (UTC +5:30)
// WiFi credentials
const char* ssid = "Dark_Spy";
const char* password = "Rohit@123";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "paulrohit@582#";

// GPIO pins for 5 switches
int latch=D8;       // for latch relay power ckt.
int switchPins[] = {D0, D3, D4, D5};
int switchStates[] = {0,0,0,0};
byte phb=D1;   //For Feedback/Voltage Sensing-------------------
//----------------------== light control Switch ==------------------
/* the program and structure is made for fan control and voltage detection,
    with phase pulse detection technic this work isolately with optocuplers in positive half cycle
    and take all data from it.
    how much time it is off and on.
    the class can detect only conduction time of optocupler diode 
    implement voltage multiplecation vector to get output voltage
*/


//-----------------------private class--------------------------

class Voltage{
  private:
    byte _pin,_mxlp,_prc=1;
    unsigned int _mxdt[10];
    unsigned long bnm,azx,&pnt,sas;
    unsigned int &pd,tot;
    float edc=0.00;
    void matrix(){
      _mxdt[_mxlp]=pd;
      for(unsigned int i : _mxdt){
        edc = (edc+i)/2;
      }
      if(_mxlp==9){
        _mxlp=0;
      }
      else{
        _mxlp++;
      }
      volt=edc;
      edc=0.00;
    }
  public:
    float volt;
    Voltage(byte pin, unsigned int &vtg, unsigned long &point): _pin(pin),pd(vtg),pnt(point) {
      pinMode(_pin,INPUT_PULLUP);
      azx=micros();
    }
    void run(){
      switch(_prc){
        case 1:
          if(digitalRead(_pin)==LOW){
            bnm=micros();
            tot=bnm-azx;
            azx=bnm;
            _prc=2;
          }
          break;
        case 2:
          if(digitalRead(_pin)==HIGH){
            sas=micros();
            pd=sas-azx;
            pnt=((tot-(2*pd))/4)+sas;   // for phase deteacting
            _prc=1;
            //matrix();           // equivalent voltage calculation
          }
          break;
        default:
          break;
      }
      if((micros()-bnm)>30000){
        bnm=micros();
        pd=0;
        tot=0;
      }

    }
};

class Fan{
  private:
    byte _pin,_lol=1;
    unsigned long pn2, &pnt;
    unsigned int &vol;
  public:
    byte spd=0;         // speed dertermine in timening
    int pers;
    Fan(byte pin, unsigned int &vtg, unsigned long &point): _pin(pin),vol(vtg),pnt(point) {
      pinMode(_pin,OUTPUT);
    }
  void run(){
    if(spd != 0 and vol!=0){
      switch(_lol){
        case 1:
          if(micros()> pnt+(spd*100)){
            digitalWrite(_pin,HIGH);
            _lol=2;
            pn2=10+pnt+(spd*100);
          }
          break;
        case 2:
          if(micros()>=pn2){
            digitalWrite(_pin,LOW);
            _lol=3;
            pn2+=9995;
          }
          break;
        case 3:
          if(micros()>= pn2){
            digitalWrite(_pin,HIGH);
            _lol=4;
            pn2+=10;
          }
          break;
        case 4:
          if(micros()>=pn2){
            digitalWrite(_pin,LOW);
            _lol=1;
          }
          break;
        default:
          digitalWrite(_pin,LOW);
          break;
      }
    }
    else{
      digitalWrite(_pin,LOW);
      _lol=1;
    }
  }
  void speed_dn(){ spd= spd<100 ? spd++ :spd ;}
  void speed_up(){spd--;}
  void RGB(byte val){
    spd= val ? 85-int(val*0.75) : 0 ;
    pers=val;
  }
};
//-------the file define for libreary with declereaction ---------------------


//---------global variable----------------
unsigned int vst;
unsigned long sdf;
//----------------------------class variable------------------
Voltage vol(phb,vst,sdf);
Fan LED1(D2,vst,sdf);
Fan LED2(D6,vst,sdf);
Fan LED3(D7,vst,sdf);

void writeData() {
    File file = SPIFFS.open("/lampos.txt", "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    
    for (int i = 0; i < 4; i++) {
      file.println(switchStates[i]) ;
        //file.println(values[i]);  // Store each integer on a new line
    }
    file.println(LED1.pers);
    file.println(LED2.pers);
    file.println(LED3.pers);
    
    file.close();
    Serial.println("Data written successfully!");
}

void readData() {
    File file = SPIFFS.open("/lampos.txt", "r");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.println("Stored values:");
    /*while (file.available()) {
        Serial.println(file.readStringUntil('\n').toInt());  // Read each integer
    }*/
    for (int i = 0; i < 4; i++) {
      switchStates[i]=file.readStringUntil('\n').toInt() ;
        //file.println(values[i]);  // Store each integer on a new line
    }
    LED1.RGB(file.readStringUntil('\n').toInt());
    LED2.RGB(file.readStringUntil('\n').toInt());
    LED3.RGB(file.readStringUntil('\n').toInt());
    file.close();
}

//---------------------coding of webserver ---------------
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

void setup() {
  Serial.begin(115200);
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS Mount Failed!");
    return;
  }
  // Initialize all switch pins as output
  pinMode(latch, OUTPUT);
  digitalWrite(latch, LOW);
  for (int i = 0; i < 4; i++) {
    pinMode(switchPins[i], OUTPUT);
    digitalWrite(switchPins[i], LOW);
  }
  // Connect to WiFi
  WiFi.hostname("RoEsP");
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
   Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
   digitalWrite(latch,HIGH);
   readData();
   for(byte i=0;i<4;i++){
    digitalWrite(switchPins[i],switchStates[i] ? HIGH :LOW );
   }
  // HTML Page Route
  server.on("/", []() {
    String trx=getHTML();
    trx.replace("$ch0",switchStates[0]? "checked":" ");
    trx.replace("$ch1",switchStates[1]? "checked":" ");
    trx.replace("$ch2",switchStates[2]? "checked":" ");
    trx.replace("$ch3",switchStates[3]? "checked":" ");
    trx.replace("$sl0",String(LED1.pers));
    trx.replace("$sl1",String(LED2.pers));
    trx.replace("$sl2",String(LED3.pers));
    server.send(200, "text/html", trx);
  });

  // Light control route: /light/light1/on or /off
  server.onNotFound([]() {
    String path = server.uri();
    Serial.println("Handling: " + path);

    if (path.startsWith("/light/")) {
      handleLight(path);
    } else if (path.startsWith("/fan/")) {
      handleFan(path);
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });
  httpUpdater.setup(&server, update_path, update_username, update_password);

  server.begin();
  Serial.println("HTTP server started");
}
unsigned long jkl=millis();
int hour;
void loop() {
  server.handleClient();
  vol.run();
  LED1.run();
  LED2.run();
  LED3.run();
  if((millis()-20000)>jkl){
    jkl=millis();
    hour = timeClient.getHours();
    if(switchStates[1]){
      if(hour >=6 && hour <10){
        switchStates[1]=0;
        digitalWrite(switchPins[1], switchStates[1]);
      }
    }
    else{
      if(hour >= 18){
        switchStates[1]=1;
        digitalWrite(switchPins[1], switchStates[1]);
      }
    }
  }
}

// ========== HTML PAGE ==========
String getHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Rohuuu_ESP</title>
  <style>
    body {
      background-color: black;
      color: white;
      font-family: Arial, sans-serif;
      text-align: center;
      padding: 20px;
    }
    .switch-board {
      max-width: 400px;
      margin: auto;
      background: #1c1c1c;
      padding: 20px;
      border-radius: 15px;
      box-shadow: 0 4px 10px rgba(255,255,255,0.2);
    }
    h2 {
      margin-bottom: 20px;
    }
    .toggle, .regulator {
      margin: 15px 0;
      display: flex;
      justify-content: space-between;
      align-items: center;
      color: white;
    }
    label {
      font-size: 1.1em;
    }

    /* Toggle Switch Style */
    .switch {
      position: relative;
      display: inline-block;
      width: 50px;
      height: 24px;
    }
    .switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0; left: 0;
      right: 0; bottom: 0;
      background-color: rgb(111, 172, 111);
      transition: .4s;
      border-radius: 34px;
    }
    .slider:before {
      position: absolute;
      content: "";
      height: 18px;
      width: 18px;
      left: 3px;
      bottom: 3px;
      background-color: white;
      transition: .4s;
      border-radius: 50%;
    }
    input:checked + .slider {
      background-color: hsl(106, 82%, 51%);
    }
    input:checked + .slider:before {
      transform: translateX(26px);
    }

    /* Fan Regulator Style */
    input[type="range"] {
      width: 150px;
      height: 12px;
      background: rgb(111, 172, 111);
      border-radius: 6px;
      outline: none;
      -webkit-appearance: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: rgb(244, 248, 244);
      border: none;
      margin-top: 0px;
    }
    
  </style>
  <script>
    function toggleSwitch(lightId) {
      const checkbox = document.getElementById(lightId);
      const status = checkbox.checked ? "on" : "off";
      fetch(`/light/${lightId}/${status}`);
    }

    function updateFan(fanId) {
      const value = document.getElementById(fanId).value;
      if(fanId=="fan1"){
        document.getElementById("lbl1").innerText=value;
      }
      else if(fanId=="fan2"){
        document.getElementById("lbl2").innerText=value;
      }
      else if(fanId=="fan3"){
        document.getElementById("lbl3").innerText=value;
      }
      fetch(`/fan/${fanId}/${value}`);
    }
  </script>
</head>
<body>
  <div class="switch-board">
    <h2>Rohuuu ESP</h2>

    <div class="toggle">
      <label>Pin 1</label>
      <label class="switch">
        <input type="checkbox" $ch0 id="light1" onchange="toggleSwitch('light1')">
        <span class="slider"></span>
      </label>
    </div>
    <div class="toggle">
      <label>Pin 2</label>
      <label class="switch">
        <input type="checkbox" $ch1 id="light2" onchange="toggleSwitch('light2')">
        <span class="slider"></span>
      </label>
    </div>
    <div class="toggle">
      <label>Pin 3</label>
      <label class="switch">
        <input type="checkbox" $ch2 id="light3" onchange="toggleSwitch('light3')">
        <span class="slider"></span>
      </label>
    </div>
    <div class="toggle">
      <label>Pin 4</label>
      <label class="switch">
        <input type="checkbox" $ch3 id="light4" onchange="toggleSwitch('light4')">
        <span class="slider"></span>
      </label>
    </div>

    <div class="regulator">
      <label>Level 1</label>
      <label style="color: rgb(255, 59, 59);" id="lbl1">$sl0 </label>
      <input type="range" min="0" max="100" value="$sl0" step="5" id="fan1" oninput="updateFan('fan1')">
    </div>
    <div class="regulator">
      <label>Level 2</label>
      <label style="color: rgb(17, 246, 5);" id="lbl2">$sl1 </label>
      <input type="range" min="0" max="100" value="$sl1" step="5" id="fan2" oninput="updateFan('fan2')">
    </div>
    <div class="regulator">
      <label>Level 3</label>
      <label style="color: rgb(29, 176, 255);" id="lbl3">$sl2 </label>
      <input type="range" min="0" max="100" value="$sl2" step="5" id="fan3" oninput="updateFan('fan3')">
    </div>
  </div>
</body>
<script>
  document.addEventListener("keydown", function(event) {
      if (event.key == "1") { // Change this to any key you want
          document.getElementById("light1").click();
      }
      else if (event.key == "2") { // Change this to any key you want
          document.getElementById("light2").click();
      }
      else if (event.key == "3") { // Change this to any key you want
          document.getElementById("light3").click();
      }
      else if (event.key == "4") { // Change this to any key you want
          document.getElementById("light4").click();
      }
  });
</script>
</html>
)rawliteral";
}

// ========== HANDLE LIGHT ==========
void handleLight(String path) {
  if (path.indexOf("light1") >= 0) {
    switchStates[0]=path.indexOf("on") > 0 ? 1 : 0;
    digitalWrite(switchPins[0], switchStates[0]);
    writeData();
  }
  else if (path.indexOf("light2") >= 0) {
    switchStates[1]=path.indexOf("on") > 0 ? 1 : 0;
    digitalWrite(switchPins[1], switchStates[1]);
    writeData();
  }
  else if (path.indexOf("light3") >= 0) {
    switchStates[2]=path.indexOf("on") > 0 ? 1 : 0;
    digitalWrite(switchPins[2], switchStates[2]);
    writeData();
  }
  else if (path.indexOf("light4") >= 0){
   switchStates[3]=path.indexOf("on") > 0 ? 1 : 0;
    digitalWrite(switchPins[3], switchStates[3]);
    writeData();
  }
  server.send(200, "text/plain", "OK");
}

// ========== HANDLE FAN ==========
void handleFan(String path) {
  byte value = path.substring(path.lastIndexOf("/") + 1).toInt();
  //int pwm = map(value, 0, 5, 0, 1023);   convert level to PWM
 
  if (path.indexOf("fan1") >= 0) {
    LED1.RGB(value);
    writeData();
  }
  else if (path.indexOf("fan2") >= 0){
    LED2.RGB(value);
    writeData();
  }
  else if (path.indexOf("fan3") >= 0){
    LED3.RGB(value);
    writeData();
  }

  server.send(200, "text/plain", "OK");
}          