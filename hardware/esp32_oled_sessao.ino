#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// TAMPAÊ - PRIMEIRO TESTE DA MÁQUINA
// ESP32 + OLED SSD1306 128x64 I2C
// SDA -> GPIO 21
// SCL -> GPIO 22
//
// Objetivo deste primeiro teste:
// 1. Conectar o ESP32 ao Wi-Fi.
// 2. Consultar o Supabase periodicamente.
// 3. Verificar se existe uma sessão ativa para esta máquina.
// 4. Quando um usuário conectar pelo aplicativo, mostrar o nome
//    dele no OLED.
//
// Não há sensores, balança, LDR ou botão neste firmware.
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define I2C_SDA 21
#define I2C_SCL 22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================
// CONFIGURAÇÃO
// ============================================================
const char* WIFI_SSID = "COLOQUE_SEU_WIFI";
const char* WIFI_PASSWORD = "COLOQUE_SUA_SENHA";

const char* SUPABASE_URL = "https://SEU-PROJETO.supabase.co";
const char* SUPABASE_ANON_KEY = "SUA_CHAVE_ANON_OU_PUBLISHABLE";

// ID da máquina cadastrada na tabela machines.
// Exemplo: "a142ed17-e276-47e0-918f-8d1145b558c0"
const char* MACHINE_ID = "COLOQUE_O_ID_DA_MAQUINA";

// device_token da mesma máquina.
// Ele é usado pela RPC get_active_session para autenticar a máquina.
const char* DEVICE_TOKEN = "COLOQUE_O_DEVICE_TOKEN";

const unsigned long SESSION_POLL_INTERVAL = 1500;
const unsigned long WIFI_RETRY_INTERVAL = 5000;

unsigned long lastSessionPoll = 0;
unsigned long lastWifiRetry = 0;

String lastSessionId = "";
String lastUserName = "";

// ============================================================
// OLED
// ============================================================
void oledClear() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
}

void oledMessage(const String& line1,
                const String& line2 = "",
                const String& line3 = "",
                const String& line4 = "") {
  oledClear();
  display.println(line1);
  if (line2.length()) display.println(line2);
  if (line3.length()) display.println(line3);
  if (line4.length()) display.println(line4);
  display.display();
}

void showConnectedUser(const String& name) {
  oledClear();

  display.setTextSize(1);
  display.println("TAMPAE");
  display.println();
  display.println("USUARIO CONECTADO");
  display.println();

  display.setTextSize(2);
  String shortName = name;
  if (shortName.length() > 10) {
    shortName = shortName.substring(0, 10);
  }
  display.println(shortName);

  display.setTextSize(1);
  display.println();
  display.println("Pode usar a maquina");
  display.display();
}

// ============================================================
// WIFI
// ============================================================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  oledMessage("TAMPAE", "Conectando Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    display.print(".");
    display.display();
  }

  if (WiFi.status() == WL_CONNECTED) {
    oledMessage("Wi-Fi conectado", WiFi.localIP().toString());
    delay(1000);
  } else {
    oledMessage("Falha no Wi-Fi", "Tentando novamente...");
  }
}

// ============================================================
// SUPABASE
// ============================================================
bool configurationIsValid() {
  if (String(WIFI_SSID) == "COLOQUE_SEU_WIFI") return false;
  if (String(SUPABASE_URL).indexOf("SEU-PROJETO") >= 0) return false;
  if (String(SUPABASE_ANON_KEY) == "SUA_CHAVE_ANON_OU_PUBLISHABLE") return false;
  if (String(MACHINE_ID) == "COLOQUE_O_ID_DA_MAQUINA") return false;
  if (String(DEVICE_TOKEN) == "COLOQUE_O_DEVICE_TOKEN") return false;
  return true;
}

String getRpcUrl() {
  String base = SUPABASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/rest/v1/rpc/get_active_session";
}

bool getActiveSession(String& userName, String& sessionId) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  String url = getRpcUrl();

  if (!http.begin(url)) {
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);

  JsonDocument request;
  request["p_machine_id"] = MACHINE_ID;
  request["p_device_token"] = DEVICE_TOKEN;

  String body;
  serializeJson(request, body);

  int httpCode = http.POST(body);

  if (httpCode < 200 || httpCode >= 300) {
    String errorBody = http.getString();
    http.end();

    Serial.print("Supabase HTTP ");
    Serial.print(httpCode);
    Serial.print(": ");
    Serial.println(errorBody);
    return false;
  }

  String response = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.print("JSON invalido: ");
    Serial.println(error.c_str());
    return false;
  }

  JsonVariant row;

  if (doc.is<JsonArray>()) {
    JsonArray array = doc.as<JsonArray>();
    if (array.isNull() || array.size() == 0) {
      userName = "";
      sessionId = "";
      return true;
    }
    row = array[0];
  } else if (doc.is<JsonObject>()) {
    row = doc.as<JsonObject>();
  } else {
    return false;
  }

  sessionId = row["session_id"] | "";
  userName = row["nome"] | "";

  // Alguns dados podem não retornar nome. Nesse caso, usamos o user_id
  // apenas como fallback visual.
  if (userName.length() == 0) {
    userName = row["user_id"] | "Usuario";
  }

  return true;
}

// ============================================================
// SESSÃO
// ============================================================
void checkSession() {
  if (!configurationIsValid()) {
    oledMessage("TAMPAE", "Configure o codigo", "Wi-Fi/Supabase", "antes do teste");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiRetry >= WIFI_RETRY_INTERVAL) {
      lastWifiRetry = millis();
      connectWiFi();
    }
    return;
  }

  String userName;
  String sessionId;

  if (!getActiveSession(userName, sessionId)) {
    oledMessage("TAMPAE", "Wi-Fi OK", "Erro ao consultar", "Supabase");
    return;
  }

  if (sessionId.length() == 0) {
    if (lastSessionId.length() > 0) {
      lastSessionId = "";
      lastUserName = "";
    }

    oledMessage("TAMPAE", "Maquina pronta", "Aguardando usuario...");
    return;
  }

  // Só atualiza a tela quando a sessão realmente muda.
  if (sessionId != lastSessionId || userName != lastUserName) {
    lastSessionId = sessionId;
    lastUserName = userName;

    Serial.print("USUARIO CONECTADO: ");
    Serial.print(userName);
    Serial.print(" | sessao: ");
    Serial.println(sessionId);

    showConnectedUser(userName);
  }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERRO: OLED SSD1306 nao encontrado.");
    while (true) {
      delay(1000);
    }
  }

  oledMessage("TAMPAE", "Iniciando maquina...");
  delay(1000);

  Serial.println();
  Serial.println("=================================");
  Serial.println("TAMPAE - TESTE ESP32 + OLED");
  Serial.println("SDA: GPIO 21");
  Serial.println("SCL: GPIO 22");
  Serial.println("=================================");

  if (!configurationIsValid()) {
    Serial.println("Configure WIFI, SUPABASE, MACHINE_ID e DEVICE_TOKEN.");
    oledMessage("TAMPAE", "CONFIGURACAO", "PENDENTE", "Veja o codigo");
    return;
  }

  connectWiFi();
  checkSession();
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  if (millis() - lastSessionPoll >= SESSION_POLL_INTERVAL) {
    lastSessionPoll = millis();
    checkSession();
  }

  delay(10);
}
