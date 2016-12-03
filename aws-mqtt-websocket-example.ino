
#include <dummy.h>
#include <WiFiUdp.h>
#include <WiFiServer.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <ESP8266WiFiType.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiAP.h>
#include <Arduino.h>
#include <Stream.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

//AWS
#include "sha256.h"
#include "Utils.h"
#include "AWSClient2.h"

//WEBSockets
#include <Hash.h>
#include "WebSocketsClient.h"
#include "WebSockets.h"

//MQTT PAHO
#include <SPI.h>
#include "IPStack.h"
#include "Countdown.h"
#include "MQTTClient.h"



//AWS MQTT Websocket
#include "Client.h"
#include "AWSWebSocketClient.h"
#include "CircularByteBuffer.h"

//AWS IOT config, change these:
//char wifi_ssid[]       = "9205";
//char wifi_password[]   = "4074337";//361";
char aws_endpoint[]    = "a1pylxiebz0eyl.iot.us-west-2.amazonaws.com";
char aws_key[]         = "AKIAIQ6EQU6ZABDSICTA";
char aws_secret[]      = "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";
char aws_region[]      = "us-west-2";
const char* aws_topic  = "$aws/things/TestChar1/shadow/update";
int port = 443;
int incomingByte = 0;

//MQTT config
const int maxMQTTpackageSize = 512;
const int maxMQTTMessageHandlers = 1;

ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient(1000);

IPStack ipstack(awsWSclient);
MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers> *client = NULL;

//# of connections
long connection = 0;

//generate random mqtt clientID
char* generateClientID () {
  char* cID = new char[23]();
  for (int i=0; i<22; i+=1)
    cID[i]=(char)random(1, 256);
  return cID;
}

//count messages arrived
int arrivedcount = 0;

//callback to handle mqtt messages
void messageArrived(MQTT::MessageData& md)
{
  MQTT::Message &message = md.message;

  Serial.print("Message ");
  Serial.print(++arrivedcount);
  Serial.print(" arrived: qos ");
  Serial.print(message.qos);
  Serial.print(", retained ");
  Serial.print(message.retained);
  Serial.print(", dup ");
  Serial.print(message.dup);
  Serial.print(", packetid ");
  Serial.println(message.id);
  Serial.print("Payload ");
  char* msg = new char[message.payloadlen+1]();
  memcpy (msg,message.payload,message.payloadlen);
  Serial.println(msg);
  delete msg;
}

//connects to websocket layer and mqtt layer
bool connect () {

    if (client == NULL) {
      client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
    } else {

      if (client->isConnected ()) {    
        client->disconnect ();
      }  
      delete client;
      client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
    }


    //delay is not necessary... it just help us to get a "trustful" heap space value
    delay (1000);
    Serial.print (millis ());
    Serial.print (" - conn: ");
    Serial.print (++connection);
    Serial.print (" - (");
    Serial.print (ESP.getFreeHeap ());
    Serial.println (")");




   int rc = ipstack.connect(aws_endpoint, port);
    if (rc != 1)
    {
      Serial.println("error connection to the websocket server");
      return false;
    } else {
      Serial.println("websocket layer connected");
    }


    Serial.println("MQTT connecting");
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    char* clientID = generateClientID ();
    data.clientID.cstring = clientID;
    rc = client->connect(data);
    delete[] clientID;
    if (rc != 0)
    {
      Serial.print("error connection to MQTT server");
      Serial.println(rc);
      return false;
    }
    Serial.println("MQTT connected");
    return true;
}

//subscribe to a mqtt topic
void subscribe () {
   //subscript to a topic
    int rc = client->subscribe(aws_topic, MQTT::QOS0, messageArrived);
    if (rc != 0) {
      Serial.print("rc from MQTT subscribe is ");
      Serial.println(rc);
      return;
    }
    Serial.println("MQTT subscribed");
}

//send a message to a mqtt topic
void sendmessage () {
    //send a message
    MQTT::Message message;
    char buf[100];
    strcpy(buf, "{\"state\":{\"reported\":{\"on\": false}, \"desired\":{\"on\": true}}}");
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    int rc = client->publish(aws_topic, message); 
}

