#include <ESP8266WiFi.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <Firebase_ESP_Client.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2  //DS18B20 pin 2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor(&oneWire);

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "Dialog 4G 224"
#define WIFI_PASSWORD "be008FF1"

// Insert Firebase project API Key
#define API_KEY "AIzaSyBtfnH8AnbTqwBsk2L4PYSQbG8mZofQDEE"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://esp-firebase-demo-3120d-default-rtdb.firebaseio.com/"

/* 4. Define the user Email and password that already registerd or added in your project */
#define USER_EMAIL "sanjayamendis05@gmail.com"
#define USER_PASSWORD "123456789"

int counter = 0;

MAX30105 particleSensor;

const byte RATE_SIZE = 4;  //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE];     //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0;  //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int intValue;
float floatValue;
bool signupOK = false;

void firebase_init() {
  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  // auth.user.email = USER_EMAIL;
  // auth.user.password = USER_PASSWORD;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;  //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST))  //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1)
      ;
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

  particleSensor.setup();                     //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A);  //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0);   //Turn off Green LED

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to Wi-Fi: ");
  Serial.println(WiFi.localIP());

  firebase_init();
}

float temperature() {
  sensor.requestTemperatures();
  float c = sensor.getTempCByIndex(0);
  return c;
}

void loop() {
  long irValue = particleSensor.getIR();

  if (checkForBeat(irValue) == true) {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute;  //Store this reading in the array
      rateSpot %= RATE_SIZE;                     //Wrap variable

      //Take average of readings
      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }

    float c = temperature();

    String num = String(counter++);
    String url_bpm = "/MAX30102/" + num + "/BPM/";
    String url_cel = "/DS18B20/" + num + "/Celcius/";

    if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0)) {
      sendDataPrevMillis = millis();

      if (Firebase.RTDB.setFloat(&fbdo, url_bpm.c_str(), beatsPerMinute)) {
        Serial.print("BPM: ");
        Serial.print(beatsPerMinute);
        Serial.println("BPM sent to Firebase");
      } else {
        Serial.println(fbdo.errorReason());
      }

      if (Firebase.RTDB.setFloat(&fbdo, url_cel.c_str(), c)) {
        Serial.print("Temp: ");
        Serial.print(c);
        Serial.println("C sent to Firebase");
      } else {
        Serial.println(fbdo.errorReason());
      }
    }
  }
}
