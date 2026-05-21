/*
 * Author: Ernesto Tolocka (Profe Tolocka)
 * Date: 2026-May-17
 * Description: Displays air quality values from Home Assistant.
 * Notes: Uses the EE05 board and a 4.26-inch monochrome EPD
 * 
 * License: MIT
 */

#include <WiFi.h>
#include "TFT_eSPI.h"

//#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <ArduinoJson.h>    // Used to parse JSON

#include <esp_sleep.h>    // For deep sleep

#include "image.h"   // Background image

EPaper epaper;

// WiFi network name and password
const char* ssid = "LosToloNetwork";
const char* password = "performance15";

// Token provisto por Home Assistant
const char* Token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJlMmU2N2EzZDIyYjQ0ODU0YjA4ODYyZjg5ZDQwYzYyNSIsImlhdCI6MTc3ODk3MTAzMSwiZXhwIjoyMDk0MzMxMDMxfQ.UjmFtAjzu8UAGPmgjcGxcvusNUAwr422tri7F2RSYWg"; 

// Home Assistant endpoint
// Asegurarse que la IP no cambie
String endpoint = String("http://192.168.100.142:8123/api/states/sensor.calidad_de_aire_carbon_dioxide");


// Function to put the ESP32 into deep sleep
void sleepSeconds (uint32_t s) {

  if (s < 60) s = 60;    // Just in case

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  esp_sleep_enable_timer_wakeup((uint64_t)s * 1000000ULL);
  //esp_deep_sleep_start();
  delay ((uint64_t)s * 1000000ULL);
}

// Function to connect to WiFi with timeout
bool connectWiFi(uint32_t timeoutMs) {
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

String getHAState(String entity_id) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return "";
  }

  WiFiClient client;
  HTTPClient http;

  String url = "http://192.168.100.142:8123/api/states/" + entity_id;

  Serial.println();
  Serial.print("Requesting: ");
  Serial.println(url);

  http.setTimeout(1000); // 1 seconds

  if (!http.begin(client, url)) {
    Serial.println("http.begin() failed");
    return "";
  }

  http.addHeader("Authorization", String("Bearer ") + Token);
  http.addHeader("Content-Type", "application/json");

  //Serial.println (String("Bearer ")+Token);
  
  int httpCode = http.GET();

  Serial.print("HTTP code: ");
  Serial.println(httpCode);

  if (httpCode != HTTP_CODE_OK) {
    String errBody = http.getString();
    Serial.println("Error body:");
    Serial.println(errBody.substring(0, 200));

    http.end();
    return "";
  }

  String payload = http.getString();

  Serial.print("Payload length: ");
  Serial.println(payload.length());

  http.end();

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return "";
  }

  String value = doc["state"].as<String>();

  Serial.print("State value: ");
  Serial.println(value);

  return value;
}

void setup() {
  Serial.begin(115200);

  Serial.println ("Starting..");

  if (!connectWiFi(30000)) {
    Serial.println("WiFi did not connect. Going to sleep for 5 min.");
    sleepSeconds(300);
    return;
  }
  Serial.println("WiFi connected!");



  // Configure and initialize display
  epaper.begin();
  epaper.fillScreen(TFT_WHITE);          // Clear screen
  epaper.setTextFont (6); 
  //epaper.setFreeFont(&Yellowtail_32);    // Select font
  epaper.setRotation(0);            
  epaper.setTextDatum(MC_DATUM);         // Set centered alignment

  epaper.setTextSize (1);
  //epaper.drawString ("Air Quality ", epaper.width()/2,50);

  // Load background image
  epaper.drawBitmap(0, 0, background, 800, 480, TFT_BLACK);
  //epaper.pushImage (0,0, 800, 480, (uint16_t *) background);

  // Show everything
  epaper.update ();

  
  // Access Home Assistant REST API

// Debería leer 3 veces y si aun asi no lee nada, mostrar "-"

String temp = getHAState("sensor.calidad_de_aire_temperature");
String hum  = getHAState("sensor.calidad_de_aire_humidity");
String co2  = getHAState("sensor.calidad_de_aire_carbon_dioxide");
String hcho = getHAState("sensor.calidad_de_aire_formaldehyde");
String voc  = getHAState("sensor.calidad_de_aire_volatile_organic_compounds");

  //Ojo, si se produce un error http en las llamadas de arriba, se resetea el micro.
  //Falta alguna comprobacion
  
  //Test
  temp = "19";

  epaper.drawString (temp, 90, 290);
  epaper.drawString (hum,  244, 290 );
  epaper.drawNumber (co2.toInt(),  398, 290);
  epaper.drawString (hcho, 556, 290 );
  epaper.drawString (voc,  710, 290 );

  epaper.update ();

/*
  // Iterate through the JSON calculating day difference
  // The first one greater than 0 is the next holiday

  bool found = false; 

  for (int i = 0; i < doc.size(); i++) {
    const char* holiday = doc[i]["date"];
    const char* name = doc[i]["name"];

    Serial.println(holiday);
    Serial.println(name);

    int epochDif = stringDateToEpoch (holiday) - epochToday;
    Serial.println (epochDif);

    if (epochDif >= 0) {   // If it is today, show 0 days

      int daysDif = epochDif / 86400;  // Seconds in a day
      Serial.println (daysDif);
      String holidayName = doc[i]["name"];  // Use "name" or "localName"
   
      // Select size and font for number of days
      epaper.setTextSize (3);
      epaper.setTextFont (7);
    
      epaper.drawNumber (daysDif, epaper.width()/2, 220);

      epaper.setTextSize (1);
      epaper.setFreeFont(&Yellowtail_32);    // Select font

      // Show only the first 30 characters
      epaper.drawString (removeAccents(holidayName.substring (0,38)), epaper.width()/2, 314);
      epaper.update ();

      found = true;
      break;
    }
  }

  if (!found) {
    // There are no more holidays :(
    
    // Select size and font for number of days
    epaper.setTextSize (3);
    epaper.setTextFont (7);
    epaper.drawString ("--", epaper.width()/2, 220);

    epaper.setTextSize (1);
    epaper.setFreeFont(&Yellowtail_32);    // Select font
    epaper.drawString ("No More Holidays!", epaper.width()/2, 314);


    epaper.update ();
  }

  // Everything is ready, go to sleep until tomorrow

  Serial.println ("Going to sleep...");

  // Delay to allow access from the IDE if necessary.
  delay (10000);

  //sleepSeconds (seconds2Tomorrow);

  */

}

void loop() {

  // Does nothing

}