void sendmessage(String state)
{
    //send a message
    MQTT::Message message;
    char buf[400];
    //strcpy(buf, state.);
    state.toCharArray(buf, 400);
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    int rc = client->publish(aws_topic, message); 
}

//collectData gets the data from the MCU and builds the JSON string
String collectData(void)
{
  String ret;
  unsigned long lower = 0, upper = 0; //long is 32 bit
  unsigned int pressure[16]; //INT is 16 bit
  int temperature;
  byte temp[42]; //byte is 8 bit
  byte data = 0,presCont = 0;

  while( data < 42)
  {
    if (Serial.available() > 0)
    {
      temp[data] = Serial.read();
      data++;
    }
  }

  for(data = 0; data < 4; data++)
  {
    lower = lower << 8;
    lower += temp[data];
  }

  for(data = 4; data < 8; data++)
  {
    upper = upper << 8;
    upper += temp[data];
  }
  
  for(data = 8; data < 40; data++)
  {
    temperature = temperature << 8;
    temperature += temp[data];
    //pressure[presCont] = pressure[presCont] << 8;
    //pressure[presCont] += temp[data];
    if(data % 2 == 1)
    {
      pressure[presCont] = temperature;
      presCont++;
    }
  }

  temperature = temp[40];
  temperature = temperature << 8;
  temperature += temp[41];

  ret = "{\"DataPosition\": \"Current\",\"LowerBackDepth\": ";
  ret.concat(lower);
  ret = ret + ",\"UpperBackDepth\": ";
  ret.concat(upper);
  ret = ret + ",\"PressureData\": [";
  ret.concat(pressure[0] & 0x0000FFFF);

  //Serial.println("made base string");
  //Serial.println(ret);
  
  for(data = 1; data < 16; data++)
  {
    ret = ret + ",";
    ret.concat(pressure[data] & 0x0000FFFF);
    Serial.println(data);
  }
  ret = ret + "]}";

  
  return ret;
}

void setup() {
    Serial.begin (115200);
    delay (2000);
    Serial.setDebugOutput(1);
    delay(1000);

    //create wifi manager
    WiFiManager wifiManager;
    wifiManager.autoConnect("PosturePerfectAP","penis");


//Obsolete code below
    //fill with ssid and wifi password
//    WiFiMulti.addAP(wifi_ssid, wifi_password);
//    Serial.println ("connecting to wifi");
//    while(WiFiMulti.run() != WL_CONNECTED) {
//    delay(100);
//    Serial.print (".");
//    }
//    Serial.println ("\nconnected");

    //fill AWS parameters    
    awsWSclient.setAWSRegion(aws_region);
    awsWSclient.setAWSDomain(aws_endpoint);
    awsWSclient.setAWSKeyID(aws_key);
    awsWSclient.setAWSSecretKey(aws_secret);
    awsWSclient.setUseSSL(true);

    if (connect ()){
      subscribe ();
      sendmessage ();
    }

}

void loop() {
  //keep the mqtt up and running
  if (awsWSclient.connected ()) {    
      client->yield();
  } else {
    //handle reconnection
    if (connect ()){
      subscribe ();      
    }
  }
  //delay(500);
  //Serial.print("Still running\n");

  if (Serial.available() > 0)
  {
    incomingByte = Serial.read();
    Serial.print("recieved command: ");
    Serial.println(incomingByte, DEC);
  }

  if(incomingByte == 84) //'T' for true
  {
    sendmessage("{\"DataPosition\": \"Current\",\"LowerBackDepth\": 3.12,\"UpperBackDepth\": 3.25,\"PressureData\": [5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5]}");
  }
  if(incomingByte == 70) //'F' for false
  {
    sendmessage("{\"DataPosition\": \"Current\",\"LowerBackDepth\": 4.18,\"UpperBackDepth\": 5.00,\"PressureData\": [5,5,0,5,10,5,5,3,5,5,5,200,5,5,6,5]}");
  }
  if(incomingByte == 68) //start character 'D' for data acquisition.
  {
    String jsonReply;
    jsonReply = collectData();
    sendmessage(jsonReply);
  }

}
