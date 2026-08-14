#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// TAMPAÊ - TESTE DA MÁQUINA REAL
// ESP32 + OLED SSD1306 128x64 I2C
// SDA -> GPIO 21
// SCL -> GPIO 22
//
// Fluxo reproduzido diretamente da máquina virtual:
// 1. Conecta no Wi-Fi.
// 2. Procura exatamente 1 evento com status "em_andamento".
// 3. Cria uma máquina em public.machines.
// 4. Recebe o id e o device_token gerados pelo banco.
// 5. Usa ESSES valores para chamar get_active_session.
// 6. Quando o app criar uma machine_session, mostra o usuário no OLED.
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

// A partir de agora NÃO usamos mais MACHINE_ID/DEVICE_TOKEN fixos.
// O ESP32 vai criar a máquina e receber esses valores do banco,
// exatamente como a máquina virtual faz.
String machineId = "";
String deviceToken = "";
String eventId = "";
String eventName = "";

const char* MACHINE_NAME = "TampAê - ESP32";
const float MACHINE_LATITUDE = 0.0;
const float MACHINE_LONGITUDE = 0.0;

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
  if (shortName.length() > 10) shortName = shortName.substring(0, 10);
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
// SUPABASE - URLs
// ============================================================
String getRestUrl(const String& table) {
  String base = SUPABASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/rest/v1/" + table;
}

String getRpcUrl(const String& rpcName) {
  String base = SUPABASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/rest/v1/rpc/" + rpcName;
}

void addSupabaseHeaders(HTTPClient& http) {
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
}

void showHttpError(const String& operation, int httpCode, const String& response) {
  Serial.print(operation);
  Serial.print(" HTTP ");
  Serial.print(httpCode);
  Serial.print(": ");
  Serial.println(response);

  oledMessage("TAMPAE", "ERRO " + operation, "HTTP " + String(httpCode), "Veja Serial");
}

// ============================================================
// 1. EVENTO - igual ao loadSingleEvent() do HTML
// ============================================================
bool loadSingleEvent() {
  HTTPClient http;
  String url = getRestUrl("events") + "?select=id,nome,descricao,data_inicio,data_fim,status&status=eq.em_andamento";

  Serial.println();
  Serial.println("--- loadSingleEvent ---");
  Serial.println(url);

  if (!http.begin(url)) {
    Serial.println("http.begin falhou para events");
    oledMessage("TAMPAE", "Erro events", "http.begin falhou");
    return false;
  }

  http.setTimeout(10000);
  addSupabaseHeaders(http);

  int httpCode = http.GET();
  String response = http.getString();
  http.end();

  Serial.print("events HTTP: ");
  Serial.println(httpCode);
  Serial.print("events resposta: ");
  Serial.println(response);

  if (httpCode < 200 || httpCode >= 300) {
    showHttpError("EVENTS", httpCode, response);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.print("JSON events invalido: ");
    Serial.println(err.c_str());
    oledMessage("TAMPAE", "Erro JSON", "events");
    return false;
  }

  if (!doc.is<JsonArray>()) {
    Serial.println("Resposta de events nao e array.");
    oledMessage("TAMPAE", "Erro events", "Formato inesperado");
    return false;
  }

  JsonArray events = doc.as<JsonArray>();

  if (events.size() == 0) {
    eventId = "";
    eventName = "";
    Serial.println("Nenhum evento em_andamento.");
    oledMessage("TAMPAE", "Nenhum evento", "em andamento");
    return false;
  }

  if (events.size() > 1) {
    eventId = "";
    eventName = "";
    Serial.print("Existem ");
    Serial.print(events.size());
    Serial.println(" eventos em_andamento. Maquina bloqueada.");
    oledMessage("TAMPAE", "ERRO: eventos", "mais de 1 ativo");
    return false;
  }

  JsonObject event = events[0];
  eventId = event["id"] | "";
  eventName = event["nome"] | "";

  Serial.print("Evento atual: ");
  Serial.print(eventName);
  Serial.print(" | ");
  Serial.println(eventId);

  return eventId.length() > 0;
}

