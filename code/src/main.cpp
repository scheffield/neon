#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include "FFat.h"
#include <ESPAsyncWebServer.h>
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include <Preferences.h>
#include <ESPmDNS.h>
#include <Adafruit_GFX.h>

const uint8_t LED_COUNT = 14;
const neoPixelType TYPE = NEO_RGBW + NEO_KHZ800;

const uint8_t BUTTON_UP = 10;
const uint8_t BUTTON_LEFT = 9;
const uint8_t BUTTON_RIGHT = 7;
const uint8_t BUTTON_DOWN = 12;
const uint8_t VBAT = 5;
const uint8_t NTC = 4;
const uint8_t STAT = 38;
const uint8_t LED1 = 42;
const uint8_t LED2 = 39;
const uint8_t LED3 = 37;
const uint8_t LED4 = 36;
const uint8_t LED5 = 35;
const uint8_t LED6 = 34;
const uint8_t LED7 = 33;
const uint8_t LED8 = 17;

const uint32_t THERMISTORNOMINAL = 10000;
// temp. for nominal resistance (almost always 25 C)
const uint8_t TEMPERATURENOMINAL = 25;
// The beta coefficient of the thermistor (usually 3000-4000)
const uint16_t BCOEFFICIENT = 3950; // most likely wrong
// the value of the 'other' resistor
const uint16_t SERIESRESISTOR = 10000;


const uint32_t colors[] = {0x00FF0000, 0x00FFFF00, 0x0000FF00, 0x0000FFFF, 0x000000FF, 0x00FF00FF, 0x00FFFFFF};
const uint8_t colorsCount = 7;

const uint8_t brightnesses[] = {0, /*1, 3, */7, 15, 31, 63, 127, 255};
const uint8_t brightnessesCount = 7;

// to deal with negative k. from: https://stackoverflow.com/a/23214219
int mod(int k, int n) {
  return ((k %= n) < 0) ? k+n : k;
}

float easeInQuad(const float x) {
  return x * x;
}

// -- color helper ------------------------------------------------------------
uint16_t convertRGB888toRGB565(uint8_t r8, uint8_t g8, uint8_t b8) {
  // from https://developer.apple.com/documentation/accelerate/1533044-vimageconvert_argb8888torgb565
  uint32_t r5 = (r8   * (31*2) + 255) / (255*2);
  uint32_t g6 = (g8 * 63 + 127) / 255;
  uint32_t b5 = (b8  * 31 + 127) / 255;
  // Serial.printf("convertRGB888toRGB565; r8: %d, r5: %d, g8: %d, g6: %d, b8: %d, b5: %d\n", r8, r5, g8, g6, b8, b5);
  return (r5 << 11) | (g6 <<  5) | b5;
}

uint16_t convertRGB888toRGB565(uint32_t in) {
  uint8_t r8 = (uint8_t)(in >> 16);
  uint8_t g8 = (uint8_t)(in >> 8);
  uint8_t b8 = (uint8_t)in;
  return convertRGB888toRGB565(r8, g8, b8);
}

uint32_t convertRGB565toARGB888(uint16_t in) {
  // bitmasks are needed as the values don't line up nicely with uint8_t
  // from https://stackoverflow.com/questions/2442576/how-does-one-convert-16-bit-rgb565-to-24-bit-rgb888
  uint8_t r5 = (uint8_t)((in & 0xf800) >> 11);
  uint8_t g6 = (uint8_t)((in & 0x07e0) >> 5);
  uint8_t b5 = (uint8_t)in & 0x001f;
  // from https://developer.apple.com/documentation/accelerate/1533159-vimageconvert_rgb565toargb8888
  uint32_t r8 = (r5 * 255 + 15) / 31;
  uint32_t g8 = (g6 * 255 + 31) / 63;
  uint32_t b8 = (b5 * 255 + 15) / 31;
  // Serial.printf("convertRGB565toARGB888; r8: %d, r5: %d, g8: %d, g6: %d, b8: %d, b5: %d\n", r8, r5, g8, g6, b8, b5);
  return (r8 << 16) | (g8 <<  8) | b8;
}

