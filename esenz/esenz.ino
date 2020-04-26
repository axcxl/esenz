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
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
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

#define BUTTON 0 //GP0 triggers OTA

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

long lastRead = 0;
long lastSend = 0;
long lastButton = 0;
float avg_temp = 0, avg_hum = 0;
int num_reads = 0;
int wifi_state = 0;
String id;

void setup_wifi() {  
  if (wifi_state == 0) {
    wifi_state = 1;
  }
  else {
    Serial.println("Already connected, skipping setup!");
    return;
  }
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
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_ota() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    ESP.reset();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void setup_mqtt() {
  if (wifi_state != 1) {
    Serial.println("WARNING: WIFI not active! Skipping MQTT SETUP!");
    return;
  }

   // Create unique ID for MQTT, based on MAC
  String mac = String(WiFi.macAddress());
  char tmp[7];
  // MAC format - FF:FF:FF:FF:FF:FF, remove the :
  int j = 0;
  for(int i = 9; i <= 16; i++) {
    if(mac[i] != ':') {
      tmp[j++] = mac[i];
    }
  }
  tmp[j] = '\0'; //end string
  id = String(tmp);
  
  // Loop until we're reconnected
  client.setServer(mqtt_server, 1883);
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    Serial.print("Using ID: ");
    Serial.println(id.c_str());
    
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
  // LED_BUILTIN as output
  pinMode(LED_BUILTIN, OUTPUT);

  //GP0 as input
  pinMode(BUTTON, INPUT);

  // Sensor setup 
  Serial.println("Starting sensor connection");
  dht.begin();  
  delay(1000); //needed for sensor init

  // Serial setup
  Serial.begin(115200);

  // Wifi settings
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);
}

void loop() {
  float t, h;
  int buttonstate;
  int buttonstate2;
  int hum_status;

  ArduinoOTA.handle();

  long now = millis();

  buttonstate = digitalRead(BUTTON);
  if (now - lastButton > 1000) {
     buttonstate2 = digitalRead(BUTTON);
     lastButton = now;

     if ((buttonstate == LOW) && (buttonstate2 == LOW)) {
      Serial.println("Starting WIFI by user request!");
      setup_wifi();
      setup_ota();
     }
  }

  // Read sensors every 30 seconds
  if (now - lastRead > 30000) {
    num_reads++;
    lastRead = now;
        
    Serial.println("reading from sensor");
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    t = dht.readTemperature();
    Serial.print(num_reads);
    Serial.print(F(" Humidity - "));
    Serial.print(h);
    Serial.print(F("%  Temperature - "));
    Serial.print(t);
    Serial.println(F("°C "));

    avg_temp += t;
    avg_hum += h;
  }

  // After 10 minutes send info to server
  if(now - lastSend > 605000) {
    lastRead = now; //no point in reading the sensor again
    
    Serial.println("Starting WIFI for send!");
    setup_wifi();
    delay(500);
    setup_mqtt();
    
    avg_temp /= num_reads;
    avg_hum /= num_reads;

    // Source: https://www.domoticz.com/forum/viewtopic.php?t=11138
    // TODO: this should be investigated further
    if(avg_hum < 30) {
      hum_status = 2; //Dry
    } else if ((avg_hum >= 30) && (avg_hum < 45)){
      hum_status = 0; //Normal
    } else if ((avg_hum >=45) && (avg_hum < 70)) {
      hum_status = 1; //Comfortable
    } else {
      hum_status = 3; //Wet
    }
    
    String topic = "data/temphum";
    topic += '/' + id + '\0';

    String svalue;
    // Svalue is "TEMP;HUM;HUM_STATUS" Quotes are mandatory for nodered processing!
    svalue = "\"" + String(avg_temp) + ";" + String(avg_hum) + ";" + String(hum_status) + "\"";

    Serial.print("Publishing ");
    Serial.print(topic);
    Serial.print(" Svalue:");
    Serial.println(svalue);
    client.publish(topic.c_str(), svalue.c_str());

    delay(1000);

    lastSend = now;
  }


  // Small delay than reset
  if(now > 605500) {
    //then start over
    Serial.println("Resetting!");
    ESP.reset();
  }

 
}
