/*

*/

#include "configuration.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <DS1302.h>
#include <EpochTime.h>

// Init the DS1302
DS1302 rtc(CEpin, IOpin, SCLKpin);

// Init a Time-data structure
Time t;

String getClientOutput(void);
String getcsvOutput(void);
String setTime(String);

SoftwareSerial mySerial(SSRX, SSTX); // RX, TX

double temperature = 0.00;
double pressure = 0.00;
int windDir = 0;
double windSpeed = 0.0;
int humidity = 0;
int lightValue = 0;
unsigned long int rainTipperCounter = 0;
unsigned long int bootTime = 0; // the epoch time the device last started
unsigned long int ct = 0; // 
String webPage = "";

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(WEB_PORT);

byte dataByte;

byte buffer[24];
int bufferindex = 0;

// I've define a packet to be as follows
// H is header   3 chars
// D is data     12 chars
// C is checksum 2 chars
// so
// HHHDDDDDDDDDDDDCC
//
// and in the D section
// byte 0 high byte of temperature
// byte 1 low byte of temperature
// byte 2 high byte of pressure
// byte 3 low byte of pressure
// byte 4 wind ordinal (values should be 0 - 15 to represent compass points)
// byte 5 Humdity (value range 0 - 100)
// bytes 6 to 9 rain tipper vale
// byte 10 Wind speed high byte
// byte 11 Wind speed low byte
struct packet {
  byte header[HEADER_SIZE];
  byte data[DATA_SIZE];
  byte checksum[CHECKSUM_SIZE];
} my_packet;

void setup()
{
  bootTime = getEpochTime();
#ifdef DEBUG
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  Serial.println("Beginning setup");
#endif

  // Set the clock to run-mode, and disable the write protection
  rtc.halt(false);
  rtc.writeProtect(true);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // set the data rate for the SoftwareSerial port
  mySerial.begin(9600);

  // Connect to WiFi network
#ifdef DEBUG
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSSID);
#endif
  WiFi.begin(SSSID, PASSWORD);

  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, HIGH);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif
  }
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
#ifdef DEBUG
  Serial.println();
  Serial.println("WiFi connected");
#endif

  // Start the server
  server.begin();
#ifdef DEBUG
  Serial.println("Server started");
#endif

  // Print the IP address
#ifdef DEBUG
  Serial.println(WiFi.localIP());
#endif

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

#ifdef DEBUG
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  Serial.println("Setup complete");
#endif
}

