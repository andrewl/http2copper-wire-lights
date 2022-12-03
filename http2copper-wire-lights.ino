#include "secrets.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// To speak to the outside world
WiFiClient espClient;

// Create AsyncWebServer object on port 80
ESP8266WebServer server(80);

// Pick any 2 pins that can be commanded with analogWrite (on Wemos D1 mini
// this should be any pins)
int pin1 = D5;
int pin2 = D8;

// pattern - a Struct containing the variables that define a pattern of lights
typedef struct {
  int brightnessDelta; //the change in brightness between steps 0-255
  int stepDelay; //delay between steps in milliseconds
  int initialBrightness; //the initial brightness
} pattern;


// This is where the patterns are defined
pattern patterns[7] = {
  // Off
  {
    .brightnessDelta = 0,
    .stepDelay = 10000000,
    .initialBrightness = 0,
  },
  // Permanently on
  {
    .brightnessDelta = 0,
    .stepDelay = 100,
    .initialBrightness = 255,
  },
  // Flashing, every 1 second
  {
    .brightnessDelta = 255,
    .stepDelay = 1000,
    .initialBrightness = 255,
  },
 // Flashing, every 2.5 second
  {
    .brightnessDelta = 255,
    .stepDelay = 2500,
    .initialBrightness = 255,
  },
  // Fast Increment
  {
    .brightnessDelta = 50,
    .stepDelay = 100,
    .initialBrightness = 255,
  },
  // Slow Increment
  {
    .brightnessDelta = 5,
    .stepDelay = 80,
    .initialBrightness = 255,
  },
};


int patternsCount;

// These all contain application state
int currentPatternIdx;
int nextBrightness;
int brightnessModifier;
int curPin;
int waitUntilMillis = 0;
pattern curPattern;

void setup() {
  Serial.begin(115200);
  pinMode(pin1, OUTPUT);
  pinMode(pin2, OUTPUT);

  brightnessModifier = -1;
  curPin = pin1;

  patternsCount = sizeof(patterns) / sizeof(pattern);
  currentPatternIdx = 1;

  changePattern(currentPatternIdx);

  setup_wifi();
  setup_routes();
}

void loop() {

  // Listen for new commands on the http API
  server.handleClient();

  // if we've not hit the time at which we need to do something else
  // to the lights then just return
  if (waitUntilMillis > millis()) {
    return;
  }

  // We're at the time that we need to execute the next step on the 
  // display of the lights

  // First, write the current brightness to the current pin
  analogWrite(curPin, nextBrightness);

  // If we've hit the minimum brightness then switch polarity
  // by switching the current pin
  if (nextBrightness == 0) {
    if (curPin == pin1) {
      curPin = pin2;
    }
    else {
      curPin = pin1;
    }
  }

  // Change the brightness to set next time around by applying the modifier
  // (which determined whether we're getting brighter or dimmer) to the 
  // brightness delta (which determines how fast the lights get brighter or 
  // dimmer.
  nextBrightness += (brightnessModifier * patterns[currentPatternIdx].brightnessDelta);

  // If the brightness is now 255 or more, pin the brightness to 255 (the max)
  // then next time start getting dimmer
  if (nextBrightness > 254) {
    nextBrightness = 255;
    brightnessModifier = -1;
  }

  // If the brightness is now 0 or less, pin the brightness to 0 then next time
  // start getting brighter
  if (nextBrightness < 1) {
    nextBrightness = 0;
    brightnessModifier = 1;
  }

  // Determine when we need to set the brightness of the lights again
  waitUntilMillis = millis() + patterns[currentPatternIdx].stepDelay;

}

void setup_wifi() {

  delay(10);

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to WiFi ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_routes() {

  // Start Web Server
  server.on("/", getCurrentPattern);
  server.on("/next", nextPattern);
  server.on("/random", randomPattern);
  server.on("/off", offPattern);

  server.onNotFound(notFoundPage);
  server.begin();
}

// Switch to the next pattern
void nextPattern() {
  currentPatternIdx++;
  if (currentPatternIdx >= patternsCount) {
    currentPatternIdx = 0;
  }
  changePattern(currentPatternIdx);
  server.send(200, "text/json", curPatternToJSONString());
}

// Returns the current pattern
void getCurrentPattern() {
  server.send(200, "text/json", curPatternToJSONString());
}

// Changes to the "off" pattern (pattern 0)
void offPattern() {
  changePattern(0);
  server.send(200, "text/json", curPatternToJSONString());
}

// Selects a random pattern
void randomPattern() {
  int randomPatternIndex = random(1,patternsCount-1);
  changePattern(randomPatternIndex);
  server.send(200, "text/json", curPatternToJSONString());
}

// Not found page
void notFoundPage() {
  server.send(404, "text/plain", "Not found");
}

// Change the current pattern to the one specified. If the specified
// pattern is out of range, select pattern 0.
void changePattern(int newPatternIdx) {
  if (newPatternIdx > patternsCount || newPatternIdx < 0) {
    newPatternIdx = 0;
  }
  curPattern = patterns[newPatternIdx];
  currentPatternIdx = newPatternIdx;
  nextBrightness = curPattern.initialBrightness;
  waitUntilMillis = 0;
}

//formats a pattern as a json String
String curPatternToJSONString() {
  String initialBrightness = "\"initialBrightness\" : " + String(curPattern.initialBrightness);
  String brightnessDelta = "\"brightnessDelta\" : " + String(curPattern.brightnessDelta);
  String stepDelay = "\"stepDelay\" : " + String(curPattern.stepDelay);
  return "{\n  " + initialBrightness + ",\n  " + brightnessDelta + ",\n  " + stepDelay + "\n}";
}
