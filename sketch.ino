#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

// =========================
// ===== CONFIGURAÇÕES =====
// =========================

// --- Pinos ---
#define DHT_PIN 15 // Data do DHT22
#define TRIG_PIN 5
#define ECHO_PIN 18
#define NEOPIXEL_PIN 4
#define BUZZER_PIN 19

// --- DHT ----
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// --- OLED 128x64 I2C ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- NeoPixel ---
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// =========================
// ===== MQTT / WIFI =======
// =========================
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const char* MQTT_BROKER = "test.mosquitto.org";
const int MQTT_PORT = 1883;

const char* TOPIC_CMD = "pragma/focuspod/cmd/user1";
const char* TOPIC_STATUS = "pragma/focuspod/status/user1";
const char* TOPIC_ENV = "pragma/focuspod/env/user1";
const char* TOPIC_PRESENCE = "pragma/focuspod/presence/user1";
const char* TOPIC_FOCUS = "pragma/focuspod/focus_level/user1";
const char* CLIENT_ID = "pragma_focuspod_user1";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// =========================
// ===== VARIÁVEIS ========
// =========================
bool sessionActive = false;
bool sessionPaused = false;
String sessionType = "none"; // "focus" ou "break" ou "none"
unsigned long sessionDurationSec = 0; // segundos (duração total)
unsigned long sessionEndMillis = 0;   // millis() quando acaba

float focusLevel = 0.0; // 0-100
unsigned long lastFocusDecayMillis = 0;
unsigned long lastPublishMillis = 0;

bool userPresent = true;
unsigned long lastSeenPresentMillis = 0;

// Parâmetros configuráveis
const unsigned long PRESENCE_TIMEOUT_MS = 30000; // 30s para considerar ausência
const unsigned long FOCUS_DECAY_INTERVAL_MS = 60000; // 1 minuto entre decaimentos
const unsigned long PUBLISH_INTERVAL_MS = 5000; // publica env/presence cada 5s

// faixa de conforto
const float TEMP_MIN = 20.0;
const float TEMP_MAX = 25.0;
const float HUM_MIN  = 40.0;
const float HUM_MAX  = 60.0;

// =========================
// ===== FUNÇÕES AUX =======
// =========================

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
  // HC-SR04
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms
  if (duration == 0) return 999.0; // sem eco
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

void publicarMQTT(const char* topic, const String& payload) {
  mqttClient.publish(topic, payload.c_str());
}

// publica presença (JSON)
void publicarPresence(bool present, float distance) {
  String p = "{";
  p += "\"present\":";
  p += (present ? "true" : "false");
  p += ",\"distance_cm\":";
  p += String(distance, 1);
  p += "}";
  publicarMQTT(TOPIC_PRESENCE, p);
}

// publica ambiente (JSON)
void publicarAmbiente(float t, float h) {
  String p = "{";
  p += "\"temperature\":";
  p += String(t, 1);
  p += ",\"humidity\":";
  p += String(h, 1);
  p += "}";
  publicarMQTT(TOPIC_ENV, p);
}

// publica foco (JSON)
void publicarFoco(float level) {
  String p = "{";
  p += "\"level\":";
  p += String((int)level);
  p += "}";
  publicarMQTT(TOPIC_FOCUS, p);
}

// publica status sessão (JSON)
void publicarStatus(String state, String reason = "") {
  String p = "{";
  p += "\"session_state\":\"";
  p += state;
  p += "\"";

  if (state == "active") {
    unsigned long remainingSec = 0;
    if (sessionEndMillis > millis()) remainingSec = (sessionEndMillis - millis()) / 1000;
    p += ",\"session_type\":\"";
    p += sessionType;
    p += "\",\"time_remaining\":";
    p += String((int)remainingSec);
  }

  if (reason.length() > 0) {
    p += ",\"reason\":\"";
    p += reason;
    p += "\"";
  }

  p += "}";
  publicarMQTT(TOPIC_STATUS, p);
}

