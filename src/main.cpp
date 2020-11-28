#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <secrets.h> // Contains definitions of my secrets

WiFiClient espClient;
PubSubClient client(espClient);

const char *ssid = MY_WIFI_SSID;         // Wifi SSID
const char *password = MY_WIFI_PASSWORD; // Wifi Password

// MQTT details
const char *mqttServer = MY_MQTT_SERVER;                   // MQTT server url
const int mqttPort = MY_MQTT_PORT;                         // MQTT server port
const char *mqttClientId = MY_MQTT_CLIENT_ID;              // Unique clientid
const char *mqttTopicOut = MY_MQTT_TOPIC_OUT;              // MQTT topic where received Lora messages are published
const char *mqttTopicIn = MY_MQTT_TOPIC_IN;                // MQTT topic subscribed, received messages are transmitted by Lora
const char *mqttTopicPing = MY_MQTT_TOPIC_PING;            // MQTT topic subscribed, receive ping
const char *mqttTopicPingReply = MY_MQTT_TOPIC_PING_REPLY; // MQTT topic where ping reply are published.

// Lora radio config for TTGO T-Beam v1.1 board. Different for other boards
const int loraCsPin = 18;    // LoRa radio chip select
const int loraResetPin = 23; // LoRa radio reset
const int loraIrqPin = 26;   // change for your board; must be a hardware interrupt pin
const int loraSF = 8;        // Lora Spreading factor

boolean gotLoraPacket = false;
String loraMsg = "";
String loraRSSI = "";

boolean gotMQTTMsg = false;
boolean gotMQTTPing = false;
String mqttMsg = "";

void initWifi()
{
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void onLoraReceive(int packetSize)
{
    if (packetSize == 0)
        return;

    // read packet header bytes:
    String incoming = "";

    while (LoRa.available())
        incoming += (char)LoRa.read();

    gotLoraPacket = true;
    loraMsg = incoming;
    loraRSSI = String(LoRa.packetRssi());
}

void sendLoraMessage(String outgoing)
{
    LoRa.beginPacket();   // start packet
    LoRa.print(outgoing); // add payload
    LoRa.endPacket();     // finish packet and send it
}

void initLora()
{
    // override the default CS, reset, and IRQ pins (optional)
    LoRa.setPins(loraCsPin, loraResetPin, loraIrqPin); // set CS, reset, IRQ pin

    // initialize ratio at 868 MHz
    if (!LoRa.begin(868E6))
    {
        Serial.println("LoRa init failed. Check your connections.");
        while (true)
            ; // if failed, do nothing
    }

    LoRa.setSpreadingFactor(loraSF); // ranges from 6-12,default 7 see API docs
    Serial.println("LoRa init succeeded.");

    LoRa.onReceive(onLoraReceive);
}

void onMQTTReceive(char *topic, byte *payload, unsigned int length)
{
    Serial.print("mqtt received:(");
    Serial.print(topic);
    Serial.print("): ");
    payload[length] = '\0'; // terminate it just in case
    Serial.println((char *)payload);

    // if mqtt msg received
    if (strcmp(topic, mqttTopicIn) == 0)
    {
        gotMQTTMsg = true;
        mqttMsg = (char *)payload;
    }

    // if ping received
    if (strcmp(topic, mqttTopicPing) == 0)
        gotMQTTPing = true;
}

void reconnectMQTT()
{
    while (!client.connected())
    { // Loop until we're reconnected
        if (client.connect(mqttClientId))
        { // Attempt to connect
            Serial.println("connected to MQTT");
            client.publish(mqttTopicPingReply, "Hi, from board"); // Once connected, publish an announcement...
            client.subscribe(mqttTopicPing);
            client.subscribe(mqttTopicIn);
        }
        else
        {
            Serial.print("MQTT client connect failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000); // Wait 5 seconds before retrying
        }
    }
}

void setup()
{
    Serial.begin(115200);
    initWifi();
    initLora();
    client.setServer(mqttServer, mqttPort);
    client.setCallback(onMQTTReceive);

    LoRa.receive();
}

void loop()
{
    if (!client.connected())
        reconnectMQTT();

    if (gotLoraPacket)
    {
        Serial.print("Received Lora message: ");
        Serial.println(loraMsg);
        Serial.print("Publishing to MQTT server... ");
        client.publish(mqttTopicOut, loraMsg.c_str());
        Serial.println("Done");
        gotLoraPacket = false;
    }

    if (gotMQTTMsg)
    {
        Serial.print("Transmitting Lora msg... ");
        sendLoraMessage(mqttMsg); // transmit Lora msg
        Serial.println("Done");
        gotMQTTMsg = false;
        LoRa.receive(); // go back into receive mode
    }

    if (gotMQTTPing)
    {
        Serial.print("Ping back to server... ");
        client.publish(mqttTopicPingReply, "I am alive ;)");
        Serial.println("Done");
        gotMQTTPing = false;
    }

    client.loop();
}
