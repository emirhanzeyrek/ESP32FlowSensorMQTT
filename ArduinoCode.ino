#include <WiFi.h>
#include "WiFiClientSecure.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

volatile int flow_frequency;    // Sensor pulse
unsigned int l_hour;            // Liters / hour
unsigned char flowsensor = 25;  // GPIO pin to which water flow sensor is connected
unsigned char ledPin = 32;      // GPIO pin to which LED is connected
unsigned long currentTime;
unsigned long cloopTime;

/************************* WiFi Access Point *********************************/
#define WLAN_SSID "WLAN_SSID"
#define WLAN_PASS "WLAN_PASSWORD"

/************************* Adafruit.io Setup *********************************/
#define AIO_SERVER      "io.adafruit.com"

// Using port 8883 for MQTTS
#define AIO_SERVERPORT  8883

// Adafruit IO Account Configuration
// (to obtain these values, visit https://io.adafruit.com and click on Active Key)
#define AIO_USERNAME "AIO_USERNAME"
#define AIO_KEY      "AIO_KEY"

/************ Global State (you don't need to change this!) ******************/

// WiFiFlientSecure for SSL/TLS support
WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// io.adafruit.com root CA
const char* adafruitio_root_ca = \
      "-----BEGIN CERTIFICATE-----\n"
      "ADAFRUİT IO ROOT CERTIFICATE LICENSE KEY"
      "-----END CERTIFICATE-----\n";

/*************************** Sketch Code ************************************/

void flow() // Interrupt
{
  flow_frequency++;
}

void setup()
{
  // Pin modes and initial states settings
  pinMode(flowsensor, INPUT);
  digitalWrite(flowsensor, HIGH);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Turn off the LED on startup

  Serial.begin(9600);

  // Interrupt setting
  attachInterrupt(digitalPinToInterrupt(flowsensor), flow, RISING);
  sei(); // interrupts on

  currentTime = millis();
  cloopTime = currentTime;

  // WiFi connection
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  delay(1000);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  delay(2000);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Setting the root CA of Adafruit IO
  client.setCACert(adafruitio_root_ca);
}

void loop()
{
  MQTT_connect();

  currentTime = millis();

  // Counting the number of pulses per second and calculating liter-hours
  if (currentTime >= (cloopTime + 1000))
  {
    cloopTime = currentTime; // Update seconds
    l_hour = (flow_frequency * 60 / 7.5); // Liter calculation: Frequency * 60 / 7.5
    flow_frequency = 0; // Reset the counter

    Serial.print("Water Flow: ");
    Serial.print(l_hour, DEC);
    Serial.println(" L/hour");

    // If there is water flow, turn the LED on, otherwise turn it off
    if (l_hour > 0)
    {
      digitalWrite(ledPin, HIGH); // Turn on the LED
      // Send the water flow via MQTT
      publishFlowData();
    }
    else
    {
      digitalWrite(ledPin, LOW); // Turn off the LED
    }
  }

  // Process MQTT packets
  mqtt.processPackets(10000);

  // If the MQTT server cannot be pinged, close the connection and try again
  if (!mqtt.ping())
  {
    mqtt.disconnect();
  }
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }

  Serial.println("MQTT Connected!");
}

// Publish the water flow over MQTT
void publishFlowData()
{
  // Send the liter/hour value to the "flow" feed
  Adafruit_MQTT_Publish flow = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/flow");
  Serial.print(F("\nSending flow value: "));
  Serial.println(l_hour);
  if (!flow.publish(l_hour))
  {
    Serial.println(F("Failed to publish flow data"));
  }
  else
  {
    Serial.println(F("Flow data published successfully"));
  }
}
