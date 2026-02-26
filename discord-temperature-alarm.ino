#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <Wire.h>
#include <Adafruit_AHTX0.h>

/* 
TODO:
  Discord WebSocket (Gateway API)
  Add sensor label to msg (i.e.: "Basement 1", "Room 1", "Homebrew ambient")
  Sliding window average?
  Logs
  Read errors
  Connection errors
  Setting change confirmation msg (green)

Maybe:
  Temp in discord msg title
  Remove "Temperature Log" from msg title

Commands:
  Get Temperature

  Future:
    Set Reporting Interval (Normal, Warning, and Error)
    Set Target
    Set Range (Warning, Error High, and Error Low) [Maybe break alarm bound into separate command]
    Get Info (Current Setting Values and Valid Commands)

  Maybes:
    Silence (Indef or for a Duration)
    Unsilence
    Set Wifi
*/


// #define UTC-5 -5*60*60
#define WEBHOOK "https://discord.com/api/webhooks/1470647908662640752/HklrpLbKj0ZXje0DRMSiwuuF3VjHbyau5T-UFJStuo0ZQ64c1S5j69XyUZ5ZudgZMLwM"
// #define SSID "Hi (3)"
// #define KEY "12345671"

#define SSID "36Albion"
#define KEY "LigmaChops24"

#define DISCORD_BLUE    32767
#define DISCORD_GREEN   65280
#define DISCORD_YELLOW  16760576
#define DISCORD_RED     16711680

// #define LABEL "Homebrew (L)"
// #define SENSOR_TYPE "DS18B20"   //DS18B20 or AHT21
// #define TEMP_PRECISION .01      // °F

#define LABEL "Homebrew (A)"
#define SENSOR_TYPE "AHT21"   //DS18B20 or AHT21
#define TEMP_PRECISION .01      // °F

int MSG_INTERVAL_ERROR  =    1 * 60; // 1 minute
int MSG_INTERVAL_WARN   =    5 * 60; // 5 minutes
int MSG_INTERVAL        =   15 * 60; // 15 minutes

double TEMP_HIGH_WARN   = 82; 
double TEMP_LOW_WARN    = 59; 

double TEMP_HIGH_ERROR  = 90; 
double TEMP_LOW_ERROR   = 55; 

static unsigned long lastTempMsg = 0;
static unsigned long lastTempWarn = 0;
static unsigned long lastTempError = 0;


WiFiClientSecure client;
HTTPClient https;
// WiFiUDP ntpUDP;

static time_t now;
const char* tz = "EST5EDT,M3.2.0/2,M11.1.0/2";
// NTPClient timeClient(ntpUDP, "north-america.pool.ntp.org", 19*60*60, 6000);

// AHT21
// Create sensor object
Adafruit_AHTX0 aht;

// DS18B20
// GPIO where the DS18B20 is connected to
const int oneWireBus = 12;     
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);


void setup() {
  Serial.begin(115200);
  // while (!Serial); // Wait for serial port to connect

  //################
  //## WIFI Setup ##
  //################
  connectWIFI();

  //####################
  //## NTP Time Setup ##
  //####################
  connectNTP();

  //###################
  //## Discord Setup ##
  //###################
  client.setInsecure();
  https.begin(client, WEBHOOK);
  sendDiscordConnect("Alarm Connected:");

  //##################
  //## Sensor Setup ##
  //##################
  initializeSensor();
}




