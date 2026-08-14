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
// Baseado diretamente no fluxo da máquina virtual de teste:
//   sb.rpc("get_active_session", {
//     p_machine_id: machine.id,
//     p_device_token: machine.device_token
//   })
//
// Neste estágio NÃO há sensores, balança, LDR ou botão.
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
const char* WIFI_SSID = "DAYANE";
const char* WIFI_PASSWORD = "@Felipe23";

const char* SUPABASE_URL = "https://usztjfxtbagjbnupiuxm.supabase.co";
const char* SUPABASE_ANON_KEY = "sb_publishable_SZhZ1FlWxXzd85fWRQ69Fg_j9HXqqnr";

// Mesmos valores informados para a máquina cadastrada.
const char* MACHINE_ID = "8076b255-53de-49f0-bb53-69024edcbd23";
const char* DEVICE_TOKEN = "a4e0a37b-9c1f-4f6b-9e1b-fddc4f4fccfa";

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
    oledMessage("Wi-Fi conectado", WiFi.localIP().toString());
    delay(800);
    return true;
  }

  Serial.print("Falha Wi-Fi. Status: ");
  Serial.println(WiFi.status());
  oledMessage("Falha no Wi-Fi", "Tentando novamente...");
  return false;
}

// ============================================================
// SUPABASE
// ============================================================
String getRpcUrl() {
  String base = SUPABASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/rest/v1/rpc/get_active_session";
}

// Mostra um erro HTTP no OLED e no Serial. Isso é importante para
// diferenciar falha de autenticação, RPC inexistente, RLS etc.
void showHttpError(int httpCode, const String& response) {
  Serial.print("get_active_session HTTP ");
  Serial.print(httpCode);
  Serial.print(": ");
  Serial.println(response);

  String code = String(httpCode);
  oledMessage("TAMPAE", "ERRO RPC", "HTTP " + code, "Veja Serial");
}

bool getActiveSession(String& userName, String& sessionId, String& errorInfo) {
  userName = "";
  sessionId = "";
  errorInfo = "";

  if (WiFi.status() != WL_CONNECTED) {
    errorInfo = "Wi-Fi desconectado";
    return false;
  }

  HTTPClient http;
  String url = getRpcUrl();

  Serial.println();
  Serial.println("--- get_active_session ---");
  Serial.print("URL: ");
  Serial.println(url);
  Serial.print("machine_id: ");
  Serial.println(MACHINE_ID);
  Serial.print("device_token: ");
  Serial.println(DEVICE_TOKEN);

  if (!http.begin(url)) {
    errorInfo = "http.begin falhou";
    Serial.println(errorInfo);
    return false;
  }

  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("Prefer", "return=representation");

  // EXATAMENTE os mesmos nomes de parâmetros usados pela máquina virtual.
  JsonDocument request;
  request["p_machine_id"] = MACHINE_ID;
  request["p_device_token"] = DEVICE_TOKEN;

  String body;
  serializeJson(request, body);

  Serial.print("POST body: ");
  Serial.println(body);

  int httpCode = http.POST(body);
  String response = http.getString();
  http.end();

  Serial.print("HTTP: ");
  Serial.println(httpCode);
  Serial.print("Resposta: ");
  Serial.println(response);

  if (httpCode < 200 || httpCode >= 300) {
    errorInfo = "HTTP " + String(httpCode);
    showHttpError(httpCode, response);
    return false;
  }

  JsonDocument doc;
  DeserializationError jsonError = deserializeJson(doc, response);

  if (jsonError) {
    errorInfo = String("JSON invalido: ") + jsonError.c_str();
    Serial.println(errorInfo);
    oledMessage("TAMPAE", "ERRO JSON", "Resposta invalida", "Veja Serial");
    return false;
  }

  // A máquina virtual aceita tanto array quanto objeto.
  // Reproduzimos exatamente essa lógica:
  // const s = Array.isArray(data) ? data[0] : data;
  JsonVariant row;

  if (doc.is<JsonArray>()) {
    JsonArray array = doc.as<JsonArray>();

    if (array.isNull() || array.size() == 0) {
      Serial.println("RPC OK: nenhuma sessao ativa.");
      return true;
    }

    row = array[0];
  } else if (doc.is<JsonObject>()) {
    row = doc.as<JsonObject>();
  } else {
    errorInfo = "Formato JSON inesperado";
    Serial.println(errorInfo);
    return false;
  }

  // EXATAMENTE os campos usados pela máquina virtual:
  // s.session_id, s.user_id, s.nome, s.evento_id
  sessionId = row["session_id"] | "";
  userName = row["nome"] | "";

  Serial.print("session_id: ");
  Serial.println(sessionId);
  Serial.print("nome: ");
  Serial.println(userName);
  Serial.print("user_id: ");
  Serial.println((const char*)(row["user_id"] | ""));
  Serial.print("evento_id: ");
  Serial.println((const char*)(row["evento_id"] | ""));

  // Mesmo fallback usado no firmware anterior, caso nome venha vazio.
  if (userName.length() == 0) {
    userName = row["user_id"] | "Usuario";
  }

  return true;
}

// ============================================================
// SESSÃO
// ============================================================
void checkSession() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiRetry >= WIFI_RETRY_INTERVAL) {
      lastWifiRetry = millis();
      connectWiFi();
    }
    return;
  }

  String userName;
  String sessionId;
  String errorInfo;

  if (!getActiveSession(userName, sessionId, errorInfo)) {
    if (errorInfo == "Wi-Fi desconectado") {
      return;
    }
    // getActiveSession já mostra o diagnóstico HTTP/JSON no OLED.
    return;
  }

  if (sessionId.length() == 0) {
    if (lastSessionId.length() > 0) {
      Serial.println("Sessao encerrada ou ainda nao existe nova sessao.");
      lastSessionId = "";
      lastUserName = "";
    }

    oledMessage("TAMPAE", "Maquina pronta", "Aguardando usuario...");
    return;
  }

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
  delay(300);

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
  Serial.println("========================================");
  Serial.println("TAMPAE - ESP32 + OLED + SUPABASE");
  Serial.println("Base: maquina virtual de teste");
  Serial.println("SDA: GPIO 21");
  Serial.println("SCL: GPIO 22");
  Serial.println("========================================");

  Serial.print("MACHINE_ID: ");
  Serial.println(MACHINE_ID);
  Serial.print("DEVICE_TOKEN: ");
  Serial.println(DEVICE_TOKEN);

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    checkSession();
  }
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
