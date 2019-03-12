#include <DS18B20.h>
#include <Adafruit_SSD1306.h>
#include <MQTT.h>
#include <AssetTracker.h>
#include <Particle.h>
#include <JsonParserGeneratorRK.h>

int transmittingData = 1;
long lastPublish = 0;
int delayMinutes = 10;
AssetTracker t = AssetTracker();
FuelGauge fuel;

#define PARTICLE_KEEPALIVE 15

Adafruit_SSD1306 oled(-1);

const int MAXRETRY = 4;
const uint32_t msSAMPLE_INTERVAL = 2500;
const uint32_t msMETRIC_PUBLISH  = 30000;

DS18B20 ds18b20(D2, true);
char szInfo[64];
double lat;
double lon;
double batt;
float celsius;
float fahrenheit;
uint32_t msLastMetric;
uint32_t msLastSample;
int tst;

long last_fix;
double SoC;
void callback(char* topic, byte* payload, unsigned int length);
byte PPServer[] = {128, 199, 157, 0 };

MQTT client(PPServer, 1883, callback);

void send_data(){
  t.updateGPS();
  lat = t.readLatDeg();
  lon = t.readLonDeg();
  batt = fuel.getSoC();
  JsonWriterStatic<256> jw;
  {
    JsonWriterAutoObject obj(&jw);
    jw.insertKeyValue("t", "p");
    jw.insertKeyValue("acc", 10);
    jw.insertKeyValue("_type", "location");
    jw.insertKeyValue("lon", lon);
    jw.insertKeyValue("lat", lat);
    jw.insertKeyValue("batt", batt);
  }
  if (strcmp(jw.getBuffer(), "{\"t\":\"p\",\"acc\":10,\"_type\":\"location\",\"lon\":lon,\"lat\":lat,\"batt\":batt}")) {
    client.publish("owntracks/bhell/gps", jw.getBuffer());
  }
  client.publish("homeassistant/bhell/ds18b20/celsius", String(celsius,2));
  client.publish("homeassistant/bhell/ds18b20/fahrenheit", String(fahrenheit,2));
  client.publish("homeassistant/bhell","online");
}

void callback(char* topic, uint8_t* payload, unsigned int length) {
  char p[length + 1];
  memcpy(p, payload, length);
  p[length] = NULL;
  String message(p);
  payload[length] = '\0';
  String strPayload = String((char*)payload);
  if (strPayload == "online") {
    send_data();
    client.publish("homeassistant/bhell/test","Request");
  }
}

void setup() {
  Particle.keepAlive(PARTICLE_KEEPALIVE);
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.clearDisplay();
  //client.connect("mqtt_bfish");
  client.connect("mqtt_bhell");
  if (client.isConnected()) {
    //client.publish("homeassistant/bfish","online");
    client.publish("homeassistant/bhell","online");
    client.subscribe("homeassistant/bhell/status/set");
  }
  t.begin();
  t.gpsOn();
  Serial.begin(9600);
  //timer.start();
  Particle.function("tmode", transmitMode);
  Particle.function("batt", batteryStatus);
  Particle.function("gps", gpsPublish);
}



void loop() {
  if (client.isConnected()){
    client.loop();
  }
  getGPS();
  getTemp();
  displayOled();
  oled.display();
  if(millis() - lastPublish > 5 * 60 * 1000){
    lastPublish = millis();
    send_data();
  }
}


void getTemp(){
  float _temp;
  int   i = 0;
  do {
    _temp = ds18b20.getTemperature();
  }
  while (!ds18b20.crcCheck() && MAXRETRY > i++);
  if (i < MAXRETRY) {
    celsius = _temp;
    fahrenheit = ds18b20.convertToFahrenheit(_temp);
  }
  else {
    celsius = fahrenheit = NAN;
    Serial.println("Invalid reading");
  }
}

void displayOled(){
  if(isnan(celsius) || isnan(fahrenheit)) {
    oled.clearDisplay();
    oled.setTextColor(WHITE);
    oled.setTextSize(2);
    oled.setCursor(5, 0);
    oled.print('Temp Error!');
    return;
  }
  oled.clearDisplay();
  oled.setTextColor(WHITE);
  oled.setTextSize(2.7);
  oled.setCursor(25, 15);
  oled.print(celsius);
  oled.print(" C");
  oled.setCursor(25, 35);
  oled.print(fahrenheit);
  oled.print(" F");
}

int transmitMode(String command) {
    transmittingData = atoi(command);
    return 1;
}

int gpsPublish(String command) {
    if (t.gpsFix()) {
        Particle.publish("G", t.readLatLon(), 60, PRIVATE);
        return 1;
    } else {
      return 0;
    }
}

void getGPS(){
  t.updateGPS();
  if(millis() - lastPublish > 5 * 60 * 1000){
    lastPublish = millis();
    if (t.gpsFix()) {
      lat = t.readLatDeg();
      lon = t.readLonDeg();
      batt = fuel.getSoC();
      JsonWriterStatic<256> jw;
      {
        JsonWriterAutoObject obj(&jw);
        jw.insertKeyValue("t", "p");
        jw.insertKeyValue("acc", 10);
        jw.insertKeyValue("_type", "location");
        jw.insertKeyValue("lon", lon);
        jw.insertKeyValue("lat", lat);
        jw.insertKeyValue("batt", batt);
      }
      if (strcmp(jw.getBuffer(), "{\"t\":\"p\",\"acc\":10,\"_type\":\"location\",\"lon\":lon,\"lat\":lat,\"batt\":batt}")) {
        //client.publish("owntracks/bfish/gps", jw.getBuffer());
        client.publish("owntracks/bhell/gps", jw.getBuffer());
      }
    }
  }
}

int batteryStatus(String command){
    Particle.publish("B",
          "v:" + String::format("%.2f",fuel.getVCell()) +
          ",c:" + String::format("%.2f",fuel.getSoC()),60, PRIVATE
    );
    if (fuel.getSoC()>10){ return 1;}
    else { return 0;}
}