// ============================================================
// 2. CRIAR MÁQUINA - igual ao createMachine() do HTML
// ============================================================
bool createMachine() {
  HTTPClient http;
  String url = getRestUrl("machines");

  Serial.println();
  Serial.println("--- createMachine ---");
  Serial.println(url);

  if (!http.begin(url)) {
    Serial.println("http.begin falhou para machines");
    oledMessage("TAMPAE", "Erro machines", "http.begin falhou");
    return false;
  }

  http.setTimeout(10000);
  addSupabaseHeaders(http);
  // Equivalente ao .insert(payload).select(...).single() do HTML.
  http.addHeader("Prefer", "return=representation");

  JsonDocument payload;
  payload["nome"] = MACHINE_NAME;
  payload["latitude"] = MACHINE_LATITUDE;
  payload["longitude"] = MACHINE_LONGITUDE;
  payload["status"] = "ativa";

  String body;
  serializeJson(payload, body);

  Serial.print("machines INSERT body: ");
  Serial.println(body);

  int httpCode = http.POST(body);
  String response = http.getString();
  http.end();

  Serial.print("machines HTTP: ");
  Serial.println(httpCode);
  Serial.print("machines resposta: ");
  Serial.println(response);

  if (httpCode < 200 || httpCode >= 300) {
    showHttpError("MACHINES", httpCode, response);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.print("JSON machines invalido: ");
    Serial.println(err.c_str());
    oledMessage("TAMPAE", "Erro JSON", "machines");
    return false;
  }

  // PostgREST retorna uma lista quando usamos Prefer: return=representation.
  JsonObject machine;
  if (doc.is<JsonArray>()) {
    JsonArray rows = doc.as<JsonArray>();
    if (rows.size() == 0) {
      Serial.println("INSERT nao retornou a maquina.");
      oledMessage("TAMPAE", "Erro", "maquina nao retornada");
      return false;
    }
    machine = rows[0].as<JsonObject>();
  } else if (doc.is<JsonObject>()) {
    machine = doc.as<JsonObject>();
  } else {
    Serial.println("Formato inesperado no retorno de machines.");
    return false;
  }

  // IMPORTANTE: agora usamos o id e device_token REALMENTE GERADOS
  // pelo banco, em vez dos valores fixos usados no teste anterior.
  machineId = machine["id"] | "";
  deviceToken = machine["device_token"] | "";

  Serial.println("========================================");
  Serial.println("MAQUINA CRIADA PELO ESP32");
  Serial.print("id: ");
  Serial.println(machineId);
  Serial.print("nome: ");
  Serial.println((const char*)(machine["nome"] | ""));
  Serial.print("device_token: ");
  Serial.println(deviceToken);
  Serial.print("evento_id: ");
  Serial.println(eventId);
  Serial.print("QR payload: ");
  Serial.print("{\"machine_id\":\"");
  Serial.print(machineId);
  Serial.print("\",\"event_id\":\"");
  Serial.print(eventId);
  Serial.println("\"}");
  Serial.println("========================================");

  if (machineId.length() == 0 || deviceToken.length() == 0) {
    Serial.println("ERRO: id ou device_token nao retornado.");
    oledMessage("TAMPAE", "Erro maquina", "sem token/id");
    return false;
  }

  oledMessage("MAQUINA CRIADA", "ID:", machineId.substring(0, 18), "Token OK");
  delay(1500);
  return true;
}

