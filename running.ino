// 1. wifi connection
// 2. water flow sensor reading and volume calculation code
// 3. send data to firebase

#define WIFI_SSID "Pawan_127A"
#define WIFI_PASSWORD "9599892474pawan"
#define DATABASE_URL "https://drop-count-default-rtdb.firebaseio.com/"
#define API_KEY "AIzaSyDuA9HjVgpAM7-os84C0--u4tNlq7yjGko"
#define SENSOR  D4


#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Arduino.h>
#include <NTPClient.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#endif
#include <Firebase_ESP_Client.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"
#include "time.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

long currentMillis = 0;
long previousMillis = 0;
int interval = 1000;
//boolean ledState = LOW;
float calibrationFactor = 4.5;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;

const unsigned long intervalDate = 24UL * 60UL * 60UL * 1000UL;
long previousMillisDate = 0;
int sensor_value = 0;
String date = "";
String location = "";
bool first = true;
String domain = "https://raw.githubusercontent.com/dscmriirs/data/main/user1.json";
String data;
String user;
void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;


  pinMode(SENSOR, INPUT_PULLUP);
  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  previousMillis = 0;
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  }
  else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  //init and get the time

  data = httpGETRequest();
  JSONVar myObject = JSON.parse(data);

  // JSON.typeof(jsonVar) can be used to get the type of the var
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    return;
  }

  Serial.print("JSON object = ");
  Serial.println(myObject["username"]);
  user = myObject["username"];
  Serial.println(user);




  timeClient.begin();
  timeClient.setTimeOffset(19800);

}

void loop() {
  currentMillis = millis();
  unsigned long currentMillisDate = millis();
  if (currentMillis - previousMillis > interval) {
    pulse1Sec = pulseCount;
    pulseCount = 0;
    flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
    previousMillis = millis();
    flowMilliLitres = (flowRate / 60) * 1000;
    totalMilliLitres += flowMilliLitres;
    // Print the flow rate for this second in litres / minute
    Serial.print("Flow rate: ");
    Serial.print(int(flowRate));  // Print the integer part of the variable
    Serial.print("L/min");
    Serial.print("\t");       // Print tab space
    // Print the cumulative total of litres flowed since starting
    Serial.print("Output Liquid Quantity: ");
    Serial.print(totalMilliLitres);
    Serial.print("mL / ");
    Serial.print(totalMilliLitres / 1000);
    sensor_value = totalMilliLitres;
    Serial.println("L");
  }
  

  if (first || (currentMillisDate - previousMillisDate >=  intervalDate)) {
    // save the time you should have changed the date
    first = false;
    previousMillisDate += intervalDate;
    timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime ((time_t *)&epochTime);
    String monthDay = String(ptm->tm_mday);
    String currentMonth = String(ptm->tm_mon + 1);
    String currentYear = String(ptm->tm_year + 1900);

    String Day = (monthDay.length() == 1) ? "0" + monthDay : monthDay;
    String Month = (currentMonth.length() == 1) ? "0" + currentMonth : currentMonth;
    String currentDate = Day + "-" + Month + "-" + currentYear;

    location = user + "/" + currentDate;

  }


  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    // Write an Int number on the database path test/int
    const char *path = location.c_str();
    Serial.println(path);
    if (Firebase.RTDB.setInt(&fbdo, path, sensor_value)) {
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}
String httpGETRequest() {
  HTTPClient http;

  // Your IP address with path or Domain name with URL path



  WiFiClientSecure client;
  client.setInsecure(); //the magic line, use with caution
  client.connect("raw.githubusercontent.com", 443);

  http.begin(client, "https://raw.githubusercontent.com/dscmriirs/data/main/user1.json");


  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}
