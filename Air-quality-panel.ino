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

//#include "bitmap.h"   // Background image

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
  epaper.setFreeFont(&Yellowtail_32);    // Select font
  epaper.setRotation(0);            
  epaper.setTextDatum(MC_DATUM);         // Set centered alignment

  epaper.setTextSize (1);
  epaper.drawString ("Air Quality ", epaper.width()/2,50);

  // Load background image
  //epaper.drawBitmap(0, 0, holidayBack, 648, 480, TFT_BLACK);

  // Show everything
  epaper.update ();

  
  // Access Home Assistant REST API

  //WiFiClientSecure client;
  //client.setInsecure(); // Encrypted HTTPS, certificate not validated

  WiFiClient client;

  HTTPClient http;

  if (!http.begin(client, endpoint)) {
    Serial.println("http.begin() failed");
    sleepSeconds (300);
    return;    // Only for clarity
  }

  // Headers para Home Assistant
  http.addHeader("Authorization", "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJlMmU2N2EzZDIyYjQ0ODU0YjA4ODYyZjg5ZDQwYzYyNSIsImlhdCI6MTc3ODk3MTAzMSwiZXhwIjoyMDk0MzMxMDMxfQ.UjmFtAjzu8UAGPmgjcGxcvusNUAwr422tri7F2RSYWg");
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.GET();
  Serial.printf("HTTP code: %d\n", httpCode);

  if (httpCode != HTTP_CODE_OK) {
    Serial.println("Server response (first 200 chars):");
    String errBody = http.getString();
    Serial.println(errBody.substring(0, 200));
    http.end();
    
    sleepSeconds (300);
    return;    // Only for clarity
  }

  // Download the entire JSON
  String payload = http.getString();
  http.end();

  Serial.printf("Payload length: %d bytes\n", payload.length());

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("Error parsing JSON: ");
    Serial.println(error.c_str());

    sleepSeconds (300);
    return;    // Only for clarity
  }

  float ppm = doc["state"];

  Serial.print ("CO2: ");
  Serial.println (ppm);

  epaper.drawNumber (ppm,20, 100);
  epaper.drawString ("ppm",90,100 );
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