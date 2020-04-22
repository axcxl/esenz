/*
 Esenz ESP8266 MQTT example

 This sketch demonstrates the capabilities of the pubsub library in combination
 with the ESP8266 board/library.

 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off

 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.

 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include "wifi_credentials.h"

/* These are defined in a wifi_credentials.h file placed in the same folder:
 *  #define SSID "<ssid>"
 *  #define PASS "<password>"
 *  #define SERV "<server ip/ server name if DNS is working>"
 */
const char* ssid = SSID;
const char* password = PASS;
const char* mqtt_server = SERV;

//#define DHTPIN 4     // Digital pin 4 for Olimex ESPs
#define DHTPIN 2       // Digital pin 2 for the rest

//#define DHTTYPE DHT11   // Also have one of these
#define DHTTYPE DHT21   // DHT 21 (AM2301)

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

long lastMsg = 0;
char msg[50];
int value = 0;
String id;

void setup_wifi() {
  Serial.println("Starting sensor connection");
  dht.begin();  

  delay(1000); //needed for sensor init
  
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    Serial.print("Using ID: ");
    Serial.println(id);
    
    // Attempt to connect
    if (client.connect(id.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  String mac = String(WiFi.macAddress());
  char tmp[7];

  // MAC format - FF:FF:FF:FF:FF:FF, remove the :
  int j = 0;
  for(int i = 9; i <= 16; i++) {
    if(mac[i] != ':') {
      tmp[j++] = mac[i];
    }
  }
  tmp[j] = '\0'; //make sure string is terminated!

  id = String(tmp);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  Serial.println("reading from sensor");
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  Serial.print(F(" Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.println(F("°C "));
     
  String temp = "data/temperature";
  temp += '/';
  temp += id;
  temp += '\0';

  String hum = "data/humidity";
  hum += '/';
  hum += id;
  hum += '\0';

  Serial.println("Publishing");
  client.publish(temp.c_str(), String(t).c_str());
  client.publish(hum.c_str(), String(h).c_str());

  delay(500); //needed to properly send te message

  Serial.println("Turning off Wifi!");
  // Turn off Wifi
  WiFi.mode( WIFI_OFF );
  WiFi.forceSleepBegin();
  delay( 1 );

  // Sleep for a while - 10 minutes should be ok - TOOD: make it configurable vis mqtt
  delay( 600000 );

  //then start over
  Serial.println("Resetting!");
  ESP.reset();
}