// -- NeoPixel matrix ---------------------------------------------------------

typedef enum {
  NEON_ANIMATION_NONE = 1,
  NEON_ANIMATION_CIRCLE_FROM_CENTER = 2,
  NEON_ANIMATION_CIRCLE_FROM_SWITCH = 3,
  NEON_ANIMATION_BREATH_CONFIGURED = 4,
  NEON_ANIMATION_BREATH_NO_WIFI = 5,
  NEON_ANIMATION_CIRCLE_CONFIG_WIFI = 6,
} Animation;

class NeonMatrix : public Adafruit_GFX {
  private: 
    Adafruit_NeoPixel* _strips[8];
    uint8_t _brightness = 0;
    uint8_t _r = 255;
    uint8_t _g = 255;
    uint8_t _b = 255;
    uint8_t _transaction = 0;
    Animation _animation = NEON_ANIMATION_NONE;
    Animation _nextAnimation = NEON_ANIMATION_NONE;
    unsigned long _animationStartedAt = 0;
    float _animationProgress = 0;
    unsigned long _lastFastIntervalMs = 0;

    float easeInOutSine(const float x) {
      // from https://easings.net/#easeInOutSine
      return -(cos(PI * x) - 1) / 2;
    }

    void nextAnimation() {
      if (_nextAnimation == NEON_ANIMATION_NONE) {
        return;
      }
      _animation = _nextAnimation;
      _nextAnimation = NEON_ANIMATION_NONE;
      _animationProgress = 0;
      _animationStartedAt = 0;
    }

    void stepAnimation() {
      if (_animation == NEON_ANIMATION_NONE) {
        return;
      }
      
      const auto now = millis();
      if (_animationStartedAt == 0) {
        _animationStartedAt = now;
      }
      
      if (_animation == NEON_ANIMATION_CIRCLE_FROM_CENTER) {
        const uint16_t currentRadius = _animationProgress * 12;
        _animationProgress = fmod((now - _animationStartedAt) / 1000.0, 1);
        const uint16_t nextRadius = _animationProgress * 12;
        if (currentRadius == nextRadius) {
          return;
        }
        startWrite();
        drawCircle(6, 6, currentRadius, 0);
        drawCircle(6, 6, nextRadius, convertRGB888toRGB565(255, 255, 0));
        endWrite();
      } else if (_animation == NEON_ANIMATION_CIRCLE_FROM_SWITCH) {
        const uint16_t currentRadius = _animationProgress * 18;
        _animationProgress = (now - _animationStartedAt) / 500.0;
        if (_animationProgress > 1) {
          nextAnimation();
          return;
        }
        const uint16_t nextRadius = _animationProgress * 18;
        if (currentRadius == nextRadius) {
          return;
        }
        startWrite();
        for (int8_t i = -2; i <= 2; i++) {
          drawCircle(8, 13, currentRadius + i, 0);
        }
        for (int8_t i = -2; i <= 2; i++) {
          const float brightness = pow((2 - abs(i)) / 2., 3);
          drawCircle(8, 13, nextRadius + i, convertRGB888toRGB565(255 * brightness, 255 * brightness, 0));
        }
        endWrite();
      } else if (_animation == NEON_ANIMATION_BREATH_CONFIGURED) {
        _animationProgress = fmod((now - _animationStartedAt) / 2000.0, 1);
        float brightness = easeInOutSine(1 - (abs(_animationProgress - 0.5) * 2));
        drawPixel(8, 13, convertRGB888toRGB565(0, 255 * brightness, 0));
      } else if (_animation == NEON_ANIMATION_BREATH_NO_WIFI) {
        _animationProgress = fmod((now - _animationStartedAt) / 2000.0, 1);
        float brightness = easeInOutSine(1 - (abs(_animationProgress - 0.5) * 2));
        drawPixel(8, 13, convertRGB888toRGB565(255 * brightness, 0, 0));
      } else if (_animation == NEON_ANIMATION_CIRCLE_CONFIG_WIFI) {
        const uint16_t currentRadius = _animationProgress * 12;
        _animationProgress = fmod((now - _animationStartedAt) / 1000.0, 1);
        const uint16_t nextRadius = _animationProgress * 12;
        if (currentRadius == nextRadius) {
          return;
        }
        startWrite();
        for (int8_t i = -2; i <= 2; i++) {
          drawCircle(6, 6, currentRadius + i, 0);
        }
        for (int8_t i = -2; i <= 2; i++) {
          const float brightness = pow((2 - abs(i)) / 2., 3);
          drawCircle(6, 6, nextRadius + i, convertRGB888toRGB565(0, 0, 255 * brightness));
        }
        endWrite();
      }
    }

