#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <SoftwareSerial.h>
#include "TinyGPS.h"


MAX30105 particleSensor;
TinyGPS gps;
SoftwareSerial ss(4, 5);
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;
  bool newData = false;
  unsigned long chars;
  unsigned short sentences, failed;
    float flat, flon;
    unsigned long age;
    float bodyTemp;

#if SIMULATED_DATA

void initSensor()
{
    // use SIMULATED_DATA, no sensor need to be inited
}

float readTemperature()
{
    return random(20, 30);
}

float readHumidity()
{
    return random(30, 40);
}

#else

static DHT dht(DHT_PIN, DHT_TYPE);
void initSensor()
{
//    dht.begin();
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

  particleSensor.setup(); //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED
  ss.begin(9600);

}
#endif
float readHeartBeatTest()
{
long irValue = particleSensor.getIR();

  if (checkForBeat(irValue) == true)
  {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable

      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }
  bodyTemp = particleSensor.readTemperature();

  Serial.print("temperatureC=");
  Serial.print(bodyTemp);

  Serial.print("IR=");
  Serial.print(irValue);
  Serial.print(", BPM=");
  Serial.print(beatsPerMinute);
  Serial.print(", Avg BPM=");
  Serial.print(beatAvg);

  if (irValue < 50000)
  {
     Serial.print(" No finger?");
     beatsPerMinute=0;
     messageSending = false;
  }
 
  Serial.println();
  
 }


float readGPSTest()
{
  // For one second we parse GPS data and report some key values
  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (ss.available())
    {
      char c = ss.read();
      // Serial.write(c); // uncomment this line if you want to see the GPS data flowing
      if (gps.encode(c)) // Did a new valid sentence come in?
        newData = true;
    }
  }

  if (newData)
  {

    gps.f_get_position(&flat, &flon, &age);
    Serial.print("LAT=");
    Serial.print(flat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flat, 6);
    Serial.print(" LON=");
    Serial.print(flon == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : flon, 6);
  }
   gps.stats(&chars, &sentences, &failed);
  if (chars == 0){
    Serial.println("** No characters received from GPS: check wiring **");
    flat=0;flon=0;
     messageSending = false;
}
 
  Serial.println();
 }

bool readMessage(int messageId, char *payload)
{
    float temperature;
    float bodytemp;
    float latitude, longitude;
    float avgbpm;
    StaticJsonBuffer<MESSAGE_MAX_LEN> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    uint32_t period = 5000L;       // 3 seconds sense
    for( uint32_t tStart = millis();  (millis()-tStart) < 5000L;  ){
     readHeartBeatTest();
//     readGPSTest();
   }
    readGPSTest();
//    if( flat!=0 && flon!=0) {
    messageSending = true;
    temperature = beatsPerMinute;
    latitude = flat;
    longitude = flon;
    bodytemp = bodyTemp;
    avgbpm = beatAvg;

    root["deviceId"] = DEVICE_ID;
    root["messageId"] = messageId;
    bool temperatureAlert = false;
   

    // NAN is not the valid json, change it to NULL
    if (std::isnan(temperature))
    {
        root["temperature"] = NULL;
    }
    else
    {
        root["temperature"] = temperature;
       
            temperatureAlert = true;
      
    }

    if (std::isnan(bodytemp))
    {
        root["bodytemp"] = NULL;
    }
    else
    {
        root["bodytemp"] = bodytemp;
    }
    if (std::isnan(avgbpm))
    {
        root["avgBpm"] = NULL;
    }
    else
    {
        root["avgBpm"] = avgbpm;
    }
    if (std::isnan(latitude))
    {
        root["latitude"] = NULL;
        root["longitude"] = NULL;
    }
    else
    {
        root["latitude"] = latitude;
        root["longitude"] = longitude;
    }
    if (beatsPerMinute >100)
    {
     String alert = "Heart Rate is high! Need attention"; 
     root["alert"] = alert;
     digitalWrite(16,HIGH) ;
     }
     else{
      digitalWrite(16,LOW) ;
     }

    root.printTo(payload, MESSAGE_MAX_LEN);
    return temperatureAlert;
//    }
//    else
//    return 0;
}

void parseTwinMessage(char *message)
{
    StaticJsonBuffer<MESSAGE_MAX_LEN> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(message);
    if (!root.success())
    {
        Serial.printf("Parse %s failed.\r\n", message);
        return;
    }

    if (root["desired"]["interval"].success())
    {
        interval = root["desired"]["interval"];
    }
    else if (root.containsKey("interval"))
    {
        interval = root["interval"];
    }
}
