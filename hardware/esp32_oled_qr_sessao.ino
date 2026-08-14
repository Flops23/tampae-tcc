#include <WiFi.h>
#include <WebServer.h>
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
WebServer server(80);

const char* WIFI_SSID = "DAYANE";
const char* WIFI_PASSWORD = "@Felipe23";
const char* SUPABASE_URL = "https://usztjfxtbagjbnupiuxm.supabase.co";
const char* SUPABASE_ANON_KEY = "sb_publishable_SZhZ1FlWxXzd85fWRQ69Fg_j9HXqqnr";
const char* EVENT_ID = "a142ed17-e276-47e0-918f-8d1145b558c0";
const char* MACHINE_NAME = "TampAê - ESP32";

String machineId = "";
String deviceToken = "";
String qrPayload = "";
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

String htmlEscape(const String& s) {
  String out = s;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  return out;
}

String makeQrSvg(const String& payload) {
  struct Capture {
    int size;
    bool modules[177][177];
  };
  static Capture capture;
  capture.size = 0;

  esp_qrcode_config_t config = {};
  config.max_qrcode_version = 10;
  config.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
  config.display_func = [](esp_qrcode_handle_t qr) {
    capture.size = esp_qrcode_get_size(qr);
    for (int y = 0; y < capture.size; y++) {
      for (int x = 0; x < capture.size; x++) {
        capture.modules[y][x] = esp_qrcode_get_module(qr, x, y);
      }
    }
  };

  esp_err_t result = esp_qrcode_generate(&config, payload.c_str());
  if (result != ESP_OK || capture.size <= 0) return "<p>Erro ao gerar QR Code.</p>";

  const int qz = 4;
  int full = capture.size + qz * 2;
  String svg;
  svg.reserve(full * full / 2);
  svg += "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 ";
  svg += String(full) + " " + String(full) + "' shape-rendering='crispEdges' class='qr'>";
  svg += "<rect width='100%' height='100%' fill='white'/>";
  for (int y = 0; y < capture.size; y++) {
    for (int x = 0; x < capture.size; x++) {
      if (capture.modules[y][x]) {
        svg += "<rect x='" + String(x + qz) + "' y='" + String(y + qz) + "' width='1' height='1' fill='black'/>";
      }
    }
  }
  svg += "</svg>";
  return svg;
}

void handleRoot() {
  String svg = makeQrSvg(qrPayload);
  String page;
  page.reserve(svg.length() + 2200);
  page += "<!doctype html><html lang='pt-BR'><head><meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>";
  page += "<title>TampAê - QR</title><style>";
  page += "*{box-sizing:border-box}body{margin:0;min-height:100vh;background:#111;display:flex;align-items:center;justify-content:center;font-family:Arial,sans-serif;color:#fff;padding:8px}";
  page += ".card{width:100%;max-width:720px;text-align:center}.qr{display:block;width:min(96vw,680px);height:auto;aspect-ratio:1;margin:auto;background:#fff;border:8px solid #fff;border-radius:3px}.title{font-size:20px;font-weight:700;margin:8px 0 3px}.hint{font-size:13px;margin-top:4px}.info{font-size:11px;opacity:.7;word-break:break-all;margin-top:4px}</style></head><body><main class='card'>";
  page += svg;
  page += "<div class='title'>TAMPAÊ</div><div class='hint'>Escaneie este QR pelo aplicativo</div><div class='info'>";
  page += htmlEscape(machineId);
  page += "</div></main></body></html>";
  server.send(200, "text/html; charset=utf-8", page);
}

void handleInfo() {
  String json = "{\"machine_id\":\"" + machineId + "\",\"event_id\":\"" + String(EVENT_ID) + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  server.send(200, "application/json", json);
}

void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/info", HTTP_GET, handleInfo);
  server.begin();
  Serial.println();
  Serial.println("========================================");
  Serial.println("SERVIDOR WEB INICIADO");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("URL: http://");
  Serial.println(WiFi.localIP());
  Serial.println("QR DISPONIVEL");
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
  req["latitude"] = 0.0;
  req["longitude"] = 0.0;
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
  if (code < 200 || code >= 300) return false;

  JsonDocument doc;
  if (deserializeJson(doc, response)) return false;
  JsonObject row;
  if (doc.is<JsonArray>()) {
    JsonArray a = doc.as<JsonArray>();
    if (!a.size()) return false;
    row = a[0].as<JsonObject>();
  } else row = doc.as<JsonObject>();

  machineId = row["id"] | "";
  deviceToken = row["device_token"] | "";
  if (!machineId.length() || !deviceToken.length()) return false;

  qrPayload = String("{\"machine_id\":\"") + machineId + "\",\"event_id\":\"" + EVENT_ID + "\"}";

  Serial.println();
  Serial.println("========================================");
  Serial.println("MAQUINA CRIADA");
  Serial.print("MACHINE_ID: "); Serial.println(machineId);
  Serial.print("DEVICE_TOKEN: "); Serial.println(deviceToken);
  Serial.print("EVENT_ID: "); Serial.println(EVENT_ID);
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
    Serial.print("RPC HTTP: "); Serial.println(code);
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
  } else if (doc.is<JsonObject>()) row = doc.as<JsonObject>();
  else return;

  String sessionId = row["session_id"] | "";
  if (!sessionId.length() || sessionId == lastSessionId) return;
  lastSessionId = sessionId;

  String userName = row["nome"] | "";
  String sessionEventId = row["evento_id"] | "";
  Serial.println();
  Serial.println("========================================");
  Serial.println("USUARIO CONECTADO");
  Serial.print("NOME: "); Serial.println(userName.length() ? userName : "Usuario");
  Serial.print("EVENTO_ID: "); Serial.println(sessionEventId.length() ? sessionEventId : "NULL");
  Serial.print("SESSION_ID: "); Serial.println(sessionId);
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
    Serial.println("ERRO: createMachine() falhou.");
    return;
  }

  // Inicia o servidor ANTES de qualquer tentativa de gerar o QR.
  // Assim o acesso web fica garantido mesmo se houver problema na renderizacao do QR.
  startWebServer();
  oledMessage("TAMPAE", "QR pronto", WiFi.localIP().toString());
}

void loop() {
  server.handleClient();
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    delay(1000);
    return;
  }
  if (millis() - lastPoll >= POLL_INTERVAL) {
    lastPoll = millis();
    checkSession();
  }
  delay(2);
}
