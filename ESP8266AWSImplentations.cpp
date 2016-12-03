#include <stdio.h>

/* application.h is Esp8266's standard library. Defines the Arduino String
 * object, the Arduino delay() procedure, and the Esp8266 TCPClient. */
#include "Esp8266AWSImplementations.h"
#include "DeviceIndependentInterfaces.h"
#include <string.h>

char* getCurrentTime(void);

Esp8266HttpClient::Esp8266HttpClient() {
}

char* Esp8266HttpClient::send(const char* request, const char* serverUrl, int port) {

    Serial.print("Connecting to ");
    Serial.print(serverUrl);
    Serial.print(':');
    Serial.print(port);
    Serial.println();
    Serial.println(request);

    String responseBuilder;
    if (sclient.connect(serverUrl, port)) {

        // Send the request
        sclient.println(request);
        sclient.println();

        int connectionTimeout = millis() + 5000;
        Serial.print('*');
        while (sclient.connected() == 0) {
          if (connectionTimeout - millis() < 0) {
            Serial.println(">>> Connection Timeout !");
            sclient.stop();
            return 0;
          }
        }

        int availableTimeout = millis() + 5000;
        Serial.print('.');
        while (sclient.available() == 0) {
          if (availableTimeout - millis() < 0) {
            Serial.println(">>> Client Timeout !");
            sclient.stop();
            return 0;
          }
        }

        while(sclient.available()){
          String line = sclient.readStringUntil('\r');
          responseBuilder.concat(line);
        }

        sclient.stop();
    } else {
        sclient.stop();
        /* Error connecting. */
        return 0;
    }

    /* Copy responseBuilder into char* */
    int len = responseBuilder.length();
    char* response = new char[len + 1]();
    responseBuilder.toCharArray(response, len + 1);

    return response;
}

bool Esp8266HttpClient::usesCurl() {
  
    /* Does not use curl command. */
    return false;
}

Esp8266DateTimeProvider::Esp8266DateTimeProvider() {
}

const char* Esp8266DateTimeProvider::getDateTime() {

    return getCurrentTime();
}
bool Esp8266DateTimeProvider::syncTakesArg(void) {
    return true;
}

void Esp8266DateTimeProvider::sync(const char* dateTime) {
  // should have no need for an implementation
}

////////////////////////////////////
// convert month to digits
////////////////////////////////////
String getMonth(String sM) {
  if(sM=="Jan") return "01";
  if(sM=="Feb") return "02";
  if(sM=="Mar") return "03";
  if(sM=="Apr") return "04";
  if(sM=="May") return "05";
  if(sM=="Jun") return "06";
  if(sM=="Jul") return "07";
  if(sM=="Aug") return "08";
  if(sM=="Sep") return "09";
  if(sM=="Oct") return "10";
  if(sM=="Nov") return "11";
  if(sM=="Dec") return "12";
  return "01";
}

////////////////////////////////////
// Scrape UTC Time from server
////////////////////////////////////
char* getCurrentTime(void) {

    int timeout_busy = 0;

    String GmtDate;
    char* dateStamp = new char[15]();

    WiFiClient* client = new WiFiClient();;
    if (client->connect("aws.amazon.com", 80)) {
      
      // send a bad header on purpose, so we get a 400 with a DATE: timestamp
      char* timeServerGet = (char*)"GET example.com/ HTTP/1.1";

      //Send Request
      client->println("GET example.com/ HTTP/1.1");
      client->println("Connection: close");
      client->println();

      while((!client->available()) && (timeout_busy++ < 5000)){
        // Wait until the client sends some data
        delay(1);
      }

      // kill client if timeout
      if(timeout_busy >= 5000) {
          client->flush();
          client->stop();
          Serial.println("timeout receiving timeserver data");
          return dateStamp;
      }

      // read the http GET Response
      String req2 = client->readString();

      // close connection
      delay(1);
      client->flush();
      client->stop();

      int ipos = req2.indexOf("Date:");
      if(ipos > 0) {
        String gmtDate = req2.substring(ipos, ipos + 35);
        String utctime = gmtDate.substring(18,22) + getMonth(gmtDate.substring(14,17)) + gmtDate.substring(11,13) + gmtDate.substring(23,25) + gmtDate.substring(26,28) + gmtDate.substring(29,31);
        utctime.substring(0, 14).toCharArray(dateStamp, 15);
      }
    }
    else {
      Serial.println("did not connect to timeserver");
    }

    delete client;
    timeout_busy=0;     // reset timeout

    return dateStamp;   // Return latest or last good dateStamp
}