// interpreta payload MQTT para comandos simples em JSON-like:
// espera strings como {"action":"start_session","type":"focus","duration":25}
void handleMQTTCommand(const String &payload) {
  String s = payload;
  s.toLowerCase();

  // Start session
  if (s.indexOf("start_session") >= 0 || s.indexOf("\"action\":\"start_session\"") >= 0) {
    // extrair duration se houver (em minutos)
    unsigned long durationMin = 25; // padrão
    int idxDur = s.indexOf("duration");
    if (idxDur >= 0) {
      String tail = s.substring(idxDur);
      String num = "";
      for (unsigned int i = 0; i < tail.length(); i++) {
        if (isDigit(tail[i])) num += tail[i];
        else if (num.length() > 0) break;
      }
      if (num.length() > 0) durationMin = (unsigned long)atoi(num.c_str());
    }
    sessionType = (s.indexOf("\"type\":\"focus\"") >= 0) ? "focus" : "focus";
    sessionDurationSec = durationMin * 60UL;
    sessionEndMillis = millis() + sessionDurationSec * 1000UL;
    sessionActive = true;
    sessionPaused = false;
    focusLevel = 90.0;
    lastFocusDecayMillis = millis();
    neopixelSetColor(255, 0, 0); // vermelho
    beepActiveBuzzer(120);
    publicarStatus("active", "");
    publicarFoco(focusLevel);
    return;
  }

  // End session
  if (s.indexOf("end_session") >= 0 || s.indexOf("\"action\":\"end_session\"") >= 0) {
    sessionActive = false;
    sessionPaused = false;
    sessionType = "none";
    sessionDurationSec = 0;
    sessionEndMillis = 0;
    neopixelSetColor(0, 255, 0); // verde_idle
    beepActiveBuzzer(120);
    publicarStatus("idle", "");
    publicarFoco(focusLevel);
    return;
  }

  // Pause
  if (s.indexOf("pause") >= 0) {
    sessionPaused = true;
    sessionActive = false;
    publicarStatus("paused", "manual");
    neopixelSetColor(0, 0, 255);
    return;
  }

  // Resume
  if (s.indexOf("resume") >= 0) {
    if (sessionType != "none" && sessionEndMillis > millis()) {
      sessionPaused = false;
      sessionActive = true;
      lastFocusDecayMillis = millis();
      neopixelSetColor(255, 0, 0);
      publicarStatus("active", "");
      return;
    }
  }
}

// =========================
// ===== MQTT CALLBACK =====
// =========================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  Serial.print("[MQTT] recebeu em ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(msg);

  if (String(topic) == TOPIC_CMD) {
    handleMQTTCommand(msg);
  }
}

// =========================
// ==== CONEXÃO MQTT/WIFI ==
// =========================
void mqttReconnect() {
  while (!mqttClient.connected()) {
    Serial.print("[MQTT] Conectando ao broker...");
    if (mqttClient.connect(CLIENT_ID)) {
      Serial.println("ok");
      mqttClient.subscribe(TOPIC_CMD);
      publicarStatus("idle", "");
    } else {
      Serial.print("falhou, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" tentando novamente em 2s");
      delay(2000);
    }
  }
}

void reconectWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return;

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("Conectando ao WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void VerificaConexaoWiFi() {
  reconectWiFi();
}

// =========================
// ===== SETUP =========
// =========================
void setup() {
  Serial.begin(115200);
  delay(100);

  // pinos
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // DHT
  dht.begin();

  // NeoPixel
  pixels.begin();
  neopixelSetColor(0, 0, 0);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("Falha ao iniciar OLED");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Pragma Focus Pod");
    display.display();
  }

  // WiFi
  Serial.println("Inicializando WiFi...");
  reconectWiFi();

  // MQTT
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  // inicializa variáveis
  focusLevel = 75.0;
  lastFocusDecayMillis = millis();
  lastPublishMillis = millis();
  lastSeenPresentMillis = millis();

  neopixelSetColor(0, 255, 0); // idle verde
  publicarStatus("idle", "");
  delay(300);
}