void loop() {
  now = time(nullptr); // + (6*60 + 10)*60;
  // timeClient.update(); //TODO: update less frequently

  sensors_event_t humidity, temp;
  getTemperature(&humidity, &temp);

  double tempF = temp.temperature * 1.8 + 32;
  String msg = "\nTemperature: " + String(temp.temperature) + " °C  |  " + String(tempF) + " °F \nHumidity: " + String(humidity.relative_humidity) + " %";
  Serial.println(msg);

  if(tempF > TEMP_HIGH_ERROR || tempF < TEMP_LOW_ERROR){
    if(now > lastTempError + MSG_INTERVAL_ERROR){
      // Zero other timestamps so changes are immediately logged
      lastTempMsg = 0;    
      lastTempWarn = 0;
      lastTempError = now;
      sendDiscordTemperature("[ERROR] " + String(LABEL) + "  Temperature out of range!", DISCORD_RED, temp.temperature, humidity.relative_humidity);
    }

  } else if(tempF > TEMP_HIGH_WARN || tempF < TEMP_LOW_WARN){
    if(now > lastTempWarn + MSG_INTERVAL_WARN){
      // Zero other timestamps so changes are immediately logged
      lastTempMsg = 0;
      lastTempWarn = now;
      lastTempError = 0;
      sendDiscordTemperature("[WARNING] " + String(LABEL) + ":", DISCORD_YELLOW, temp.temperature, humidity.relative_humidity);
    }

  } else if(now > lastTempMsg + MSG_INTERVAL) {
    // Zero other timestamps so changes are immediately logged
    lastTempMsg = now;
    lastTempWarn = 0;
    lastTempError = 0;
    sendDiscordTemperature(String(LABEL) + ":", DISCORD_BLUE, temp.temperature, humidity.relative_humidity);
  }

  delay(1000); // Refresh every 1 second
}




void initializeSensor(){
  if(SENSOR_TYPE == "AHT21"){
      // Initialize I2C (SDA = D2/GPIO4, SCL = D1/GPIO5)
    if (!aht.begin()) {
      Serial.println("Could not find AHT21? Check wiring");
      while (1) delay(10);
    }
    Serial.println("AHT21 found");

  } else if (SENSOR_TYPE == "DS18B20"){
    // Start the DS18B20 sensor
    sensors.begin();

  } else sendFatalError("Sensor type '" + String(SENSOR_TYPE) + "' not supported");
}

void getTemperature(sensors_event_t* humidity, sensors_event_t* temp){
  if(SENSOR_TYPE == "AHT21"){
    aht.getEvent(humidity, temp); // Populate objects with fresh data
    
  } else if (SENSOR_TYPE == "DS18B20"){  
    sensors.requestTemperatures(); 
    temp->temperature = sensors.getTempCByIndex(0);
    // float temperatureF = sensors.getTempFByIndex(0);

  } else sendFatalError("IDEK how you got here. Sensor type '" + String(SENSOR_TYPE) + "' not supported");
}


void connectWIFI() {
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
}

void connectNTP(){
  Serial.println("NTP Server: ");
  configTime(tz, "pool.ntp.org", "time.nist.gov");
  for (time_t now = time (nullptr); now < 8 * 3600 * 2; now = time (nullptr)) delay (500);
  yield();
  // delay(500); //NOTE
  Serial.println("NTP Server Synchronized: " + getFormattedTime((uint32_t)  time (nullptr), false));
}

// void setupDiscord(){
//   timeClient.update();
//   client.setInsecure();
//   https.begin(client, WEBHOOK);
//   sendDiscordMsg("Alarm Connected: " + timeClient.getFormattedTime());
// }


  //##############################
  //## Discord Webhook Function ##
  //##############################

// Blue:      32767 (Regular logs)
// Green:     65280 (Get, setting change confirmation)
// Yellow:    16760576
// Red:       16711680
void sendFatalError(String errorMsg){
  Serial.println(errorMsg);
  sendDiscordMsg(errorMsg, DISCORD_RED);  
  // sendDiscordMsg("**ERROR:  **" + (String) errorCode + "   " + https.errorToString(errorCode), color);
  while(true) delay(10);
}

void sendDiscordMsg(String subContent, int color){
  String content = "";
  https.addHeader("Content-Type", "application/json");
  //TODO: Check iss for titles
  int code = https.POST("{\"content\":\"" + content + "\",\"embeds\": [{\"color\": " + (String) color + ", \"fields\": [{\"name\": \"" + subContent + "\", \"value\": \"\"\}] }],\"tts\":false}");
  Serial.println("Send Discord Msg POST Code: " + (String) code + "   " + https.errorToString(code));
}

