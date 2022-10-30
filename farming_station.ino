/** Farming/gardening environmental data gathering station software.
 *  Gathers ambient temperature/humidity, soil humidity, and sunlight level data.
 *  Sends the data periodically using a UDB socket. The data is broadcast to the
 *  specified subnet.
 *  Requirements: This program requires the DHT Sensor Library by Adafruit. 
 *  Version 1.4.4 was used. 
 */

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "time.h"
#include "sntp.h"

// Type of DHT sensor being used
#define DHTTYPE DHT11

// Delay between the farming station readings in milliseconds (There is a few seconds of built-in delay required to take readings)
#define DELAY_MILLIS 0

// Pins
#define DHTPIN 5
#define SOILPIN A0
#define SOLARPIN A1
#define VCC_SOIL 6

// Wifi name and password
const char * networkName = "MY_SSID";
const char * networkPswd = "MY_PASSWORD";

// IP adress and port of device to message to (192.168.2.255 is the adress to broadcast to all devices)
const char * udpAddress = "192.168.2.255";
const int udpPort = 10000;
boolean connected = false;

// Time servers and time zone
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* time_zone = "EST5EDT,M3.2.0,M11.1.0";

DHT_Unified dht(DHTPIN, DHTTYPE);
WiFiUDP udp;

// This function runs once when the program starts up
void setup() {
  // Pin setup
  pinMode(DHTPIN, INPUT);
  pinMode(SOILPIN, INPUT);
  pinMode(SOLARPIN, INPUT);
  pinMode(VCC_SOIL, OUTPUT);
    
  // Serial, wifi, and sensor setup
  Serial.begin(115200);
  connectToWiFi(networkName, networkPswd);
  dht.begin();

  // Time setup
  sntp_servermode_dhcp(1);
  configTzTime(time_zone, ntpServer1, ntpServer2);
}

// This function runs continually
void loop() {
  // Begin the message to send on the wifi
  udp.beginPacket(udpAddress,udpPort);

  // Print sensor readings
  udp.printf("\n----------FARMING STATION UPDATE v1.0----------\n");
  printTime();
  printTempAndHumidity();
  printSoilHumidityAndSunshine();

  // End the message
  udp.endPacket();

  // Delay (Remember a few seconds of delay is built-in because of required time to take sensor readings)
  delay(DELAY_MILLIS);
}

// Prints the time
void printTime()
{
  // Sees if time is available, if it is then it gets the time
  boolean has_time = false;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    has_time = true;
  }

  // If the time was available then print it. If not then print "No time stamp."
  if (has_time)
  {
    udp.printf("%i-%i-%i %i:%i:%i\n", timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_year + 1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
  else
  {
    udp.printf("No time stamp.\n");
  }
}

// Prints the ambient temperature and humidity
void printTempAndHumidity()
{
  // Get temperature once
  sensors_event_t event;
  dht.temperature().getEvent(&event);

  // If invalid reading then keep trying until there is a valid reading
  while (isnan(event.temperature))
  {
    // One-second delays between sensor readings required
    delay(1000);
    dht.temperature().getEvent(&event);
  }
  delay(1000);

  // Sensor outputs in Celsius so convert to fahrenheit and print
  double f = (event.temperature * 1.8) + 32;
  udp.printf("Ambient Temperature: %g *F\n", f);

  // Get humidity once
  dht.humidity().getEvent(&event);

  // If invalid reading then keep trying until there is a valid reading
  while (isnan(event.relative_humidity))
  {
    delay(1000);
    dht.humidity().getEvent(&event);
  }

  // Print reading
  udp.printf("Ambient Humidity: %g", event.relative_humidity);
  udp.print("%");
  udp.printf("\n");
}

// Prints the soil humidity and sunlight
void printSoilHumidityAndSunshine()
{
  // shum means soil humidity
  float shum_sum = 0;
  float sun_sum = 0;

  // This digitalWrite powers on the soil sensor because it corrodes slower if you only power it on when you need it
  digitalWrite(VCC_SOIL, HIGH);

  // Take 10 readings and average them for more accuracy
  for (int i = 0; i < 9; i++)
  { 
    shum_sum += voltage(analogRead(SOILPIN));
    sun_sum += voltage(analogRead(SOLARPIN));

    delay(100);
  }
  digitalWrite(VCC_SOIL, LOW);
  
  float soil_humidity = shum_sum / 10;
  float sunlight = sun_sum / 10;

  // Calculate sunlight percent: the brightest sun outputs voltage of 4.90 volts so divide output by 4.90 to get percent
  float sunlight_percent = 100 * (sunlight / 4.90);
  
  // No negative percents or percents below 0
  if (sunlight_percent > 100)
  {
    sunlight_percent = 100;
  }
  else if (sunlight_percent < 0)
  {
    sunlight_percent = 0;
  }

  // Decide if the solar cell was in direct sunlight or not: the threshold is 88 percent sunlight
  boolean in_sunlight = false;
  if (sunlight_percent >= 88)
  {
    in_sunlight = true;
  }

  // Calculate soil percentate: the range of voltage is 2.2 volts to about 1 volt, so minus the max and min by 1 and divide by max to get percent
  float soil_percent = 100 - (((soil_humidity - 1) / 1.20) * 100);

  // No negative percents or percents below 0
  if (soil_percent > 100)
  {
    soil_percent = 100;
  }
  else if (soil_percent < 0)
  {
    soil_percent = 0;
  }

  // Print the soil humidity and sunlight
  udp.printf("Soil Humidity: %g", soil_percent);
  udp.print("%");
  udp.printf("\nSunlight: %.2f", sunlight_percent);
  udp.print("% ");
  
  // Print the decision if we were in direct sunlight or not
  if (in_sunlight)
  {
    udp.printf("(Probably in direct sunlight.)\n");
  }
  else
  {
    udp.printf("(Probably not in direct sunlight.)\n");
  }
}

// Connects to wifi
void connectToWiFi(const char * ssid, const char * pwd){
  Serial.println("Connecting to WiFi network: " + String(ssid));
  // Delete old config
  WiFi.disconnect(true);
  // Register event handler
  WiFi.onEvent(WiFiEvent);
  // Initiate connection
  WiFi.begin(ssid, pwd);

  Serial.println("Waiting for WIFI connection...");
}

// The function that is registered when the wifi is connected to.
void WiFiEvent(WiFiEvent_t event){
    switch(event) {
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
          // When connected
          Serial.print("WiFi connected! IP address: ");
          Serial.println(WiFi.localIP());  
          // Initializes the UDP state
          // This initializes the transfer buffer
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

// Converts the input value from analogRead() to voltage and multiplies the voltage by 2 (because all analog inputs had a voltage divider that divided voltage by 2)
float voltage(int value)
{
  // This number comes from 0.0003369552 * 2. That number was obtained by taking two points and finding the slope of the line that relates the input value to the voltage
  float voltage = value * 0.00067391038;

  // No negative voltages
  if (voltage < 0)
  {
    voltage = 0;
  }
  return voltage;
}
