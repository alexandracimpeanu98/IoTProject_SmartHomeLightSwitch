/*
 * Original sourse: https://github.com/RoboticsBrno/ESP32-Arduino-Servo-Library
 * 
 * This is Arduino code to control Servo motor with ESP32 boards.
 * Watch video instruction for this code: https://youtu.be/cDbd0ASkMog
   
 * Written/updated by Ahmad Shamshiri for Robojax Video channel www.Robojax.com
 * Date: Dec 17, 2019, in Ajax, Ontario, Canada
 * Permission granted to share this code given that this
 * note is kept with the code.
 * Disclaimer: this code is "AS IS" and for educational purpose only.
 * this code has been downloaded from http://robojax.com/learn/arduino/
 
 * Get this code and other Arduino codes from Robojax.com
Learn Arduino step by step in structured course with all material, wiring diagram and library
all in once place. Purchase My course on Udemy.com http://robojax.com/L/?id=62

****************************
Get early access to my videos via Patreon and have  your name mentioned at end of very 
videos I publish on YouTube here: http://robojax.com/L/?id=63 (watch until end of this video to list of my Patrons)
****************************

or make donation using PayPal http://robojax.com/L/?id=64

 *  * This code is "AS IS" without warranty or liability. Free to be used as long as you keep this note intact.* 
 * This code has been download from Robojax.com
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* Demonstration of Rtc_Pcf8563 Alarms. 
 *
 * The Pcf8563 has an interrupt output, Pin3.
 * Pull Pin3 HIGH with a resistor, I used a 10kohm to 5v.
 * I used a RBBB with Arduino IDE, the pins are mapped a 
 * bit differently.  Change for your hw.
 * SCK - A5, SDA - A4, INT - D3/INT1
 *
 * After loading and starting the sketch, use the serial monitor
 * to see the clock output.
 * 
 * setup:  see Pcf8563 data sheet.
 *         1x 10Kohm pullup on Pin3 INT
 *         No pullups on Pin5 or Pin6 (I2C internals used)
 *         1x 0.1pf on power
 *         1x 32khz chrystal
 *
 * Joe Robertson, jmr
 * orbitalair@bellsouth.net
 */

 /*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-date-time-ntp-client-server-arduino/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/


#include <Servo_ESP32.h>
#include <WiFi.h>
#include <Wire.h>
#include <Rtc_Pcf8563.h>
#include <WiFi.h>
#include "time.h"
#include <string>
#include <iostream>
#include "FS.h"
#include "SPIFFS.h"

// servo motor config
static const int servoPin = 14;
Servo_ESP32 myServo;
int servoDefaultAngle = 90;
int servoAngleStep = 1;
int offsetONAngle = 45;
int offsetOFFAngle = -35;
//int offsetONAngle = 30;
//int offsetOFFAngle = -30;

// web serve config
const char* ssid = "Tenda_EA6120";
const char* password = "bestgroup057";
WiFiServer server(80);                   // set server port to 80
String header;                           // stores the HTTP request header
String lightSwitchState = "OFF";         // stores the state of the light switch
unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

// NTP Server config
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;        // GMT offset of local time in seconds (2h)
const int   daylightOffset_sec = 3600;

// RTC module config
Rtc_Pcf8563 rtc;

// WAKEUP ALARM FUNCTIONALITY CONFIG
int wakeupAlarm = 0; // 0 - no alarm is set; 1- alarm is set;
char wakeupTime[6];
char wakeupDate[6];

// file logging config
#define FORMAT_SPIFFS_IF_FAILED true
char * logFilePath = "/agenda2.txt";

void setup() {
    Serial.begin(115200);

    // attach servo motor to pin
    myServo.attach(servoPin);
    // set default position
    myServo.write(servoDefaultAngle);

    // connect to wifi
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    // print local ip address to access the web server
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();

    // Config NTP server and setup RTC module
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    setupRTC();

    // setup file logging
    setupFlashFileLogging();
}

void setupFlashFileLogging () {
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
    Serial.println("SPIFFS Mount Failed");
    return;
  }
   
  readLogFile(SPIFFS, logFilePath);
}

void setupRTC(){
  // time info from NTP server
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  
  // config RTC module start time (using the time from NTP server)
  rtc.initClock();
  rtc.setDate(timeinfo.tm_mday, // day
              timeinfo.tm_wday, // weekday
              timeinfo.tm_mon + 1, // month
              0, // century (0 = 2000)
              22); // year
  rtc.setTime(timeinfo.tm_hour, // hour
              timeinfo.tm_min, // minute
              timeinfo.tm_sec); // second
}

void loop()
{
  listenForClientConnections();
  checkWakeupAlarm();
  delay(1000);
}


void listenForClientConnections()
{
  // listen for incoming clients
  WiFiClient client = server.available();

  // if any client connects
  if (client) {
    currentTime = millis();
    previousTime = currentTime;
    // holds incoming data from client
    String currentLine = "";

    // read data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {
      currentTime = millis();
      if (client.available()) {
        char c = client.read();
        header += c;
        if (c == '\n') {
          
          // end of the client HTTP request and send a response:
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turn the light ON / OFF
            if (header.indexOf("GET /light/on") >= 0) {
              lightSwitchState = "ON";
              // send ON command to servo motor
              turnLightSwitchON();
            } else if (header.indexOf("GET /light/off") >= 0) {
              lightSwitchState = "OFF";
              // send OFF command to servo motor
              turnLightSwitchOFF();
            } else if (header.indexOf("GET /setup-alarm") >= 0) {
              setupAlarm();
            } else if (header.indexOf("GET /reset-alarm") >= 0) {
              resetAlarm();
            }
            
            // display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}");
            client.println(".input {margin: 10px;}");
            client.print("</style></head>");
            // heading
            client.println("<body><h1>Smart Home - Light Switch Server</h1>");
            
            // display current state of the light: ON/OFF  
            client.println("<p>Bedroom Light is " + lightSwitchState + "</p>");
            // display button that changes the light state
            if (lightSwitchState == "OFF") {
              client.println("<p><a href=\"/light/on\"><button class=\"button\">ON</button></a></p>");
              client.println("<img src=\"https://nypost.com/wp-content/uploads/sites/2/2020/03/031120-cute-animals-anti-corona-01.jpg?quality=80&strip=all\" style=\"height:400px;\">");
            } else {
              client.println("<p><a href=\"/light/off\"><button class=\"button button2\">OFF</button></a></p>");
              client.println("<img src=\"https://media.makeameme.org/created/i-see-light-351352.jpg\" style=\"height:400px;\">");
            }

            // set wakeup alarm
            client.println("<h2 style=\"margin-top:30px;\"> Wakeup Alarm </h2>");
            if (wakeupAlarm == 0) {
              client.println("<form action=\"/setup-alarm\">");
              client.println("time: <input type=\"text\" name=\"time\" class=\"input\" minlength=\"5\" maxlength=\"5\" pattern=\"[0-9]{2}:[0-9]{2}\" placeholder=\"hh:mm\" required><br>");
              client.println("date: <input type=\"text\" name=\"date\" class=\"input\" minlength=\"5\" maxlength=\"5\" pattern=\"[0-9]{2}/[0-9]{2}\" placeholder=\"mm/dd\" required><br>");
              client.println("<input class=\"input\" type=\"submit\" value=\"Set Wakeup Alarm\">");
              client.println("</form>");
            } else {
              // reset wakeup alarm
              client.println("<form action=\"/reset-alarm\">");
              char temp[100];
              sprintf(temp, "<div class=\"input\">You have an alarm set for <br>time: %s <br>date: %s</div>", wakeupTime, wakeupDate);
              client.println(temp);
              client.println("<input class=\"input\" type=\"submit\" value=\"Reset Wakeup Alarm\">");
              client.println("</form>");
            }
            
            client.println("</body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          // read data from client
          currentLine += c; 
        }
      }
    }
    // reset the header
    header = "";
    // close the connection
    client.stop();
  }
}

void turnLightSwitchON()
{
  Serial.println("Bedroom Light Switch [SERVER] - changed state to ON");
  appendToLogFile(SPIFFS, logFilePath, "Bedroom Light Switch [SERVER] - changed state to ON");
  
  for(int angle = servoDefaultAngle; angle <= servoDefaultAngle + offsetONAngle; angle += servoAngleStep) {
    myServo.write(angle);
    delay(20);
  }

  // go back to default position
  myServo.write(servoDefaultAngle);
  delay(20); 
}


void turnLightSwitchOFF() 
{
  Serial.println("Bedroom Light Switch [SERVER] - changed state to OFF");
  appendToLogFile(SPIFFS, logFilePath, "Bedroom Light Switch [SERVER] - changed state to OFF");

  for(int angle = servoDefaultAngle; angle >= servoDefaultAngle + offsetOFFAngle; angle -= servoAngleStep) {
    myServo.write(angle);
    delay(20);
  }

  // go back to default position
  myServo.write(servoDefaultAngle);
  delay(20); 
}


// check if the alarm must be triggered or not; if yes, it is triggered
void checkWakeupAlarm()
{
  if (wakeupAlarm == 0)
    return;

  // check date and time  
  if(strncmp(wakeupTime, rtc.formatTime(), 5) == 0 && 
     strncmp(wakeupDate, rtc.formatDate(), 5) == 0) {
    turnLightSwitchON();
    resetAlarm();
  }
}

// setus up wakeup alarm data
void setupAlarm()
{
  // set time and date   
  wakeupTime[0] = header[22];
  wakeupTime[1] = header[23];
  wakeupTime[2] = ':';
  wakeupTime[3] = header[27];
  wakeupTime[4] = header[28];
  wakeupTime[5] = 0;

  wakeupDate[0] = header[35];
  wakeupDate[1] = header[36];
  wakeupDate[2] = '/';
  wakeupDate[3] = header[40];
  wakeupDate[4] = header[41];
  wakeupDate[5] = 0;

  wakeupAlarm = 1;
}

// reset alarm config variables
void resetAlarm()
{
  Serial.print("Resetting Wakeup Alarm.");
  wakeupAlarm = 0;
}

void appendToLogFile(fs::FS &fs, const char * path, const char * message)
{
  char timestamped_message [100];
  strcpy (timestamped_message, message);
  strcat (timestamped_message, " ");
  strcat (timestamped_message, rtc.formatDate());
  strcat (timestamped_message, " ");
  strcat (timestamped_message, rtc.formatTime());
  strcat (timestamped_message, "\n");
  
  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("- failed to open file for appending");
      return;
  }

  // write message and a new line after
  if(file.print(timestamped_message)){
  } else {
    Serial.println("- append failed");
  }
  file.close();
}

// can be removed; has debugging purposes
void printTimeAndDate() 
{
  Serial.println(wakeupTime);
  Serial.println(wakeupDate);
  
  Serial.print(rtc.formatTime());
  Serial.print("\r\n");
  Serial.print(rtc.formatDate());
  Serial.print("\r\n");
}


void readLogFile(fs::FS &fs, const char * path){
    Serial.printf("===== Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
    Serial.printf("===== Finished reading file");
}
