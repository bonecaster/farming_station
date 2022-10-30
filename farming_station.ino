/** Farming/gardening environmental data gathering station software.
 *  Gathers ambient temperature/humidity, soil humidity, and sunlight level data.
 *  Sends the data periodically using a UDB socket. The data is broadcast to the
 *  specified subnet.
 *  Requirements: This program requires the DHT Sensor Library by Adafruit. 
 *  Version 1.4.4 was used. 
 */

//    struct tm {
//        int8_t          tm_sec; /**< seconds after the minute - [ 0 to 59 ] */
//        int8_t          tm_min; /**< minutes after the hour - [ 0 to 59 ] */
//        int8_t          tm_hour; /**< hours since midnight - [ 0 to 23 ] */
//        int8_t          tm_mday; /**< day of the month - [ 1 to 31 ] */
//        int8_t          tm_wday; /**< days since Sunday - [ 0 to 6 ] */
//        int8_t          tm_mon; /**< months since January - [ 0 to 11 ] */
//        int16_t         tm_year; /**< years since 1900 */
//        int16_t         tm_yday; /**< days since January 1 - [ 0 to 365 ] */
//        int16_t         tm_isdst; /**< Daylight Saving Time flag */
//    };

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "time.h"
#include "sntp.h"

#define DHTTYPE DHT11
#define DELAY_MILLIS 0
#define DHTPIN 5
#define SOILPIN A0
#define SOLARPIN A1
#define VCC_SOIL 6


unsigned long lastTime;
const char * networkName = "MY_SSID";
const char * networkPswd = "MY_PASSWORD";

// Broadcast address to send the datagram to.
const char * udpAddress = "192.168.2.255";
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* time_zone = "EST5EDT,M3.2.0,M11.1.0";

const int udpPort = 10000;
boolean connected = false;


DHT_Unified dht(DHTPIN, DHTTYPE);
WiFiUDP udp;

void setup() {
  // Pin setup
  pinMode(DHTPIN, INPUT);
  pinMode(SOILPIN, INPUT);
  pinMode(SOLARPIN, INPUT);
  pinMode(VCC_SOIL, OUTPUT);
    
  // Serial and sensor setup
  Serial.begin(115200);
  connectToWiFi(networkName, networkPswd);
  dht.begin();

  // Time setup
  //sntp_set_time_sync_notification_cb(timeavailable);
  sntp_servermode_dhcp(1);
  configTzTime(time_zone, ntpServer1, ntpServer2);
}

void loop() {
  udp.beginPacket(udpAddress,udpPort);
  udp.printf("\n----------FARMING STATION UPDATE v1.0----------\n");
  printTime();
  printTempAndHumidity();
  printSoilHumidityAndSunshine();
  udp.endPacket();

  // Delay
  delay(DELAY_MILLIS);
}

void printSoilHumidityAndSunshine()
{
  float shum_sum = 0;
  float sun_sum = 0;
  
  digitalWrite(VCC_SOIL, HIGH);
  for (int i = 0; i < 9; i++)
  { 
    shum_sum += voltage(analogRead(SOILPIN));
    sun_sum += voltage(analogRead(SOLARPIN));
    
    delay(100);
  }
  digitalWrite(VCC_SOIL, LOW);
  
  // Get sensor measurements

  float soil_humidity = shum_sum / 10;
  float sunlight = sun_sum / 10;

  float sunlight_percent = 100 * (sunlight / 4.90);
  if (sunlight_percent > 100)
  {
    sunlight_percent = 100;
  }
  else if (sunlight_percent < 0)
  {
    sunlight_percent = 0;
  }

  boolean in_sunlight = false;
  if (sunlight_percent >= 88)
  {
    in_sunlight = true;
  }

  float soil_percent = 100 - (((soil_humidity - 1) / 1.20) * 100);
  
  if (soil_percent > 100)
  {
    soil_percent = 100;
  }
  else if (soil_percent < 0)
  {
    soil_percent = 0;
  }

  
  udp.printf("Soil Humidity: %g", soil_percent);
  udp.print("%");
  udp.printf("\nSunlight: %.2f", sunlight_percent);
  udp.print("% ");
  if (in_sunlight)
  {
    udp.printf("(Probably in direct sunlight.)\n");
  }
  else
  {
    udp.printf("(Probably not in direct sunlight.)\n");
  }
}

void printTime()
{
  boolean has_time = false;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    has_time = true;
  }

  if (has_time)
  {
    udp.printf("%i-%i-%i %i:%i:%i\n", timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
  else
  {
    udp.printf("No time stamp.\n");
  }
}

void printTempAndHumidity()
{
  sensors_event_t event;
  dht.temperature().getEvent(&event);

  while (isnan(event.temperature))
  {
    delay(1000);
    dht.temperature().getEvent(&event);
  }
  delay(1000);

  double f = (event.temperature * 1.8) + 32;
  udp.printf("Ambient Temperature: %g *F\n", f);
  
  dht.humidity().getEvent(&event);
  
  while (isnan(event.relative_humidity))
  {
    dht.humidity().getEvent(&event);
    delay(1000);
  }
  udp.printf("Ambient Humidity: %g", event.relative_humidity);
  udp.print("%");
  udp.printf("\n");
}

// Soil humidity senser ranged from 2.2 volts to 0.9 volts

void connectToWiFi(const char * ssid, const char * pwd){
  Serial.println("Connecting to WiFi network: " + String(ssid));

  // delete old config
  WiFi.disconnect(true);
  //register event handler
  WiFi.onEvent(WiFiEvent);
  
  //Initiate connection
  WiFi.begin(ssid, pwd);

  Serial.println("Waiting for WIFI connection...");
}

void WiFiEvent(WiFiEvent_t event){
    switch(event) {
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
          //When connected set 
          Serial.print("WiFi connected! IP address: ");
          Serial.println(WiFi.localIP());  
          //initializes the UDP state
          //This initializes the transfer buffer
          udp.begin(WiFi.localIP(),udpPort);
          connected = true;
          break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
          Serial.println("WiFi lost connection");
          connected = false;
          break;
      default: break;
    }
}

// Converts the input value from analogRead() to voltage and multiplies the voltage by 2
float voltage(int value)
{
  // This number comes from 0.0003369552 * 2
  float voltage = value * 0.00067391038;
  if (voltage < 0)
  {
    voltage = 0;
  }
  return voltage;
}
