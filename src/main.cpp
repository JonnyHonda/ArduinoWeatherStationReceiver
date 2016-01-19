/*
  Receives from the hardware serial, sends to software serial.
  Receives from software serial, sends to hardware serial.

  The circuit:
   RX is digital pin 14 (connect to TX of other device)
   TX is digital pin 12 (connect to RX of other device)

*/

#include "configuration.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>   
#include <SoftwareSerial.h>

String getClientOutput(void);
String getcsvOutput(void);

SoftwareSerial mySerial(14, 12); // RX, TX
int green_led = 5;
int red_led = 4;

const char* ssid = SSSID;
const char* password = PASSWORD;

double temperature = 0.00;
double pressure = 0.00;
int windDir = 0;
double windSpeed = 0.0;
int humidity = 0;
int lightValue = 0;
unsigned long int rainTipperCounter = 0;
unsigned long int lastUpdate = 0;

String s = "";

WiFiServer server(80);
void setup()
{
  pinMode(green_led, OUTPUT);
  pinMode(red_led, OUTPUT);

#ifdef DEBUG
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  Serial.println("Goodnight moon!");
#endif
  // set the data rate for the SoftwareSerial port
  mySerial.begin(9600);
  // Create an instance of the server
  // specify the port to listen on as an argument


  // Connect to WiFi network
#ifdef DEBUG
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
#endif
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(red_led, HIGH);
    digitalWrite(green_led, HIGH);
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif
    digitalWrite(red_led, LOW);
    digitalWrite(green_led, LOW);
  }
#ifdef DEBUG
  Serial.println("");
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
  digitalWrite(red_led, LOW);
  digitalWrite(green_led, LOW);
}
byte dataByte;

byte buffer[24];
int bufferindex = 0;

// I'v define a packet to be as follows
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
  byte header[3];
  byte data[14];
  byte checksum[2];
}my_packet;


void loop() // run over and over
{
  lastUpdate = millis() / 1000;
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
      for (int i = 0; i < 14; ++i) {
        checksum += my_packet.data[i];
      }
      if (checksum == ( my_packet.checksum[0] << 8 ) + my_packet.checksum[1]) {
        digitalWrite(green_led, HIGH);
#ifdef DEBUG
        Serial.println("Checksum valid");
#endif
        temperature = ( my_packet.data[0] << 8 ) + my_packet.data[1];
        temperature = (temperature / 10) - 120;
#ifdef DEBUG
        Serial.print("Temperature = ");
        Serial.println(temperature);
#endif

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

        humidity = my_packet.data[5];
#ifdef DEBUG
        Serial.print("Humidity = ");
        Serial.print(humidity);
        Serial.println("%");
#endif


        windSpeed = ( my_packet.data[10] << 8 ) + my_packet.data[11];
        windSpeed = (windSpeed / 100) - 45;
#ifdef DEBUG
        Serial.print("Wind Speed = ");
        Serial.println(windSpeed);
#endif


        // Rain tipper counter
        // unsigned long int rainTipperCounter;
        rainTipperCounter = my_packet.data[6];
        rainTipperCounter = (rainTipperCounter << 8) + my_packet.data[7];
        rainTipperCounter = (rainTipperCounter << 8) + my_packet.data[8];
        rainTipperCounter = (rainTipperCounter << 8) + my_packet.data[9];

#ifdef DEBUG
        Serial.print("Rain Tipper Counter = ");
        Serial.println(rainTipperCounter );
        
#endif

        lightValue = ( my_packet.data[12] << 8 ) + my_packet.data[13];

#ifdef DEBUG
        Serial.print("Light Value = ");
        Serial.println(lightValue);
        Serial.print(lastUpdate);
#endif
      }

      bufferindex = 0;
      digitalWrite(green_led, LOW);
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
    while (client.connected()) {
      if (client.available()) {
#ifdef DEBUG
        Serial.println("client is available");
#endif
        // Read the first line of the request
        String req = client.readStringUntil('\r');
#ifdef DEBUG
        Serial.println(req);
#endif
        client.flush();
        // Prepare the response
        digitalWrite(red_led, HIGH);
        // Match the request
        if (req.indexOf("/csv") != -1) {
          s = getcsvOutput();
        } else {
          s = getClientOutput();
        }
        client.print(s);
        delay(1);
        client.stop();
#ifdef DEBUG
        Serial.println("Client disconnected");
#endif
        digitalWrite(red_led, LOW);
      }
    }
  }
}


String getcsvOutput() {
  s = "";
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
  s += lastUpdate;
  return s;
}

String getClientOutput() {
  s = "HTTP/1.1 200 OK\r\n";
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

  s += "Last Update: \r\n";

  s += lastUpdate;
  s += "<br />\r\n";
  s += "</body>\r\n";
  s += "</html>\r\n";

  return s;
}

