#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

// Pinos
#define DHT_PIN 15
#define TRIG_PIN 5
#define ECHO_PIN 18
#define NEOPIXEL_PIN 4
#define BUZZER_PIN 19

// DHT
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// NeoPixel
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// WiFi / MQTT (FIWARE)
const char* WIFI_SSID = "Wokwi-GUEST"; // ajuste conforme necessário
const char* WIFI_PASS = "";

const char* MQTT_BROKER = "44.223.43.74";
const int MQTT_PORT = 1883;

const char* TOPIC_CMD   = "TEF/device014/cmd";
const char* TOPIC_ATTR_ST = "TEF/device014/attrs/s";
const char* TOPIC_ATTR_T = "TEF/device014/attrs/t";
const char* TOPIC_ATTR_H = "TEF/device014/attrs/h";
const char* TOPIC_ATTR_D = "TEF/device014/attrs/d";
const char* TOPIC_ATTR_PR = "TEF/device014/attrs/p";
const char* TOPIC_ATTR_F = "TEF/device014/attrs/f";

const char* CLIENT_ID = "fiware_014";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Estado / variáveis
bool sessionActive = false;
bool sessionPaused = false;
String sessionType = "none";
unsigned long sessionDurationSec = 0;
unsigned long sessionEndMillis = 0;

float focusLevel = 75.0;
unsigned long lastFocusDecayMillis = 0;
unsigned long lastPublishMillis = 0;

bool userPresent = true;
unsigned long lastSeenPresentMillis = 0;

const unsigned long PRESENCE_TIMEOUT_MS = 30000;
const unsigned long FOCUS_DECAY_INTERVAL_MS = 60000;
const unsigned long PUBLISH_INTERVAL_MS = 5000;

const float TEMP_MIN = 20.0;
const float TEMP_MAX = 25.0;
const float HUM_MIN  = 40.0;
const float HUM_MAX  = 60.0;

// --- utilitários ---
void beepActiveBuzzer(unsigned int ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

void neopixelSetColor(uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}

float medirDistanciaCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 999.0;
  float distance = duration * 0.034 / 2.0;
  return distance;
}

String formatTimeMMSS(unsigned long sec) {
  unsigned int m = sec / 60;
  unsigned int s = sec % 60;
  char buf[8];
  sprintf(buf, "%02u:%02u", m, s);
  return String(buf);
}

// --- publicadores UL 2.0 (cada tópico leva payload 'key|value') ---
void publicarAttr(const char* topic, const String &payload) {
  mqttClient.publish(topic, payload.c_str());
}

void publicarTemperatura(float t) {
  String p = String("t|") + String(t, 1);
  publicarAttr(TOPIC_ATTR_T, p);
}
void publicarUmidade(float h) {
  String p = String("h|") + String(h, 1);
  publicarAttr(TOPIC_ATTR_H, p);
}
void publicarDistancia(float d) {
  String p = String("d|") + String(d, 1);
  publicarAttr(TOPIC_ATTR_D, p);
}
void publicarPresenca(bool pr) {
  String p = String("pr|") + String(pr ? "true" : "false");
  publicarAttr(TOPIC_ATTR_PR, p);
}
void publicarFoco(int f) {
  String p = String("f|") + String(f);
  publicarAttr(TOPIC_ATTR_F, p);
}
void publicarStatus(String st) {
  String p = String("st|") + st;
  publicarAttr(TOPIC_ATTR_ST, p);
}

void publicarCmdExe(const String &cmd) {
  String payload = cmd + "|OK";
  mqttClient.publish("TEF/device014/cmdexe", payload.c_str());
}

// --- tratar comandos simples vindos do IoT-Agent (pub/sub) ---
void handleCommandString(const String &cmd) {
  String s = cmd;
  s.trim();
  s.toLowerCase();

  if (s == "start") {
    sessionType = "focus";
    sessionDurationSec = 25 * 60UL;
    sessionEndMillis = millis() + sessionDurationSec * 1000UL;
    sessionActive = true;
    sessionPaused = false;

    focusLevel = 90.0;
    lastFocusDecayMillis = millis();
    neopixelSetColor(255, 0, 0);

    publicarStatus("active");
    publicarFoco(focusLevel);

    publicarCmdExe("start");
    beepActiveBuzzer(120);
    return;
  }

  if (s == "stop") {
    sessionActive = false;
    sessionPaused = false;
    sessionType = "none";
    sessionEndMillis = 0;

    neopixelSetColor(0, 255, 0);
    publicarStatus("idle");

    publicarCmdExe("stop");
    beepActiveBuzzer(120);
    return;
  }
}

// MQTT callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  Serial.print("[MQTT] recebeu em ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(msg);

  // comandos chegam no TOPIC_CMD
  if (String(topic) == TOPIC_CMD) {
    handleCommandString(msg);
  }
}

