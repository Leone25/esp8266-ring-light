#include <Adafruit_NeoPixel.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// Config start

// D4 is the builtin led, don't use it
#define LED_N 36
#define LED_OFFSET 0
#define LED_DATA D5
#define LED_MODE NEO_GRBW + NEO_KHZ800
#define BUTTON_PIN D3
#define WIFI_SSID "test-wifi"
#define WIFI_PWD "ipad2015"
#define TIMEZONE 3600
#define ANIMATION_BUFFER LED_N*10
// Config end


// Display mode
#define DISPLAY_TIME 0
#define DISPLAY_LIGHT 1


// callback identifiers
#define IDENTIFIER_NONE 0
#define IDENTIFIER_MINUTE_OFF 1
#define IDENTIFIER_MINUTE_ON 2
#define IDENTIFIER_HOUR_OFF 3
#define IDENTIFIER_HOUR_ON 4
#define IDENTIFIER_LIGHT 5

struct pixelAnimation {
  int n;
  byte startRed;
  byte startGreen;
  byte startBlue;
  byte startWhite;
  byte stopRed;
  byte stopGreen;
  byte stopBlue;
  byte stopWhite;
  unsigned long start;
  unsigned long stop;
  int identifier;
};

pixelAnimation animations[ANIMATION_BUFFER];
int activeAnimations = 0;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(LED_N, LED_DATA, LED_MODE);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

void stopAllAnimations();
void animateBlock(int start, int stop, byte red, byte green, byte blue, byte white, int duration, int identifierLast = IDENTIFIER_NONE, int offset = 0);
void animateFollow(int start, int stop, byte red, byte green, byte blue, byte white, int duration, int identifierLast = IDENTIFIER_NONE, int offset = 0);
void addAnimation(int n, byte stopRed, byte stopGreen, byte stopBlue, byte stopWhite, int duration, int identifier = IDENTIFIER_NONE, int offset = 0);
void addAnimationFrom(int n, byte startRed, byte startGreen, byte startBlue, byte startWhite, byte stopRed, byte stopGreen, byte stopBlue, byte stopWhite, int duration, int identifier = IDENTIFIER_NONE, int offset = 0);
void animationEndCallback(int identifier, int led);
;void updateLights();

int currentDisplay = DISPLAY_TIME;

void setup() {
  pixels.begin();
  pixels.clear();

  Serial.begin(9600);

  WiFi.begin(WIFI_SSID, WIFI_PWD);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    Serial.print ( "." );
  }
  digitalWrite(LED_BUILTIN, 1);
  
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  timeClient.begin();
  timeClient.setTimeOffset(TIMEZONE);
  
  // this triggers the clock hands to show
  addAnimation(0, 0, 0, 0, 0, 2000, IDENTIFIER_MINUTE_OFF);
  addAnimation(0, 0, 0, 0, 0, 2000, IDENTIFIER_HOUR_OFF);
}

void loop() {
  
  timeClient.update();
    
  if (!digitalRead(BUTTON_PIN)) {
    delay(200);
    if (currentDisplay == DISPLAY_TIME) {
      currentDisplay = DISPLAY_LIGHT;
      stopAllAnimations();
      animateBlock(0, LED_N, 0, 0, 0, 0, 500); // turn off everything that is on
      animateFollow(0, LED_N/2, 0, 0, 0, 255, 500, IDENTIFIER_NONE, 500);
      animateFollow(LED_N, LED_N/2, 0, 0, 0, 255, 500, IDENTIFIER_LIGHT, 500);
    } else if (currentDisplay == DISPLAY_LIGHT) {
      currentDisplay = DISPLAY_TIME;
      stopAllAnimations();
      animateBlock(0, LED_N, 0, 0, 0, 0, 500); // turn off everything that is on
      addAnimation(0, 0, 0, 0, 0, 0, IDENTIFIER_MINUTE_OFF, 500);
      addAnimation(0, 0, 0, 0, 0, 0, IDENTIFIER_HOUR_OFF, 500);
    }
    Serial.println(currentDisplay);
  }

  updateLights();

  delay(10);
}

void stopAllAnimations() {
  activeAnimations = 0;
}

void animateBlock(int start, int stop, byte red, byte green, byte blue, byte white, int duration, int identifierLast, int offset) {
  int n = abs(start-stop);
  for (int j = 0; j < n; j++) {
    if (j == n - 1) {
      addAnimation((start+j)%LED_N, red, green, blue, white, duration, identifierLast, offset);
    } else {
      addAnimation((start+j)%LED_N, red, green, blue, white, duration, IDENTIFIER_NONE, offset);
    }
  }
}