  public:
    NeonMatrix() : Adafruit_GFX(8 * 2, 14), _strips{
      new Adafruit_NeoPixel(LED_COUNT, LED1, TYPE),
      new Adafruit_NeoPixel(LED_COUNT, LED2, TYPE),
      new Adafruit_NeoPixel(LED_COUNT, LED3, TYPE),
      new Adafruit_NeoPixel(LED_COUNT, LED4, TYPE),
      new Adafruit_NeoPixel(LED_COUNT, LED5, TYPE),
      new Adafruit_NeoPixel(LED_COUNT, LED6, TYPE),
      new Adafruit_NeoPixel(LED_COUNT, LED7, TYPE),
      new Adafruit_NeoPixel(LED_COUNT, LED8, TYPE),
    } {}

    void begin() {
      for (int i = 0; i < 8; i++) {
        Adafruit_NeoPixel* strip = _strips[i];
        strip->begin();
        strip->show(); // Initialize all pixels to 'off'
      }
    }

    void drawPixel(int16_t x, int16_t y, const uint16_t color) {
      // Serial.printf("x: %d, y: %d, color: %d\n", x, y, color);
      if (x % 2 == 1 || x < 0 || x >= width() || y < 0 || y >= height()) {
        return;
      }
      x = x / 2;
      Adafruit_NeoPixel* strip = _strips[x];
      // TODO: GFX only works with RGB565, do we lose anything?
      strip->setPixelColor(y, convertRGB565toARGB888(color));
      if (!_transaction) {
        strip->show();
      }
    }

    void startWrite() {
      _transaction++;
    }

    void endWrite() {
      _transaction--;
      if (_transaction) {
        return;
      }
      for (int i = 0; i < 8; i++) {
        Adafruit_NeoPixel* strip = _strips[i];
        strip->show();
      }
    }

    uint8_t getBrightness() {
      return _brightness;
    }

    void setBrightness(const uint8_t brightness) {
      if (_brightness == brightness) {
        return;
      }
      _brightness = brightness;
      setAnimation(NEON_ANIMATION_NONE);
      fillScreen(convertRGB888toRGB565(
        (_r * _brightness) >> 8,
        (_g * _brightness) >> 8,
        (_b * _brightness) >> 8
      ));
    }

    uint8_t getR() {
      return _r;
    }

    uint8_t getG() {
      return _g;
    }

    uint8_t getB() {
      return _b;
    }

    void setColor(const uint32_t c) {
      uint8_t r = (uint8_t)(c >> 16);
      uint8_t g = (uint8_t)(c >> 8);
      uint8_t b = (uint8_t)c;
      setColor(r, g, b);
    }