void loop()
{
  if (mySerial.available()) {
    dataByte = mySerial.read();

    if (dataByte == 13) {
      my_packet.header[0] = buffer[1];
      my_packet.header[1] = buffer[2];
      my_packet.header[2] = buffer[3];

      my_packet.data[0] = buffer[4]; // Temp
      my_packet.data[1] = buffer[5]; // Temp
      my_packet.data[2] = buffer[6]; // Pressure
      my_packet.data[3] = buffer[7]; // Pressure
      my_packet.data[4] = buffer[8]; // Win Dir
      my_packet.data[5] = buffer[9]; // Humidity
      my_packet.data[6] = buffer[10]; // Rain
      my_packet.data[7] = buffer[11]; // Rain
      my_packet.data[8] = buffer[12]; // Rain
      my_packet.data[9] = buffer[13]; // Rain
      my_packet.data[10] = buffer[14]; //Wind Speed
      my_packet.data[11] = buffer[15]; //Wind Speed
      my_packet.data[12] = buffer[16]; //Light Value
      my_packet.data[13] = buffer[17]; //Light Value

      my_packet.checksum[0] = buffer[18]; // Lowbyte
      my_packet.checksum[1] = buffer[19]; //Highbyte

      /// Perform checksum
      int checksum = 0;
      for (int i = 0; i < DATA_SIZE; ++i) {
        checksum += my_packet.data[i];
      }

      if (checksum == ( my_packet.checksum[0] << 8 ) + my_packet.checksum[1]) {
        digitalWrite(GREEN_LED, HIGH);

        // Temperature
        temperature = ( my_packet.data[0] << 8 ) + my_packet.data[1];
        temperature = (temperature / 10) - 120;
#ifdef DEBUG
        Serial.print("Temperature = ");
        Serial.println(temperature);
#endif

        //Pressure
        pressure = ( my_packet.data[2] << 8 ) + my_packet.data[3];
        pressure = (pressure / 10) - 800;
#ifdef DEBUG
        Serial.print("Pressure = ");
        Serial.println(pressure);
#endif

        // Wind Ordinal
        windDir = my_packet.data[4];
#ifdef DEBUG
        Serial.print("Wind Dir: = ");
        Serial.println(windDir);
#endif

        // Humidity
        humidity = my_packet.data[5];
#ifdef DEBUG
        Serial.print("Humidity = ");
        Serial.print(humidity);
        Serial.println("%");
#endif

        // Wind Speed
        windSpeed = ( my_packet.data[10] << 8 ) + my_packet.data[11];
        windSpeed = (windSpeed / 100) - 45;
#ifdef DEBUG
        Serial.print("Wind Speed = ");
        Serial.println(windSpeed);
#endif

        // Rain tipper counter
        rainTipperCounter = my_packet.data[6];
        rainTipperCounter = (rainTipperCounter << 8) + my_packet.data[7];
        rainTipperCounter = (rainTipperCounter << 8) + my_packet.data[8];
        rainTipperCounter = (rainTipperCounter << 8) + my_packet.data[9];
#ifdef DEBUG
        Serial.print("Rain Tipper Counter = ");
        Serial.println(rainTipperCounter );

#endif

        // Light Value
        lightValue = ( my_packet.data[12] << 8 ) + my_packet.data[13];
#ifdef DEBUG
        Serial.print("Light Value = ");
        Serial.println(lightValue);
        Serial.print(getEpochTime());
#endif
      }

      bufferindex = 0;
      digitalWrite(GREEN_LED, LOW);

#ifdef DEBUG
      Serial.println();
#endif
    }
    else {
      buffer[bufferindex] = dataByte;
      bufferindex++;
    }
  }

  // Check if a client has connected
  WiFiClient client = server.available();
  if (client) {
#ifdef DEBUG
    Serial.println("new client");
#endif
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    ct = millis();
    while (client.connected() && ct < millis() + CONNECTION_TIMEOUT) {
      if (client.available()) {
#ifdef DEBUG
        Serial.println("Client is available");
#endif
        // Read the first line of the request
        String req = client.readStringUntil('\r');
#ifdef DEBUG
        Serial.println(req);
#endif
        client.flush();
        // Prepare the response
        digitalWrite(RED_LED, HIGH);
        // Match the request
        if (req.indexOf("/csv") != -1) {
          webPage = getcsvOutput();
        } else if (req.indexOf("/settime") != -1) {
          pinMode(ENABLE_SET_TIME_PIN, INPUT);
          if (digitalRead(ENABLE_SET_TIME_PIN) == HIGH) {
#ifdef DEBUG
            Serial.println("Setting Time");
#endif
            webPage = setTime(req);
          } else {
#ifdef DEBUG
            Serial.println("Can't set Time");
#endif
            webPage = "HTTP/1.1 200 OK\r\n";
            webPage += "Content-Type: text/html\r\n";
            webPage += "Connection: close\r\n\r\n"; // the connection will be closed after completion of the response

            webPage += "<html><body><head>";
            webPage += "<title>Arduino Weather Station - Error</title>";
            webPage += "</head>";
            webPage += "<h3>Error Setting Time</h3>";
            webPage += "Can't set Time - Check <b>Enable Set Time</b> jumper</br>\r\n";
            webPage += "<a href=\"/\">home</a>";
            webPage += "</body><html>";
          }
        } else {
          webPage = getClientOutput();
        }
        client.print(webPage);
        delay(1);
        client.stop();
#ifdef DEBUG
        Serial.println("Client disconnected");
#endif
        digitalWrite(RED_LED, LOW);
      }
    }
  }
}


String getcsvOutput() {
  String s = "";
  s = temperature;
  s += ",";
  s += pressure;
  s += ",";
  s += windDir;
  s += ",";
  s += humidity ;
  s += ",";
  s += windSpeed ;
  s += ",";
  s += rainTipperCounter ;
  s += ",";
  s += lightValue ;
  s += ",";
  s += getEpochTime();
  return s;
}

