#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <qrcode.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define I2C_SDA 21
#define I2C_SCL 22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* WIFI_SSID = "DAYANE";
const char* WIFI_PASSWORD = "@Felipe23";
const char* SUPABASE_URL = "https://usztjfxtbagjbnupiuxm.supabase.co";
const char* SUPABASE_ANON_KEY = "sb_publishable_SZhZ1FlWxXzd85fWRQ69Fg_j9HXqqnr";
const char* EVENT_ID = "a142ed17-e276-47e0-918f-8d1145b558c0";
const char* MACHINE_NAME = "TampAê - ESP32";
const float MACHINE_LATITUDE = 0.0;
const float MACHINE_LONGITUDE = 0.0;

String machineId = "";
String deviceToken = "";
String lastSessionId = "";
unsigned long lastPoll = 0;
const unsigned long POLL_INTERVAL = 1500;

void oledMessage(const String& a, const String& b = "", const String& c = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(a);
  if (b.length()) display.println(b);
  if (c.length()) display.println(c);
  display.display();
}

// Desenha o QR usando a maior escala inteira que couber nos 128x64.
// Para o payload atual, normalmente teremos 37x37 modulos (QR v5),
// permitindo escala 1 e aproveitamento de praticamente toda a altura.
void drawQR(esp_qrcode_handle_t qrHandle) {
  int modules = esp_qrcode_get_size(qrHandle);
  const int quietZone = 2;

  // Escolhe automaticamente a maior escala que cabe na tela.
  int scaleX = SCREEN_WIDTH / (modules + quietZone * 2);
  int scaleY = SCREEN_HEIGHT / (modules + quietZone * 2);
  int scale = min(scaleX, scaleY);
  if (scale < 1) scale = 1;

  int total = modules * scale + quietZone * 2 * scale;
  int x0 = (SCREEN_WIDTH - total) / 2;
  int y0 = (SCREEN_HEIGHT - total) / 2;

  display.clearDisplay();
  display.fillRect(x0, y0, total, total, SSD1306_WHITE);

  for (int y = 0; y < modules; y++) {
    for (int x = 0; x < modules; x++) {
      if (esp_qrcode_get_module(qrHandle, x, y)) {
        display.fillRect(
          x0 + (quietZone + x) * scale,
          y0 + (quietZone + y) * scale,
          scale,
          scale,
          SSD1306_BLACK
        );
      }
    }
  }

  display.display();

  Serial.print("QR: ");
  Serial.print(modules);
  Serial.print("x");
  Serial.print(modules);
  Serial.print(" | escala: ");
  Serial.println(scale);
}

void showQR(const String& payload) {
  esp_qrcode_config_t config = {};
  config.display_func = drawQR;
  config.max_qrcode_version = 10;
  config.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;

  esp_err_t result = esp_qrcode_generate(&config, payload.c_str());

  Serial.println();
  Serial.println("========================================");
  Serial.println("QR EXIBIDO NO OLED");
  Serial.print("Payload: ");
  Serial.println(payload);
  if (result != ESP_OK) {
    Serial.print("ERRO AO GERAR QR: ");
    Serial.println((int)result);
  } else {
    Serial.println("Escaneie este QR pelo aplicativo.");
  }
  Serial.println("========================================");
}

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  oledMessage("TAMPAE", "Conectando Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi OK. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.print("Wi-Fi falhou. Status: ");
  Serial.println(WiFi.status());
  oledMessage("TAMPAE", "Falha Wi-Fi");
  return false;
}

String restUrl(const String& table) {
  String base = SUPABASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/rest/v1/" + table;
}

String rpcUrl(const String& rpc) {
  String base = SUPABASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/rest/v1/rpc/" + rpc;
}

void headers(HTTPClient& http) {
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
}

bool createMachine() {
  HTTPClient http;
  if (!http.begin(restUrl("machines"))) return false;

  headers(http);
  http.addHeader("Prefer", "return=representation");

  JsonDocument req;
  req["nome"] = MACHINE_NAME;
  req["latitude"] = MACHINE_LATITUDE;
  req["longitude"] = MACHINE_LONGITUDE;
  req["status"] = "ativa";

  String body;
  serializeJson(req, body);

  int code = http.POST(body);
  String response = http.getString();
  http.end();

  Serial.print("machines HTTP: ");
  Serial.println(code);
  Serial.print("machines resposta: ");
  Serial.println(response);

  if (code < 200 || code >= 300) {
    oledMessage("TAMPAE", "Erro machines", "HTTP " + String(code));
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) return false;

  JsonObject row;
  if (doc.is<JsonArray>()) {
    JsonArray a = doc.as<JsonArray>();
    if (!a.size()) return false;
    row = a[0].as<JsonObject>();
  } else {
    row = doc.as<JsonObject>();
  }

  machineId = row["id"] | "";
  deviceToken = row["device_token"] | "";

  if (!machineId.length() || !deviceToken.length()) return false;

  Serial.println();
  Serial.println("========================================");
  Serial.println("MAQUINA CRIADA");
  Serial.print("MACHINE_ID: ");
  Serial.println(machineId);
  Serial.print("DEVICE_TOKEN: ");
  Serial.println(deviceToken);
  Serial.print("EVENT_ID: ");
  Serial.println(EVENT_ID);
  Serial.println("========================================");

  return true;
}

void checkSession() {
  if (WiFi.status() != WL_CONNECTED || !machineId.length() || !deviceToken.length()) return;

  HTTPClient http;
  if (!http.begin(rpcUrl("get_active_session"))) return;

  headers(http);

  JsonDocument req;
  req["p_machine_id"] = machineId;
  req["p_device_token"] = deviceToken;

  String body;
  serializeJson(req, body);

  int code = http.POST(body);
  String response = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    Serial.print("RPC HTTP: ");
    Serial.println(code);
    Serial.println(response);
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) return;

  JsonVariant row;
  if (doc.is<JsonArray>()) {
    JsonArray a = doc.as<JsonArray>();
    if (!a.size()) return;
    row = a[0];
  } else if (doc.is<JsonObject>()) {
    row = doc.as<JsonObject>();
  } else {
    return;
  }

  String sessionId = row["session_id"] | "";
  if (!sessionId.length() || sessionId == lastSessionId) return;
  lastSessionId = sessionId;

  String userName = row["nome"] | "";
  String sessionEventId = row["evento_id"] | "";

  Serial.println();
  Serial.println("========================================");
  Serial.println("USUARIO CONECTADO");
  Serial.print("NOME: ");
  Serial.println(userName.length() ? userName : "Usuario");
  Serial.print("EVENTO_ID: ");
  Serial.println(sessionEventId.length() ? sessionEventId : "NULL");
  Serial.print("SESSION_ID: ");
  Serial.println(sessionId);
  Serial.println("========================================");
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERRO: OLED nao encontrado.");
    while (true) delay(1000);
  }

  oledMessage("TAMPAE", "Iniciando...");

  if (!connectWiFi()) return;

  oledMessage("TAMPAE", "Criando maquina...");

  if (!createMachine()) {
    oledMessage("TAMPAE", "Erro ao criar", "maquina");
    return;
  }

  String qrPayload = String("{\"machine_id\":\"") + machineId +
                     "\",\"event_id\":\"" + EVENT_ID + "\"}";

  showQR(qrPayload);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    delay(1000);
    return;
  }

  if (millis() - lastPoll >= POLL_INTERVAL) {
    lastPoll = millis();
    checkSession();
  }

  delay(10);
}