    void setColor(const uint8_t r, const uint8_t g, const uint8_t b) {
      if (_r == r && _g == g && _b == b) {
        return;
      }
      _r = r;
      _g = g;
      _b = b;
      setAnimation(NEON_ANIMATION_NONE);
      fillScreen(convertRGB888toRGB565(
        (_r * _brightness) >> 8,
        (_g * _brightness) >> 8,
        (_b * _brightness) >> 8
      ));
    }

    void loop() {
      const unsigned long current = millis();

      if (current - _lastFastIntervalMs >= 50) {
        _lastFastIntervalMs = current;
        stepAnimation();
      }
    }

    void setAnimation(Animation animation) {
      _animation = animation;
      if (_animation != NEON_ANIMATION_NONE) {
        setBrightness(0);
      }
    }

    void setNextAnimation(Animation animation) {
      if (_animation == NEON_ANIMATION_NONE) {
        _animation = animation;
      } else {
        _nextAnimation = animation;
      }
    }
};

NeonMatrix matrix = NeonMatrix();

typedef enum {
  NEON_UNCONNECTED = 0,
  NEON_CONNECTING = 1,
  NEON_CONNECTED = 2,
  NEON_PAIRING = 3,
  NEON_PAIRING_CONNECTING = 4,
  NEON_PAIRING_SUCCESS = 5,
} WifiState;

struct State {
  float battery = 0;
  bool charging = false;
  float cpuTemp = 0;
  float batteryTemp = 0;
  long upPressedAt = 0;
  long downPressedAt = 0;
  long leftPressedAt = 0;
  long rightPressedAt = 0;

  WifiState wifiState = NEON_UNCONNECTED;
  String tmpSsid = String();
  String tmpPassword = String();
  long wifiConnectingSince = 0;

  int8_t currentColor = 0;
  int8_t currentBrightness = 0;
} _state;

AsyncWebServer server(80);

Preferences preferences;

typedef enum {
  NEON_BUTTON_NONE = 1,
  NEON_BUTTON_SHORT = 2,
  NEON_BUTTON_LONG = 3,
} ButtonPress;

ButtonPress handleButton(const uint8_t pin, long& pressedAt) {
  bool signal = !digitalRead(pin); // pulled up, normally high
  const long current = millis();
  if (signal) {
    if (pressedAt == 0) {
      pressedAt = current;
    }
  } else {
    ButtonPress res;
    if (pressedAt != 0) {
      if (current - pressedAt >= 1000) {
        res = NEON_BUTTON_LONG;
      } else {
        res = NEON_BUTTON_SHORT;
      }
      pressedAt = 0;
      return res;
    }
  }
  return NEON_BUTTON_NONE;
}

// Allow when in pairing mode
bool filterOnPairing(AsyncWebServerRequest *request) { 
  return _state.wifiState == NEON_PAIRING
      || _state.wifiState == NEON_PAIRING_CONNECTING
      || _state.wifiState == NEON_PAIRING_SUCCESS;
  }

// -- Wifi State Machine / helpers --------------------------------------------
struct WifiConfig {
  const String ssid;
  const String password;
};

typedef enum {
  NEON_FAILED = 1,
  NEON_WAITING = 2,
  NEON_SUCCESS = 3,
} WifiCheckResult;

// -- Wifi State Machine / side effects ---------------------------------------
WifiConfig readWifiPreferences() {
  return WifiConfig{
    preferences.getString("ssid"),
    preferences.getString("password"),
  };
}

void writeWifiPreferences(const WifiConfig config) {
  preferences.putString("ssid", config.ssid);
  preferences.putString("password", config.password);
}

WifiConfig readTmpWifiPreferences() {
  return WifiConfig{
    _state.tmpSsid,
    _state.tmpPassword,
  };
}

void writeTmpWifiPreferences(const WifiConfig config) {
  _state.tmpSsid = config.ssid;
  _state.tmpPassword = config.password;
}

void connectToWifi(const WifiConfig config) {
  Serial.printf("Connecting to ssid: %s\n", config.ssid);
  WiFi.setHostname("Neon");
  WiFi.begin(config.ssid.c_str(), config.password.c_str());
  _state.wifiConnectingSince = millis();
}