void sendDiscordTemperature(String title, int color, double temp, double humidity){
  String fullTitle = "**" + title + "  " + String(temp * 1.8 + 32, 0) + "°F " + getFormattedTime() + "**\\u0000";
  String tempStr = String(temp * 1.8 + 32) + " °F" + "    |    " + String(temp) + " °C";
  String humidityStr = "\\u0000\"}";
  if(SENSOR_TYPE != "DS18B20") humidityStr = "\\n\\u0000\"}, {\"name\": \"Humidity\",\"value\": \"" + String(humidity) + " %\\u0000\"}";

  https.addHeader("Content-Type", "application/json");
  int code = https.POST("{\"content\":\"\",\"embeds\": [{\"title\": \"" + fullTitle + "\", \"color\": " + (String) color + ", \"fields\": [{\"name\": \"\",\"value\": \"" + tempStr + humidityStr + "] }],\"tts\":false}");

  Serial.println("Send Discord Temperature POST Code: " + (String) code + "   " + https.errorToString(code));
}


void sendDiscordConnect(String title){
  String fullTitle = "**" + title + "  " + getFormattedTime() + "**\\u0000";

  // Messaging Intervals
  String nominalMsgIntStr = "Nominal:  " + String(MSG_INTERVAL) + "s";
  String warningMsgIntStr = "Warning:  " + String(MSG_INTERVAL_WARN) + "s";
  String errorMsgIntStr   = "Error:\\u00A0\\u00A0\\u00A0\\u00A0\\u00A0\\u00A0\\u00A0 " + String(MSG_INTERVAL_ERROR) + "s";
  String msgIntervalsStr  =  nominalMsgIntStr + "\\n" + warningMsgIntStr + "\\n" + errorMsgIntStr;

  // Alert Temperature Ranges
  String nominalRangeStr  = "Nominal:  " + String((int) TEMP_LOW_WARN) + "-" + String((int) TEMP_HIGH_WARN) + "°F";
  String warningRangeStr  = "Warning:  " + String((int) TEMP_LOW_ERROR) + "-" + String(TEMP_LOW_WARN - TEMP_PRECISION) + "°F  or  "  + String(TEMP_HIGH_WARN + TEMP_PRECISION) + "-" + String((int) TEMP_HIGH_ERROR) + "°F";
  String errorRangeStr    = "Error:\\u00A0\\u00A0\\u00A0\\u00A0\\u00A0\\u00A0 <"   + String((int) TEMP_LOW_ERROR) + "°F  or  >"  + String((int) TEMP_HIGH_ERROR) + "°F";
  String alertRangeStr    = nominalRangeStr + "\\n" + warningRangeStr + "\\n" + errorRangeStr;

  https.addHeader("Content-Type", "application/json");
  int code = https.POST("{\"content\":\"\",\"embeds\": [{\"title\": \"" + fullTitle + "\", \"color\": " + (String) DISCORD_GREEN + ", \"fields\": [{\"name\": \"***" + LABEL + "***\",\"value\": \"_" + SENSOR_TYPE + "_\\n\\u0000\"}, {\"name\": \"Messaging Intervals\",\"value\": \"" + msgIntervalsStr + "\\n\\u0000\"}, {\"name\": \"Alert Temperature Ranges\",\"value\": \"" + alertRangeStr + "\\u0000\"} ] }],\"tts\":false}");

  Serial.println("Send Discord Temperature POST Code: " + (String) code + "   " + https.errorToString(code));
}


  //########################
  //## NTP Time Functions ##
  //########################

String getFormattedTime() {
  now = time(nullptr); // + (6*60 + 10)*60;
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  return formatDateTime(timeinfo);
}

String formatDateTime(const struct tm& t)
{
  char buffer[25];  // Enough for "YYYY-MM-DD HH:MM:SS"
  
  snprintf(buffer, sizeof(buffer),
           " %02d/%02d  %02d:%02d:%02d",
           t.tm_mon + 1,
           t.tm_mday,
          //  t.tm_year + 1900 - 2000,
           t.tm_hour,
           t.tm_min,
           t.tm_sec);

  return String(buffer);
}

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
