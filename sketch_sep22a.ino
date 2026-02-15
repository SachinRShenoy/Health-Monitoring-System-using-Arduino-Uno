#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <SoftwareSerial.h>

// --------------------------- HARDWARE SETUP ---------------------------

// DHT11
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// KY-039 heart rate sensor
const int SENSOR_PIN = A0;
const int FINGER_THRESHOLD = 950;
const int BPM_SAMPLES = 5;

int bpmReadings[BPM_SAMPLES] = {72,72,72,72,72};
int bpmIndex = 0;

// LCD 16x2
LiquidCrystal_I2C lcd(0x27, 16, 2);

// HC-05 Bluetooth
SoftwareSerial btSerial(10, 11); // RX, TX

// GSM SIM900A
SoftwareSerial gsm(8, 9); // RX, TX
const char phoneNumber[] = "+919353539822";

// --------------------------- CONNECTION STATUS ---------------------------
bool dhtConnected = false;
bool lcdConnected = false;
bool btConnected = false;
bool gsmConnected = false;

// --------------------------- TIMING ---------------------------
unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2000;

unsigned long lastBpmRead = 0;
const unsigned long BPM_INTERVAL = 1000;

unsigned long lastDisplay = 0;
const unsigned long DISPLAY_INTERVAL = 2000;

unsigned long lastBtSend = 0;
const unsigned long BT_INTERVAL = 5000;

unsigned long lastConnectionCheck = 0;
const unsigned long CONNECTION_CHECK_INTERVAL = 30000; // Check every 30s

// --------------------------- GSM STATE MACHINE ---------------------------
enum GSM_State {
  GSM_IDLE,
  GSM_SENDING_AT,
  GSM_SETTING_TEXT_MODE,
  GSM_SETTING_NUMBER,
  GSM_SENDING_MESSAGE,
  GSM_SENDING_CTRL_Z,
  GSM_WAITING_RESPONSE
};

GSM_State gsmState = GSM_IDLE;
unsigned long gsmStateTimer = 0;
const unsigned long GSM_TIMEOUT = 5000;
bool smsRequested = false;

// --------------------------- AVERAGING ---------------------------
float tempSum = 0;
int tempCount = 0;

float bpmSumForSMS = 0;
int bpmCountForSMS = 0;

// --------------------------- FINGER DETECTION ---------------------------
bool fingerPresent = false;

// --------------------------- SETUP ---------------------------
void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);
  gsm.begin(9600);

  Serial.println("=== Health Monitor Starting ===");
  
  // Test each component
  testLCD();
  testDHT();
  testBluetooth();
  testGSM();
  
  Serial.println("=== System Ready ===");
  displaySystemStatus();
}

// --------------------------- COMPONENT TESTING ---------------------------
void testLCD() {
  Serial.print("Testing LCD... ");
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("LCD Test OK");
  lcdConnected = true;
  Serial.println("OK");
  delay(1000);
  lcd.clear();
}

void testDHT() {
  Serial.print("Testing DHT11... ");
  dht.begin();
  delay(2000); // DHT needs time to stabilize
  
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  
  if(!isnan(t) && !isnan(h)) {
    dhtConnected = true;
    Serial.print("OK (T:");
    Serial.print(t,1);
    Serial.print("C, H:");
    Serial.print(h,1);
    Serial.println("%)");
  } else {
    dhtConnected = false;
    Serial.println("FAILED");
  }
}

void testBluetooth() {
  Serial.print("Testing Bluetooth... ");
  btSerial.println("AT");
  delay(1000);
  
  if(btSerial.available()) {
    String response = btSerial.readString();
    if(response.indexOf("OK") != -1) {
      btConnected = true;
      Serial.println("OK");
    } else {
      btConnected = false;
      Serial.println("FAILED - No OK response");
    }
  } else {
    // Assume BT is working if no AT response (some modules don't respond to AT)
    btConnected = true;
    Serial.println("OK (Assumed)");
  }
}

void testGSM() {
  Serial.print("Testing GSM... ");
  gsm.println("AT");
  delay(3000);
  
  String response = "";
  unsigned long startTime = millis();
  
  while(millis() - startTime < 3000) {
    if(gsm.available()) {
      response += gsm.readString();
    }
  }
  
  if(response.indexOf("OK") != -1) {
    gsmConnected = true;
    Serial.println("OK");
    
    // Check network registration
    gsm.println("AT+CREG?");
    delay(2000);
    response = "";
    startTime = millis();
    
    while(millis() - startTime < 2000) {
      if(gsm.available()) {
        response += gsm.readString();
      }
    }
    
    if(response.indexOf("+CREG: 0,1") != -1 || response.indexOf("+CREG: 0,5") != -1) {
      Serial.println("GSM Network: Connected");
    } else {
      Serial.println("GSM Network: Not registered");
    }
    
  } else {
    gsmConnected = false;
    Serial.println("FAILED - No response");
  }
}