String getClientOutput() {
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n";
  s += "Connection: close\r\n"; // the connection will be closed after completion of the response
  s += "Refresh: 60\r\n\r\n"; // refresh the page automatically every 60 sec

  s += "<!DOCTYPE HTML>\r\n";
  s += "<html>\r\n";
  s += "<body>\r\n";
  s += "<head>\r\n";
  s += "<title>Arduino Serial Weather Station</title>\r\n";

  s += "</head>\r\n";
  s += "<h3>Last good readings</h3>\r\n";

  s += "<script>\r\n";
  s += "var d = new Date(" + (String) getEpochTime() + "000);\r\n";
  s += "document.getElementById(\"date\").innerHTML = \"Last Update: \" + d;\r\n";
  s += "</script>\r\n";
  s += "<p id=\"date\"></p>\r\n";
  
  s += "Temperature: \r\n";
  s += temperature;
  s += " degrees C\r\n";
  s += "<br />\r\n";
  s += "Pressure: " + String(pressure);
  s += " Pa\r\n";
  s += "<br />\r\n";
  s += "Wind Direction: \r\n";
  s += windDir;

  s += "<br />\r\n";
  s += "Humidity: \r\n";
  s += humidity;
  s += "<br />\r\n";

  s += "Wind Speed: \r\n";
  s += windSpeed;
  s += "<br />\r\n";

  s += "Rain counter: \r\n";
  s += rainTipperCounter;
  s += "<br />\r\n";

  s += "Light Value: \r\n";
  s += lightValue;
  s += "<br />\r\n";

  s += "<script>\r\n";
  s += "var d = new Date(" + (String) bootTime + "000);\r\n";
  s += "document.getElementById(\"bootdate\").innerHTML = \"Boot Time: \" + d;\r\n";
  s += "</script>\r\n";
  s += "<p id=\"bootdate\"></p>\r\n";

  s += "<br />\r\n";
  s += "</body>\r\n";
  s += "</html>\r\n";

  return s;
}

/**
@name: setTime
@param: String uri
@description: Function to set the time via a url.
curl http://weather617.servebeer.com/settime?2016-02-02T11:52:02-2

**/
String setTime(String uri) {
  int index = uri.lastIndexOf('?');
  int l = uri.lastIndexOf(' ');
  String date = uri.substring(index + 1, l);

  // extract the various elements from date
  // zero being the first element
  // 2016-01-26T23:59:00-7

  int year = date.substring(0, 4).toInt();
  int month = date.substring(5, 7).toInt();
  int day = date.substring(8, 10).toInt();
  int hour = date.substring(11, 13).toInt();
  int min = date.substring(14, 16).toInt();
  int sec = date.substring(17, 19).toInt();
  int dow = date.substring(20, 21).toInt();
#ifdef DEBUG
  Serial.println(date);
  Serial.print(day, DEC); Serial.print("/"); Serial.print(month, DEC); Serial.print("/"); Serial.print(year, DEC);
  Serial.println();
  Serial.print(hour, DEC); Serial.print(":"); Serial.print(min, DEC); Serial.print(":"); Serial.print(sec, DEC);
  Serial.println();
  Serial.print("dow "); Serial.print(dow, DEC);
#endif
  rtc.halt(false);
  rtc.writeProtect(false);

  // Set the time in the DS1302
  rtc.setDOW(dow);        // Set Day-of-Week Monday will be a one
  rtc.setTime(hour, min, sec);     // Set the time (24hr format)
  rtc.setDate(day, month, year);   // Set the date

  rtc.writeProtect(true);

  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n";
  s += "Connection: close\r\n\r\n"; // the connection will be closed after completion of the response

  s += "<!DOCTYPE HTML>\r\n";
  s += "<html>\r\n";
  s += "<body>\r\n";
  s += "<head>\r\n";
  s += "<title>Arduino Serial Weather Station</title>\r\n";

  s += "</head>\r\n";
  s += "<h3>Clock set</h3>\r\n";
  s += "<p id=\"date\"></p>\r\n";

  s += "<script>\r\n";
  s += "var d = new Date(" + (String) getEpochTime() + "000);\r\n";
  s += "document.getElementById(\"date\").innerHTML = d;\r\n";
  s += "</script>\r\n";
  s += "<br />\r\n";
  s += "<a href=\"/\">Home</a>";
  s += "</body>\r\n";
  s += "</html>\r\n";
  return s;
}