void animateFollow(int start, int stop, byte red, byte green, byte blue, byte white, int duration, int identifierLast, int offset) {
  int numLeds;
  int increment;
  if (start > stop) {
    numLeds = start - stop + 1;
    increment = -1;
  } else {
    numLeds = stop - start + 1;
    increment = 1;
  }
  int delay = duration / numLeds;
  for (int i = start; i != stop + increment; i += increment) {
    addAnimation(i, red, green, blue, white, delay, identifierLast, offset);
    offset += delay;
  }
}


void addAnimation(int n, byte stopRed, byte stopGreen, byte stopBlue, byte stopWhite, int duration, int identifier, int offset) {
  if (activeAnimations-1 >= ANIMATION_BUFFER || n >= LED_N) {
    return;
  }
  
  bool found = false;
  unsigned long lastTime;
  byte lastRed;
  byte lastGreen;
  byte lastBlue;
  byte lastWhite;  
  for (int i = 0; i < activeAnimations; i++) {
    if (animations[i].n == n) {
      if (!found) {
        lastTime = animations[i].stop;
        found = true;
      } else if(lastTime < animations[i].stop) {
        lastTime = animations[i].stop;
        lastRed = animations[i].stopRed;
        lastGreen = animations[i].stopGreen;
        lastBlue = animations[i].stopBlue;
        lastWhite = animations[i].stopWhite;
      }
    }
  }

  if (!found) {
    uint32_t color = pixels.getPixelColor(n);
    lastRed = (color >> 16) & 0xFF;
    lastGreen = (color >> 8) & 0xFF;
    lastBlue = color & 0xFF;
    lastWhite = (color >> 24) & 0xFF;
  }

  addAnimationFrom(n, lastRed, lastGreen, lastBlue, lastWhite, stopRed, stopGreen, stopBlue, stopWhite, duration, identifier, offset);
}

void addAnimationFrom(int n, byte startRed, byte startGreen, byte startBlue, byte startWhite, byte stopRed, byte stopGreen, byte stopBlue, byte stopWhite, int duration, int identifier, int offset) {
  if (activeAnimations-1 >= ANIMATION_BUFFER || n >= LED_N) {
    return;
  }

  unsigned long time = millis();

  animations[activeAnimations] = {
    n,
    startRed,
    startGreen,
    startBlue,
    startWhite,
    stopRed,
    stopGreen,
    stopBlue,
    stopWhite,
    time + offset, // start
    time + offset + duration,  // stop
    identifier,
  };
  activeAnimations++;
}

void animationEndCallback(int identifier, int led) {
  if (identifier == IDENTIFIER_MINUTE_ON) {
    addAnimation(led, 0, 0, 0, 0, 2000, IDENTIFIER_MINUTE_OFF);
  } else if (identifier == IDENTIFIER_MINUTE_OFF) {
    addAnimation((int) (timeClient.getMinutes()*LED_N/60+LED_OFFSET)%LED_N, 255, 0, 0, 0, 2000, IDENTIFIER_MINUTE_ON);
  } else if (identifier == IDENTIFIER_HOUR_ON) {
    if ((int) ((timeClient.getHours()%12)*LED_N/12+LED_OFFSET)%LED_N == led) {
      addAnimation(led, 0, 255, 0, 0, 2000, IDENTIFIER_HOUR_ON);
    } else {
      addAnimation(led, 0, 0, 0, 0, 2000, IDENTIFIER_HOUR_OFF);
    }
  } else if (identifier == IDENTIFIER_HOUR_OFF) {
    addAnimation((int) ((timeClient.getHours()%12)*LED_N/12+LED_OFFSET)%LED_N, 0, 255, 0, 0, 2000, IDENTIFIER_HOUR_ON);
  }
}

void updateLights() {
  unsigned long time = millis();
  for (int i = 0; i < activeAnimations; i++) {
    if (animations[i].start < time) {
      float pos = max(min((float)(time-animations[i].start)/(animations[i].stop - animations[i].start), (float)1), (float)0);
      pixels.setPixelColor(animations[i].n, 
        animations[i].startRed*(1-pos)+animations[i].stopRed*pos,
        animations[i].startGreen*(1-pos)+animations[i].stopGreen*pos,
        animations[i].startBlue*(1-pos)+animations[i].stopBlue*pos,
        animations[i].startWhite*(1-pos)+animations[i].stopWhite*pos
      );
    }
  }
  for (int i = 0; i < activeAnimations; i++) {
    if (animations[i].stop < time) {
        int identifier = animations[i].identifier;
        int led = animations[i].n;
        activeAnimations--;
        if (activeAnimations < ANIMATION_BUFFER - 2)
          animations[i] = animations[activeAnimations];
        animationEndCallback(identifier, led);
    }
  }
  pixels.show();
}


