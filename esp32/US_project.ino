

#include <DHT.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>

const char* ssid     = "GFiber_2.4_Coverage_8653C";
const char* password = "BE597D18";

#define SS_PIN    21  // SDA Pin
#define RST_PIN   22  // Reset Pin
MFRC522 rfrc522(SS_PIN, RST_PIN);

#define DHTPIN 13       
#define DHTTYPE DHT22   

DHT dht(DHTPIN,DHTTYPE);
float temperature=0.0;
float humidity= 0.0;
unsigned long temp_time=0;

// change pin number based on esp32
const int trigpin=1;
const int echopin=2;
const int trigpin2=7;
const int echopin2=8;

const int yellowpin=4;
const int redpin=5;
const int greenpin=3;

const int relaypin=6;// for pump,relay should output from NO

float t; //time of first sensor
int d;//distance of first sensor
float t2;// time of 2nd sensor
int d2;// distance of 2nd sensor

unsigned long pump_delay=2000;
unsigned long hand1_present_time = 0; 
unsigned long hand_present_time=0;
unsigned long last_wifi_check = 0;






void setup(){
  Serial.begin(115200);  
  SPI.begin();
  rfrc522.PCD_Init();

  pinMode(trigpin,OUTPUT);
  pinMode(echopin,INPUT);

  pinMode(yellowpin,OUTPUT);
  pinMode(redpin,OUTPUT);
  pinMode(greenpin,OUTPUT);

  pinMode(relaypin,OUTPUT);

  pinMode(trigpin2,OUTPUT);
  pinMode(echopin2,INPUT);
  
  digitalWrite(redpin,1);
  delay(50);
  digitalWrite(yellowpin,1);
  delay(50);
  digitalWrite(greenpin,1);
  delay(50);
  digitalWrite(yellowpin,0);
  digitalWrite(greenpin,0);
  
  Serial.println();
  Serial.print("Connecting to Wi-Fi network: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wi-Fi Connected successfully!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP()); // Prints the local IP address assigned to your device

  dht.begin();

  
 
}

void wifi_reconnect_checker() {
  // Check the network status every 10 seconds without stopping execution
  if (millis() - last_wifi_check >= 10000) {
    last_wifi_check = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi link lost! Reconnecting in background...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
  }
}

void temp_reader(){
  if (millis()-temp_time>=30000){
    temp_time=millis();
    
    humidity=dht.readHumidity();
    temperature=dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)){
    Serial.print("Warning:Failed to read from DHT sensor");
    }
    else{
      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.print("%  |  Temperature: ");
      Serial.print(temperature);
      Serial.println("°C");
    }
   
  }
}

bool sensor_read1(){
  digitalWrite(redpin, 1);

  digitalWrite(trigpin, 0);
  delayMicroseconds(2);
  digitalWrite(trigpin, 1);
  delayMicroseconds(10);
  digitalWrite(trigpin, 0);

  t = pulseIn(echopin, HIGH);
  d = t * 0.034 / 2;


  if (d > 0 && d <= 5){
    
    // If the hand JUST arrived, record the starting timestamp
    if (hand1_present_time == 0) {
      hand1_present_time = millis();
      Serial.println("Hand detected at Sensor 1. Hold still for 2 seconds...");
    }
    
    // Check if 2 seconds (2000ms) have passed continuously
    if (millis() - hand1_present_time >= 2000){
      digitalWrite(redpin, 0);
      digitalWrite(yellowpin, 1);
      digitalWrite(greenpin, 0);
      return true; //  Threshold passed! Return true to unlock RFID checking
    }
    
    // Hand is there, but 2 seconds haven't passed yet
    return false; 
  }
  else{
    //  BREAK CASE: Hand was pulled away or not detected. Reset the timer instantly!
    if (hand1_present_time != 0) {
      Serial.println("Sensor 1 reset: Hand removed too early.");
      hand1_present_time = 0;
    }
    
    digitalWrite(redpin, 1);
    digitalWrite(yellowpin, 0);
    Serial.println("STAND BY...");
    return false;
  }
}
void sensor_read2(){
  // if RFID is detected or true  then activate sensor 2 then gives alcohol if hand is within 5cm
  if (RFID_detector() ){
    digitalWrite(redpin,0);
    digitalWrite(yellowpin,0);
    digitalWrite(greenpin,1);

    Serial.println("Please place your hand nearby sensor 2.");

    digitalWrite(trigpin2,0);
    delayMicroseconds(2);
    digitalWrite(trigpin2,1);
    delayMicroseconds(10);
    digitalWrite(trigpin2,0);

    t2=pulseIn(echopin2,HIGH);
    d2=t2*0.034/2;
    if (d2 > 0 && d2<= 5){
      if (hand_present_time == 0) {
        hand_present_time = millis(); 
        Serial.println("Hand detected at Sensor 2. Starting 2-second countdown...");
      }
      
      // Check if the hand has stayed continuously for 2 seconds (2000ms)
      if (millis() - hand_present_time >= pump_delay) {
        Serial.println("Target time reached! Dispensing alcohol...");
        
        digitalWrite(relaypin, 1);  
        delay(1000);                 
        digitalWrite(relaypin, 0);  
        
        // Reset everything so they can't infinite-spray
        hand_present_time = 0; 
        hand1_present_time = 0;
      }
    } 
    else {
      //  BREAK CASE: The moment the hand is pulled away, reset the timer!
      if (hand_present_time != 0) {
        Serial.println("Hand removed too early! Resetting timer.");
        hand_present_time = 0; 
      }
    }
  }
}
bool RFID_detector(){
  if (sensor_read1()){
    if (!rfrc522.PCD_IsNewCardPresent()) {
    return false; 
    }
    if ( ! rfrc522.PCD_ReadCardSerial()) {
    return false;
    }
    Serial.print("Card Detected! UID Unique ID Tag: ");
    String cardUID = "";
    
    for (byte i = 0; i < rfrc522.uid.size; i++) {
    cardUID += String(rfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    cardUID += String(rfrc522.uid.uidByte[i], HEX);    
    }
    cardUID.toUpperCase();
    Serial.println(cardUID);
    rfrc522.PCD_StopCrypto1();
    return true;

  }
  return false;
}

void loop(){
  wifi_reconnect_checker();
  temp_reader();
  sensor_read2();
  

  
  
  
  
}
  
  

  
  