// ============================================================
// 3. SESSÃO - igual ao checkSession() do HTML
// ============================================================
bool getActiveSession(String& userName, String& sessionId) {
  userName = "";
  sessionId = "";

  if (WiFi.status() != WL_CONNECTED || machineId.length() == 0 || deviceToken.length() == 0) {
    return false;
  }

  HTTPClient http;
  String url = getRpcUrl("get_active_session");

  if (!http.begin(url)) {
    Serial.println("http.begin falhou para get_active_session");
    oledMessage("TAMPAE", "Erro RPC", "http.begin falhou");
    return false;
  }

  http.setTimeout(10000);
  addSupabaseHeaders(http);

  // EXATAMENTE os mesmos parâmetros da máquina virtual.
  JsonDocument request;
  request["p_machine_id"] = machineId;
  request["p_device_token"] = deviceToken;

  String body;
  serializeJson(request, body);

  Serial.println();
  Serial.println("--- get_active_session ---");
  Serial.print("machine_id REAL: ");
  Serial.println(machineId);
  Serial.print("device_token REAL: ");
  Serial.println(deviceToken);
  Serial.print("POST body: ");
  Serial.println(body);

  int httpCode = http.POST(body);
  String response = http.getString();
  http.end();

  Serial.print("RPC HTTP: ");
  Serial.println(httpCode);
  Serial.print("RPC resposta: ");
  Serial.println(response);

  if (httpCode < 200 || httpCode >= 300) {
    showHttpError("RPC", httpCode, response);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.print("JSON RPC invalido: ");
    Serial.println(err.c_str());
    oledMessage("TAMPAE", "Erro JSON RPC", "Veja Serial");
    return false;
  }

  JsonVariant row;

  if (doc.is<JsonArray>()) {
    JsonArray array = doc.as<JsonArray>();
    if (array.size() == 0) {
      Serial.println("RPC OK: nenhuma sessao ativa.");
      return true;
    }
    row = array[0];
  } else if (doc.is<JsonObject>()) {
    row = doc.as<JsonObject>();
  } else {
    Serial.println("Formato JSON inesperado na RPC.");
    return false;
  }

  sessionId = row["session_id"] | "";
  userName = row["nome"] | "";

  if (userName.length() == 0) {
    userName = row["user_id"] | "Usuario";
  }

  Serial.print("session_id: ");
  Serial.println(sessionId);
  Serial.print("nome: ");
  Serial.println(userName);
  Serial.print("evento_id: ");
  Serial.println((const char*)(row["evento_id"] | ""));

  return true;
}

void checkSession() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiRetry >= WIFI_RETRY_INTERVAL) {
      lastWifiRetry = millis();
      connectWiFi();
    }
    return;
  }

  if (machineId.length() == 0 || deviceToken.length() == 0) return;

  String userName;
  String sessionId;

  if (!getActiveSession(userName, sessionId)) return;

  if (sessionId.length() == 0) {
    if (lastSessionId.length() > 0) {
      lastSessionId = "";
      lastUserName = "";
    }

    oledMessage("TAMPAE", "Maquina pronta", "Aguardando usuario...");
    return;
  }

  if (sessionId != lastSessionId || userName != lastUserName) {
    lastSessionId = sessionId;
    lastUserName = userName;
    showConnectedUser(userName);

    Serial.print("USUARIO CONECTADO: ");
    Serial.print(userName);
    Serial.print(" | sessao: ");
    Serial.println(sessionId);
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
    while (true) delay(1000);
  }

  oledMessage("TAMPAE", "Iniciando maquina...");
  delay(800);

  Serial.println();
  Serial.println("========================================");
  Serial.println("TAMPAE - ESP32 + OLED + SUPABASE");
  Serial.println("CRIACAO DE MAQUINA IGUAL AO HTML");
  Serial.println("========================================");

  if (!connectWiFi()) return;

  // Igual ao fluxo da máquina virtual: primeiro carrega o evento.
  if (!loadSingleEvent()) return;

  // Depois cria a máquina usando o payload do HTML.
  if (!createMachine()) return;

  // Só depois de criar a máquina começamos a procurar sessão.
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