void startMdns() {
  if(!MDNS.begin("neon")) {
    Serial.println("Error starting mDNS");
  }
}

WifiCheckResult checkConnectedWithTimeout() {
  if (WiFi.status() == WL_CONNECTED) {
    return NEON_SUCCESS;
  }
  if (millis() - _state.wifiConnectingSince >= 10000) {
    Serial.println("Failed to connect to Wifi");
    return NEON_FAILED;
  }
  return NEON_WAITING;
}

void startServer() {
  server.begin();
}

void startAp() {
  WiFi.softAP("Neon Setup");
}

void stopAp() {
  WiFi.softAPdisconnect();
}

// -- Wifi State Machine / actions --------------------------------------------
void connectWifi() {
  if (_state.wifiState != NEON_UNCONNECTED) {
    return;
  }

  const auto config = readWifiPreferences();
  if (config.ssid.isEmpty()) {
    Serial.println("No wifi ssid configured");
    return;
  }
  
  connectToWifi(config);

  _state.wifiState = NEON_CONNECTING;
}

void checkWifi() {
  if (_state.wifiState != NEON_CONNECTING) {
    return;
  }

  const auto res = checkConnectedWithTimeout();
  if (res == NEON_SUCCESS) {
    startServer();
    startMdns();
    matrix.setAnimation(NEON_ANIMATION_BREATH_CONFIGURED);
    _state.wifiState = NEON_CONNECTED;
  } else if (res == NEON_FAILED) {
    _state.wifiState = NEON_UNCONNECTED;
  }
}

// void disconnectWifi() {}

void startPairing() {
  if (_state.wifiState != NEON_UNCONNECTED && _state.wifiState != NEON_CONNECTING && _state.wifiState != NEON_CONNECTED) {
    return;
  }

  startAp();
  startServer();
  matrix.setAnimation(NEON_ANIMATION_CIRCLE_CONFIG_WIFI);

  _state.wifiState = NEON_PAIRING;
}

void updateWifi(const String ssid, const String password) {
  if (_state.wifiState != NEON_PAIRING) {
    return;
  }

  const auto config = WifiConfig{ssid, password};
  writeTmpWifiPreferences(config);
  connectToWifi(config);
  Serial.println("updateWifi; connecting done");

  _state.wifiState = NEON_PAIRING_CONNECTING;
}

void checkPairing() {
  if (_state.wifiState != NEON_PAIRING_CONNECTING) {
    return;
  }

  const auto res = checkConnectedWithTimeout();
  if (res == NEON_SUCCESS) {
    writeWifiPreferences(readTmpWifiPreferences());
    startMdns();
    matrix.setAnimation(NEON_ANIMATION_BREATH_CONFIGURED);
    _state.wifiState = NEON_PAIRING_SUCCESS;
  } else if (res == NEON_FAILED) {
    _state.wifiState = NEON_PAIRING;
  }
}

void pairingSuccessMessaged() {
  if (_state.wifiState != NEON_PAIRING_SUCCESS) {
    return;
  }

  stopAp();

  _state.wifiState = NEON_CONNECTED;
}

String wifiStateToString() {
       if (_state.wifiState == NEON_UNCONNECTED) {return "NEON_UNCONNECTED";}
  else if (_state.wifiState == NEON_CONNECTING) {return "NEON_CONNECTING";}
  else if (_state.wifiState == NEON_CONNECTED) {return "NEON_CONNECTED";}
  else if (_state.wifiState == NEON_PAIRING) {return "NEON_PAIRING";}
  else if (_state.wifiState == NEON_PAIRING_CONNECTING) {return "NEON_PAIRING_CONNECTING";}
  else if (_state.wifiState == NEON_PAIRING_SUCCESS) {return "NEON_PAIRING_SUCCESS";}
  return "unknown state";
}