// =========================
// ===== LOOP PRINCIPAL ====
// =========================
void loop() {

  VerificaConexaoWiFi();

  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  unsigned long now = millis();

  // ---- ler sensores ----
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  if (isnan(temp) || isnan(hum)) {
    // leitura inválida, mantém últimos valores (ou publica erro)
    Serial.println("[DHT] leitura inválida");
  }

  float dist = medirDistanciaCM();
  bool presentNow = (dist < 40.0); // limite de presença (cm). Ajuste se quiser.
  if (presentNow) {
    userPresent = true;
    lastSeenPresentMillis = now;
  } else {
    if (now - lastSeenPresentMillis > PRESENCE_TIMEOUT_MS) userPresent = false;
  }

  // publica presença/ambiente periodicamente
  if (now - lastPublishMillis >= PUBLISH_INTERVAL_MS) {
    publicarPresence(userPresent, dist);
    if (!isnan(temp) && !isnan(hum)) publicarAmbiente(temp, hum);
    lastPublishMillis = now;
  }

  // ---- lógica de sessão ----
  if (sessionActive) {
    // se usuário ausente, pausar automaticamente
    if (!userPresent) {
      sessionActive = false;
      sessionPaused = true;
      focusLevel = 10.0;
      neopixelSetColor(0, 0, 255); // azul
      publicarStatus("paused", "user_absent");
      publicarFoco(focusLevel);
      beepActiveBuzzer(200);
    } else {
      // verificar fim da sessão
      if (sessionEndMillis > 0 && millis() >= sessionEndMillis) {
        // sessão finalizada
        sessionActive = false;
        sessionType = "none";
        sessionDurationSec = 0;
        sessionEndMillis = 0;
        neopixelSetColor(0, 255, 0); // verde pausa
        publicarStatus("idle", "session_finished");
        publicarFoco(focusLevel);
        // buzzer final
        for (int i = 0; i < 2; i++) {
          beepActiveBuzzer(150);
          delay(120);
        }
      } else {
        // decaimento do foco (a cada minuto)
        if (now - lastFocusDecayMillis >= FOCUS_DECAY_INTERVAL_MS) {
          lastFocusDecayMillis = now;
          bool ambienteRuim = false;
          if (!isnan(temp) && !isnan(hum)) {
            if (temp < TEMP_MIN || temp > TEMP_MAX || hum < HUM_MIN || hum > HUM_MAX) ambienteRuim = true;
          }
          float decay = ambienteRuim ? 4.0 : 2.0; // pontos por minuto
          focusLevel -= decay;
          if (focusLevel < 0) focusLevel = 0;
          publicarFoco(focusLevel);
          // se ambiente ruim, piscar azul (indicador)
          if (ambienteRuim) {
            neopixelSetColor(0, 0, 255); delay(200);
            neopixelSetColor(255, 0, 0); delay(200);
          }
        }
      }
    }
  } else {
    // sessão inativa: se usuário voltou e havia sessão pausada por ausência, retomar automaticamente se houver tempo restante
    if (!sessionActive && sessionType != "none" && userPresent && sessionEndMillis > millis()) {
      // retomar
      sessionActive = true;
      sessionPaused = false;
      lastFocusDecayMillis = now;
      neopixelSetColor(255, 0, 0);
      publicarStatus("active", "resumed_by_presence");
      publicarFoco(focusLevel);
      beepActiveBuzzer(120);
    } else {
      // fora de sessão, pequena recuperação de foco ao longo do tempo (opcional)
      if (now - lastFocusDecayMillis >= FOCUS_DECAY_INTERVAL_MS) {
        lastFocusDecayMillis = now;
        if (userPresent && focusLevel < 90.0) {
          focusLevel += 1.0; // recuperação lenta
          if (focusLevel > 90.0) focusLevel = 90.0;
          publicarFoco(focusLevel);
        }
      }
    }
  }

  // ---- atualizar OLED (rápido) ----
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Sessao: ");
  display.print(sessionType);
  display.setCursor(0, 10);
  if (sessionActive && sessionEndMillis > millis()) {
    unsigned long remSec = (sessionEndMillis - millis()) / 1000;
    display.print("Timer: ");
    display.print(formatTimeMMSS(remSec));
  } else {
    display.print("Timer: --:--");
  }
  display.setCursor(0, 22);
  display.print("Foco: ");
  display.print((int)focusLevel);
  display.print("%");
  display.setCursor(0, 34);
  if (!isnan(temp)) {
    display.print("Temp: ");
    display.print(temp, 1);
    display.print("C ");
  } else {
    display.print("Temp: -- ");
  }
  display.setCursor(0, 46);
  if (!isnan(hum)) {
    display.print("Umid: ");
    display.print(hum, 1);
    display.print("%");
  } else {
    display.print("Umid: -- ");
  }
  display.setCursor(0, 56);
  display.print("Pres: ");
  display.print(userPresent ? "SIM" : "NAO");
  display.display();

  delay(100); // pequeno delay para estabilidade
}