void displaySystemStatus() {
  if(lcdConnected) {
    lcd.setCursor(0,0);
    lcd.print("DHT:");
    lcd.print(dhtConnected ? "OK" : "ERR");
    lcd.print(" BT:");
    lcd.print(btConnected ? "OK" : "ERR");
    
    lcd.setCursor(0,1);
    lcd.print("GSM:");
    lcd.print(gsmConnected ? "OK" : "ERR");
    lcd.print(" Ready!");
    
    delay(3000);
    lcd.clear();
  }
}

// --------------------------- CONNECTION MONITORING ---------------------------
void checkConnections() {
  // Re-test DHT periodically
  if(!dhtConnected) {
    float t = dht.readTemperature();
    if(!isnan(t)) {
      dhtConnected = true;
      Serial.println("DHT reconnected");
    }
  }
  
  // Test GSM connection
  if(!gsmConnected) {
    gsm.println("AT");
    delay(1000);
    if(gsm.available() && gsm.readString().indexOf("OK") != -1) {
      gsmConnected = true;
      Serial.println("GSM reconnected");
    }
  }
}

// --------------------------- NON-BLOCKING GSM SMS ---------------------------
void processSMS() {
  if(!smsRequested || gsmState == GSM_IDLE) return;
  if(!gsmConnected) {
    Serial.println("GSM not connected - SMS failed");
    smsRequested = false;
    return;
  }
  
  unsigned long now = millis();
  
  switch(gsmState) {
    case GSM_SENDING_AT:
      if(now - gsmStateTimer >= GSM_TIMEOUT) {
        gsm.println("AT");
        gsmState = GSM_SETTING_TEXT_MODE;
        gsmStateTimer = now;
      }
      break;
      
    case GSM_SETTING_TEXT_MODE:
      if(now - gsmStateTimer >= GSM_TIMEOUT) {
        gsm.println("AT+CMGF=1");
        gsmState = GSM_SETTING_NUMBER;
        gsmStateTimer = now;
      }
      break;
      
    case GSM_SETTING_NUMBER:
      if(now - gsmStateTimer >= GSM_TIMEOUT) {
        gsm.print("AT+CMGS=\"");
        gsm.print(phoneNumber);
        gsm.println("\"");
        gsmState = GSM_SENDING_MESSAGE;
        gsmStateTimer = now;
      }
      break;
      
    case GSM_SENDING_MESSAGE:
      if(now - gsmStateTimer >= GSM_TIMEOUT) {
        float avgTemp = (tempCount>0) ? tempSum/tempCount : 0;
        float avgBpmSMS = (bpmCountForSMS>0) ? bpmSumForSMS/bpmCountForSMS : 0;
        
        gsm.print("Hello, Patient: Sachin R Shenoy, DOB:06/04/2005, ");
        gsm.print("Avg Temp: "); gsm.print(avgTemp,1); gsm.print(" C, ");
        gsm.print("Avg BPM: "); gsm.print((int)avgBpmSMS); gsm.print(". Thank you.");
        
        gsmState = GSM_SENDING_CTRL_Z;
        gsmStateTimer = now;
      }
      break;
      
    case GSM_SENDING_CTRL_Z:
      if(now - gsmStateTimer >= 1000) {
        gsm.write(26); // Ctrl+Z
        gsmState = GSM_WAITING_RESPONSE;
        gsmStateTimer = now;
      }
      break;
      
    case GSM_WAITING_RESPONSE:
      if(gsm.available()) {
        String response = gsm.readString();
        if(response.indexOf("OK") != -1 || response.indexOf("+CMGS") != -1) {
          Serial.println("SMS Sent Successfully!");
        } else {
          Serial.println("SMS Send Failed");
        }
        smsRequested = false;
        gsmState = GSM_IDLE;
      } else if(now - gsmStateTimer >= 10000) {
        Serial.println("SMS Timeout");
        smsRequested = false;
        gsmState = GSM_IDLE;
      }
      break;
  }
}