void writeStateToResponse(AsyncResponseStream *response) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root["brightness"] = matrix.getBrightness();
  root["r"] = matrix.getR();
  root["g"] = matrix.getG();
  root["b"] = matrix.getB();
  root["battery"] = _state.battery;
  root["charging"] = _state.charging;
  root["cpuTemp"] = _state.cpuTemp;
  root["batteryTemp"] = _state.batteryTemp;
  root.printTo(*response);
}

template <typename TString, typename TValue>
bool maybeUpdateStateFromJson(JsonObject& jsonObj, TString* key, TValue& current) {
  if (jsonObj.containsKey(key)) {
    uint8_t value = jsonObj[key];
    if (current != value) {
      current = value;
      return true;
    }
  }
  return false;
}

bool maybeUpdateStateFromJson(JsonObject& jsonObj, String key, uint8_t (*get)(), void (*set)(uint8_t)) {
  if (jsonObj.containsKey(key)) {
    uint8_t value = jsonObj[key];
    if (get() != value) {
      set(value);
      return true;
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  // while(!Serial) {
  //   delay(10);
  // }

  pinMode(BUTTON_UP, INPUT);
  pinMode(BUTTON_LEFT, INPUT);
  pinMode(BUTTON_RIGHT, INPUT);
  pinMode(BUTTON_DOWN, INPUT);
  pinMode(VBAT, INPUT);
  pinMode(NTC, INPUT);
  pinMode(STAT, INPUT);

  matrix.begin();
  matrix.setAnimation(NEON_ANIMATION_CIRCLE_FROM_SWITCH);
  matrix.setNextAnimation(NEON_ANIMATION_BREATH_NO_WIFI);

  if(!FFat.begin(true)){
    Serial.println("FFat Mount Failed");
    return;
  }

  if (!preferences.begin("neon", false)) {
    Serial.println("Failed to open preferences");
    return;
  }

  connectWifi();

  // -- STA --

  server.on("/_api/light", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    writeStateToResponse(response);
    request->send(response);
  });

  AsyncCallbackJsonWebHandler* apiHandler = new AsyncCallbackJsonWebHandler("/_api/light", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject& jsonObj = json.as<JsonObject>();

    if (jsonObj.containsKey("brightness")) {
      matrix.setBrightness(jsonObj["brightness"]);
    }
    if (jsonObj.containsKey("r") && jsonObj.containsKey("g") && jsonObj.containsKey("b")) {
      matrix.setColor(jsonObj["r"], jsonObj["g"], jsonObj["b"]);
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    writeStateToResponse(response);
    request->send(response);
  });
  server.addHandler(apiHandler);

  // it doesn't seem to work to ask for server.serveStatic("/", FFat, "/index.html")
  // similarly, server.serveStatic("/", FFat, "/"); server.rewrite("/", "index.html") doesn't work either
  server.serveStatic("/", FFat, "/").setDefaultFile("index.html");
  // routing is done in the client, all we need is to provide the index.html for all possible routs
  server.serveStatic("/setup", FFat, "/index.html");

  // -- AP --

  server.on("/_api/setup", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    int n = WiFi.scanComplete();
    if(n == -2){
      WiFi.scanNetworks(true);
    } else if(n) {
      JsonArray& networks = root.createNestedArray("networks");
      for (int i = 0; i < n; ++i) {
        JsonObject& network = networks.createNestedObject();
        network["rssi"] = WiFi.RSSI(i);
        network["ssid"] = WiFi.SSID(i);
        // network["bssid"] = WiFi.BSSIDstr(i);
        // network["channel"] = WiFi.channel(i);
        network["secure"] = WiFi.encryptionType(i);
        // network["connected"] = WiFi.SSID(i) == String(_state.ssid);
      }
      WiFi.scanDelete();
      if(WiFi.scanComplete() == -2){
        WiFi.scanNetworks(true);
      }
    }
    root["wifiStatus"] = wifiStateToString();
    // This only actually advaces the state machine if we had a pairing success
    pairingSuccessMessaged();
    root["staIp"] = WiFi.localIP().toString();
    root.printTo(*response);
    request->send(response);
  }).setFilter(filterOnPairing);

  AsyncCallbackJsonWebHandler* setupHandler = new AsyncCallbackJsonWebHandler("/_api/setup", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject& jsonObj = json.as<JsonObject>();
    String ssid = jsonObj["ssid"];
    if (ssid == NULL || ssid.isEmpty()) {
      request->send(400);
      return;
    }
    String password = jsonObj["password"];
    updateWifi(ssid, password);
    request->send(200);
  });
  server.addHandler(setupHandler).setFilter(filterOnPairing);
}

