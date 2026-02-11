#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
/*
Wifi
Time
Discord web hooks (send and receive)
Aht21 or DS18B20

No:
  Screen
  Battery? (Lipo + Charger + Regulator)

Commands:
  Set Reporting Interval (Normal, Warning, and Error)
  Set Target
  Set Range (Warning, Error High, and Error Low) [Maybe break alarm bound into separate command]
  Get Temperature
  Get Info (Current Setting Values and Valid Commands)

  Maybes:
    Silence (Indef or for a Duration)
    Unsilence

Features:
  Sliding window average?
  Timestamps on messages
  Logs
  Read errors
  Setting change confirmation
*/


// #define UTC-5 -5*60*60
#define WEBHOOK "https://discord.com/api/webhooks/1470647908662640752/HklrpLbKj0ZXje0DRMSiwuuF3VjHbyau5T-UFJStuo0ZQ64c1S5j69XyUZ5ZudgZMLwM"
#define SSID "Hi (3)"
#define KEY "12345671"

WiFiClientSecure client;
HTTPClient https;
WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "north-america.pool.ntp.org", 19*60*60, 6000);
// static time_t now;

// Create sensor object
Adafruit_AHTX0 aht;

void setup() {
  Serial.begin(115200);
  Serial.println("AHT21 Test\n\n");

  //################
  //## WIFI Setup ##
  //################

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, KEY);
  Serial.print("Connecting WiFi: ");
  for(int i=0; WiFi.status() != WL_CONNECTED; i++){
    Serial.print("*");
    delay(250);
  } 
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  Serial.println("\n\nWiFi Connected");


  //####################
  //## NTP Time Setup ##
  //####################
  Serial.println("NTP Server: ");
  timeClient.begin();

  // configTime("GMT0", "pool.ntp.org", "time.nist.gov");
  // for (time_t now = time (nullptr); now < 8 * 3600 * 2; now = time (nullptr)) delay (500);
  yield();
  delay(500); //NOTE
  // Serial.println("NTP Server Synchronized: " + getFormattedTime((uint32_t)  time (nullptr), false));

  client.setInsecure();
  https.begin(client, WEBHOOK);
  sendDiscord("Alarm Connected: " + timeClient.getFormattedTime(), 65280);


  // Initialize I2C (SDA = D2/GPIO4, SCL = D1/GPIO5)
  if (!aht.begin()) {
    Serial.println("Could not find AHT21? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT21 found");
}

void loop() {
  // now = time(nullptr); // + (6*60 + 10)*60;
  timeClient.update(); //TODO: update less frequently

  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp); // Populate objects with fresh data
  
  String msg = "\nTempurature: " + String(temp.temperature) + " °C  |  " + String(temp.temperature* 1.8 + 32) + " °F \nHumidity: " + String(humidity.relative_humidity) + " %";

  Serial.println(msg);
  sendDiscord(msg, 65280);

  delay(2000); // Read every 2 seconds
}

void connectWIFI() {
//TODO
}

// void setupDiscord(){
//   timeClient.update();
//   client.setInsecure();
//   https.begin(client, WEBHOOK);
//   sendDiscord("Alarm Connected: " + timeClient.getFormattedTime());
// }


  //##############################
  //## Discord Webhook Function ##
  //##############################

// Green:     65280
// Yellow:    16760576
// Red:       16711680
void sendErrorDiscord(int errorCode, int color){
  sendDiscord("**ERROR:  **" + (String) errorCode + "   " + https.errorToString(errorCode), color);
}

void sendDiscord(String subContent, int color){
  String content = "";
  bool a = https.begin(client, WEBHOOK);
  https.addHeader("Content-Type", "application/json");
  //TODO: Check iss for titles
  int code = https.POST("{\"content\":\"" + content + "\",\"embeds\": [{\"color\": " + (String) color + ", \"fields\": [{\"name\": \"" + subContent + "\", \"value\": \"\"\}] }],\"tts\":false}");
  Serial.println("Send Discord POST Code: " + (String) code + ", " + https.errorToString(code));
}


  //########################
  //## NTP Time Functions ##
  //########################

String getFormattedTime(unsigned long rawTime) {
  return getFormattedTime(rawTime, false);
}

String getFormattedTime(unsigned long rawTime, bool isDur) {
  unsigned long hours = rawTime / 3600;
  if(!isDur) hours %= 24;
  String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

  unsigned long minutes = (rawTime % 3600) / 60;
  String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

  unsigned long seconds = rawTime % 60;
  String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

  return isDur ? (hoursStr + "h " + minuteStr + "m " + secondStr + "s") : (hoursStr + ":" + minuteStr + ":" + secondStr);
}
