#include "BluetoothSerial.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <DHT.h>
#include <Wire.h>
#include <RTClib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <PubSubClient.h>

////////////////////////// DHT22 ///////////////////////////////////////
#define DHT_SENSOR_PIN 2 //15 // ESP32 pin GIOP21 connected to DHT11 sensor
#define DHT_SENSOR_TYPE DHT22
////////////////////////////////////////////////////////////////////////

////////////////////////// DS1820 ///////////////////////////////////////
#define ONE_WIRE_BUS 4
///////////////////////////////////////////////////////////////////////
// Replace the next variables with your SSID/Password combination
//String serveripaddressstr = "192.168.111.99";

WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "BCRFID";//BCRFID
const char* password = "PikSwart99!"; //PikSwart99!
const char* mqtt_server = "nodered.belgiumcampus.ac.za";//192.168.100.116
bool reconnected = false;

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;
DHT dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

OneWire oneWire(ONE_WIRE_BUS);       // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature.

String UnitID = "2";
int numberOfDevices;
DeviceAddress tempDeviceAddress;

void printAddress(DeviceAddress deviceAddress);

RTC_DS3231 rtc; 
// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  Serial.begin(115200);
  rtc.begin();
  rtc.adjust(DateTime(2022,6,28,12,0,0));  
  dht_sensor.begin(); // initialize the DHT sensor
  initializeSensors();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  //client.setCallback(callback);
  SerialBT.begin("ESP32Unit1"); //Bluetooth device name
 // Serial.println("The device started, now you can pair it with bluetooth!");
 if(!SD.begin(5)){
 //Serial.println("Card Mount Failed");
    return;
   }
 // pinMode(LED_BUILTIN, OUTPUT);
}

void initializeSensors()
{
  // Start up the library's
  sensors.begin();
  numberOfDevices = sensors.getDeviceCount();
  //locate devices on the bus
  //Serial.print(F("Locating devices..."));
  //Serial.print("Found ");
  //Serial.print(numberOfDevices, DEC);
  //Serial.println(" device/  s.");
  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (sensors.getAddress(tempDeviceAddress, i))
    {
     //Serial.print("Found device ");
     //Serial.print(i, DEC);
     //Serial.print(" with address: ");
     //printAddress(tempDeviceAddress);
     //Serial.println("/////////Printed OneWire////////////////");
    }
    else
    {
     //Serial.print("Found ghost device at ");
     //Serial.print(i, DEC);
     //Serial.print(" but could not detect address. Check power and cabling");
    }
  }
     //Serial.println("Sensors are initialized");
}
// the loop function runs over and over again forever
void loop() 
{
 // digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  //delay(2000);                       // wait for a second
 // digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
 // delay(2000);

  if(WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }
 // if(reconnected == false){
 //   if (!client.connected()) {     
 //     reconnect(); 
 //   }  
 // }
  client.loop();
 //////////////////////////////////////////////////////////////////////////////////////
  // read humidity
  float humiDHT22  = dht_sensor.readHumidity();
  // read temperature in Celsius
  float tempCDHT22 = dht_sensor.readTemperature();
   // check whether the reading is successful or not
  if ( isnan(tempCDHT22) || isnan(humiDHT22)) 
  {
       Serial.println("Failed to read from DHT sensor!");
  } 
  else 
  {
    
    char tempString[8];
    dtostrf(tempCDHT22, 1, 2, tempString);
 //   Serial.print("Temperature: ");
//    Serial.println(tempString);
    // Convert the value to a char array
    char humString[8];
    dtostrf(humiDHT22, 1, 2, humString);
//    Serial.print("Humidity: ");
//    Serial.println(humString);
    //client.publish("esp32/humidity", humString);
    //char DataString[100];
    DateTime dtt = rtc.now();
    //Serial.print(dtt.year(),DEC);
    char buff[] = "YYYY-MM-DDThh:mm:ss";
    Serial.println(dtt.toString(buff));
    String timeString = dtt.toString(buff);
    String DataString = UnitID + ';' + timeString + ';' + tempString + ';' + humString;
   
   sensors.requestTemperatures();
   for (int i = 0; i < numberOfDevices; i++)
    {
      // Search the wire for address
      if (sensors.getAddress(tempDeviceAddress, i))
      {
   //     Serial.print("Found device \n");
        Serial.print(i, DEC);
        float tempOnewire  = sensors.getTempC(tempDeviceAddress);
        DataString += ';';
        DataString += String(tempOnewire,0);       
      }
    }   

    DataString += '\n';
    char dString[50];
    DataString.toCharArray(dString,50);
    /////////////////////////////////////////////////////////////////////////////
 //   Serial.println(dString);
    client.disconnect();
    reconnect(); 
    client.publish("data", dString);
    appendFile(SD, "/Data.txt", dString);
  }

  // wait a 2 seconds between readings
  delay(2000); 
 
 if (Serial.available()) {
  SerialBT.write(Serial.read());
  }
  if (SerialBT.available()) {
    Serial.write(SerialBT.read());
    writeFile(SD, "/Data.txt","tone");
  }
  delay(20);
  // wait for a second
}

void reconnect() {
 
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe
      client.setKeepAlive(1);
      //client.subscribe("esp32/readcard");
      client.subscribe("data");     
      reconnected =true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      reconnected = false;
    }
  }
}
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Mac address: ");
  Serial.println(WiFi.macAddress());  
}

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16)
      Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
///////////////// SD Card WriteFile //////////////////////////////////
void writeFile(fs::FS &fs, const char * path, const char * message){
 // Serial.printf("Writing file: %s\n", path);
  File file = fs.open(path, FILE_WRITE);
  if(!file){
 //   Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
 //   Serial.println("File written");
  } else {
 //   Serial.println("Write failed");
  }
  file.close();
}
///////////////// SD Card AppendFile //////////////////////////////////
void appendFile(fs::FS &fs, const char * path, const char * message){
//  Serial.printf("Appending to file: %s\n", path);
  File file = fs.open(path, FILE_APPEND);
  if(!file){
 //   Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
 //     Serial.println("Message appended");
  } else {
 //   Serial.println("Append failed");
  }
  file.close();
}

///////////////// SD Card DeleteFile //////////////////////////////////
void deleteFile(fs::FS &fs, const char * path){
//  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
 //   Serial.println("File deleted");
  } else {
  //  Serial.println("Delete failed");
  }
}