void updateSensorState() {
  // TODO: make dependent of uC ADC resolution
  uint16_t batteryVoltageRaw = analogRead(VBAT); // [0..8192)
  float batteryVoltage = 3.3 * batteryVoltageRaw / 8192 * (100 + 100) / 100 * .8;

  // from https://learn.adafruit.com/thermistor/using-a-thermistor
  uint16_t ntcResistanceRaw = analogRead(NTC); // [0..8192)
  float ntcResistance = 10000. / (8192. / ntcResistanceRaw - 1.);
  float steinhart;
  steinhart = ntcResistance / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert absolute temp to C

  _state.battery = batteryVoltage;
  _state.charging = !stat;
  _state.batteryTemp = steinhart;
  // _state.cpuTemp = (temprature_sens_read() - 32) / 1.8; // celsius
}

long _lastFastIntervalMs = 0;
long _lastSlowIntervalMs = 0;

void loop() {
  matrix.loop();
  unsigned long current = millis();

  if (current - _lastFastIntervalMs >= 100) {
    _lastFastIntervalMs = current;
    const auto up = handleButton(BUTTON_UP, _state.upPressedAt);
    if (up == NEON_BUTTON_LONG) {
      startPairing();
    } else if (up == NEON_BUTTON_SHORT) {
      _state.currentBrightness = constrain(_state.currentBrightness + 1, 0, brightnessesCount - 1);
      matrix.setBrightness(brightnesses[_state.currentBrightness]);
    }
    if (handleButton(BUTTON_DOWN, _state.downPressedAt) == NEON_BUTTON_SHORT) {
      _state.currentBrightness = constrain(_state.currentBrightness - 1, 0, brightnessesCount - 1);
      matrix.setBrightness(brightnesses[_state.currentBrightness]);
    }
    if (handleButton(BUTTON_LEFT, _state.leftPressedAt) == NEON_BUTTON_SHORT) {
      _state.currentColor = mod(_state.currentColor + 1, colorsCount);
      matrix.setColor(colors[_state.currentColor]);
      if (matrix.getBrightness() == 0) {
        matrix.setBrightness(31);
        _state.currentBrightness = 3;
      }
    }
    if (handleButton(BUTTON_RIGHT, _state.rightPressedAt) == NEON_BUTTON_SHORT) {
      _state.currentColor = mod(_state.currentColor - 1, colorsCount);
      matrix.setColor(colors[_state.currentColor]);
      if (matrix.getBrightness() == 0) {
        matrix.setBrightness(31);
        _state.currentBrightness = 3;
      }
    }
  }
  if (current - _lastSlowIntervalMs >= 2000) {
    _lastSlowIntervalMs = current;
    checkWifi();
    checkPairing();
    updateSensorState();
    Serial.printf("vBat: %1.2f, ntc: %f, stat: %d, ap ip: %s, sta ip: %s, rssi: %d, status: %s\n", _state.battery, _state.batteryTemp, _state.charging, WiFi.softAPIP().toString(), WiFi.localIP().toString(), WiFi.RSSI(), wifiStateToString().c_str());
  }
}