void mqttReconnect() {
  while (!mqttClient.connected()) {
    Serial.print("[MQTT] Conectando ao broker...");
    if (mqttClient.connect(CLIENT_ID)) {
      Serial.println("ok");
      mqttClient.subscribe(TOPIC_CMD);
      publicarStatus("idle");
    } else {
      Serial.print("falhou, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" tentando novamente em 2s");
      delay(2000);
    }
  }
}

void reconectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando ao WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi conectado, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi não conectado (emulador pode exigir net config).");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();
  pixels.begin();
  neopixelSetColor(0,0,0);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("Falha ao iniciar OLED");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Pragma Focus Pod");
    display.display();
  }

  Serial.println("Inicializando WiFi...");
  reconectWiFi();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  lastFocusDecayMillis = millis();
  lastPublishMillis = millis();
  lastSeenPresentMillis = millis();

  neopixelSetColor(0,255,0);
  publicarStatus("idle");
  delay(300);
}

void loop() {
  // manter wifi + mqtt
  reconectWiFi();
  if (!mqttClient.connected()) mqttReconnect();
  mqttClient.loop();

  unsigned long now = millis();

  // sensores
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  float dist = medirDistanciaCM();
  bool presentNow = (dist < 40.0);

  if (presentNow) {
    userPresent = true;
    lastSeenPresentMillis = now;
  } else {
    if (now - lastSeenPresentMillis > PRESENCE_TIMEOUT_MS) userPresent = false;
  }

  // publicar periodicamente cada atributo separadamente (UL 2.0)
  if (now - lastPublishMillis >= PUBLISH_INTERVAL_MS) {
    if (!isnan(temp)) publicarTemperatura(temp);
    if (!isnan(hum)) publicarUmidade(hum);
    publicarDistancia(dist);
    publicarPresenca(userPresent);
    publicarFoco((int)focusLevel);
    // status vai sendo publicado quando muda; mas publicar sempre também é ok
    if (sessionActive) publicarStatus("active");
    else if (sessionPaused) publicarStatus("paused");
    else publicarStatus("idle");

    lastPublishMillis = now;
  }

  // lógica de sessão / foco (igual ao que você já tem)
  if (sessionActive) {
    if (!userPresent) {
      sessionActive = false;
      sessionPaused = true;
      focusLevel = 10.0;
      neopixelSetColor(0,0,255);
      publicarStatus("paused");
      publicarFoco((int)focusLevel);
      beepActiveBuzzer(200);
    } else {
      if (sessionEndMillis > 0 && millis() >= sessionEndMillis) {
        sessionActive = false;
        sessionType = "none";
        sessionDurationSec = 0;
        sessionEndMillis = 0;
        neopixelSetColor(0,255,0);
        publicarStatus("idle");
        publicarFoco((int)focusLevel);
        for (int i=0;i<2;i++){ beepActiveBuzzer(150); delay(120); }
      } else {
        if (now - lastFocusDecayMillis >= FOCUS_DECAY_INTERVAL_MS) {
          lastFocusDecayMillis = now;
          bool ambienteRuim = false;
          if (!isnan(temp) && !isnan(hum)) {
            if (temp < TEMP_MIN || temp > TEMP_MAX || hum < HUM_MIN || hum > HUM_MAX) ambienteRuim = true;
          }
          float decay = ambienteRuim ? 4.0 : 2.0;
          focusLevel -= decay;
          if (focusLevel < 0) focusLevel = 0;
          publicarFoco((int)focusLevel);
          if (ambienteRuim) {
            neopixelSetColor(0,0,255); delay(200);
            neopixelSetColor(255,0,0); delay(200);
          }
        }
      }
    }
  } else {
    if (!sessionActive && sessionType != "none" && userPresent && sessionEndMillis > millis()) {
      sessionActive = true;
      sessionPaused = false;
      lastFocusDecayMillis = now;
      neopixelSetColor(255,0,0);
      publicarStatus("active");
      publicarFoco((int)focusLevel);
      beepActiveBuzzer(120);
    } else {
      if (now - lastFocusDecayMillis >= FOCUS_DECAY_INTERVAL_MS) {
        lastFocusDecayMillis = now;
        if (userPresent && focusLevel < 90.0) {
          focusLevel += 1.0;
          if (focusLevel > 90.0) focusLevel = 90.0;
          publicarFoco((int)focusLevel);
        }
      }
    }
  }

  // atualizar OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Sessao: "); display.print(sessionType);
  display.setCursor(0,10);
  if (sessionActive && sessionEndMillis > millis()) {
    unsigned long remSec = (sessionEndMillis - millis())/1000;
    display.print("Timer: "); display.print(formatTimeMMSS(remSec));
  } else display.print("Timer: --:--");
  display.setCursor(0,22); display.print("Foco: "); display.print((int)focusLevel); display.print("%");
  display.setCursor(0,34);
  if (!isnan(temp)) { display.print("Temp: "); display.print(temp,1); display.print("C"); } else display.print("Temp: --");
  display.setCursor(0,46);
  if (!isnan(hum)) { display.print("Umid: "); display.print(hum,1); display.print("%"); } else display.print("Umid: --");
  display.setCursor(0,56); display.print("Pres: "); display.print(userPresent ? "SIM":"NAO");
  display.display();

  delay(100);
}