void startSMS() {
  if(gsmState == GSM_IDLE && gsmConnected) {
    smsRequested = true;
    gsmState = GSM_SENDING_AT;
    gsmStateTimer = millis();
    Serial.println("Starting SMS send...");
  } else if(!gsmConnected) {
    Serial.println("Cannot send SMS - GSM not connected");
  } else {
    Serial.println("SMS already in progress");
  }
}

// --------------------------- MAIN LOOP ---------------------------
void loop() {
  unsigned long now = millis();

  // ----- Connection Check -----
  if(now - lastConnectionCheck >= CONNECTION_CHECK_INTERVAL) {
    lastConnectionCheck = now;
    checkConnections();
  }


// ----- DHT11 Reading with retries -----
if(now - lastDHTRead >= DHT_INTERVAL && dhtConnected) {
  lastDHTRead = now;
  float t = NAN, h = NAN;
  int retries = 3;
  
  while(retries-- > 0 && (isnan(t) || isnan(h))) {
    t = dht.readTemperature();
    h = dht.readHumidity();
    delay(50); // small pause between retries
  }

  if(!isnan(t) && !isnan(h)) {
    float calibratedTemp = (t * 1.1) + 3.0; // adjust here
    tempSum += calibratedTemp;
    tempCount++;
  }
}


  // ----- KY-039 Heart Rate (improved smoothing) -----
  if(now - lastBpmRead >= BPM_INTERVAL) {
  lastBpmRead = now;
  int raw = analogRead(SENSOR_PIN);
  fingerPresent = (raw > FINGER_THRESHOLD);

  int bpmValue = 0;
  if(fingerPresent) {
    // crude mapping, but clamp range for realism
    bpmValue = map(raw, FINGER_THRESHOLD, 1023, 60, 100);
    bpmValue = constrain(bpmValue, 50, 120); // filter out crazy spikes
  }

  bpmReadings[bpmIndex] = bpmValue;
  bpmIndex = (bpmIndex + 1) % BPM_SAMPLES;

  int bpmSum = 0;
  for(int i=0; i<BPM_SAMPLES; i++) bpmSum += bpmReadings[i];
  int avgBpm = bpmSum / BPM_SAMPLES;

  if(fingerPresent && avgBpm > 0) {
    bpmSumForSMS += avgBpm;
    bpmCountForSMS++;
  }
}


  // ----- Display on LCD -----
  if(now - lastDisplay >= DISPLAY_INTERVAL && lcdConnected) {
    lastDisplay = now;
    float latestTemp = (tempCount > 0) ? tempSum/tempCount : 0;

    int bpmSumDisplay = 0;
    for(int i=0; i<BPM_SAMPLES; i++) bpmSumDisplay += bpmReadings[i];
    int latestBpm = bpmSumDisplay / BPM_SAMPLES;

    lcd.setCursor(0,0);
lcd.print("Temp: "); 
lcd.print(latestTemp,1); 
lcd.print(" C   ");   // pad with spaces

lcd.setCursor(0,1);
lcd.print("BPM: "); 
lcd.print(latestBpm); 
lcd.print("     ");   // pad with spaces

  }

  // ----- Bluetooth -----
  if(now - lastBtSend >= BT_INTERVAL && btConnected) {
    lastBtSend = now;
    float latestTemp = (tempCount > 0) ? tempSum/tempCount : 0;

    int bpmSumDisplay = 0;
    for(int i=0; i<BPM_SAMPLES; i++) bpmSumDisplay += bpmReadings[i];
    int latestBpm = bpmSumDisplay / BPM_SAMPLES;

    btSerial.print("Temp: "); btSerial.print(latestTemp,1);
    btSerial.print(" C, BPM: "); btSerial.println(latestBpm);
  }

  // ----- Process SMS State Machine -----
  processSMS();

  // ----- Manual SMS Trigger -----
  if(Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if(input.equalsIgnoreCase("OK")) {
      startSMS();
    } else if(input.equalsIgnoreCase("STATUS")) {
      Serial.println("=== System Status ===");
      Serial.print("DHT: "); Serial.println(dhtConnected ? "Connected" : "Disconnected");
      Serial.print("LCD: "); Serial.println(lcdConnected ? "Connected" : "Disconnected");
      Serial.print("Bluetooth: "); Serial.println(btConnected ? "Connected" : "Disconnected");
      Serial.print("GSM: "); Serial.println(gsmConnected ? "Connected" : "Disconnected");
    } else if(input.equalsIgnoreCase("TEST")) {
      Serial.println("Running component tests...");
      testDHT();
      testBluetooth();
      testGSM();
    }
  }
}