#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =========================
// CONFIGURAÇÕES DA MÁQUINA
// =========================
const char* WIFI_SSID = "PREENCHA_AQUI";
const char* WIFI_PASSWORD = "PREENCHA_AQUI";
const char* SUPABASE_URL = "PREENCHA_AQUI";
const char* SUPABASE_KEY = "PREENCHA_AQUI";
const char* MACHINE_ID = "PREENCHA_AQUI";
const char* MACHINE_TOKEN = "PREENCHA_AQUI";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool sessaoAtiva = false;
String sessionId, userId, userName, eventId;
unsigned long ultimaConsulta = 0;
unsigned long ultimaTentativaWiFi = 0;

void mostrarTela(const String& linha1, const String& linha2 = "", const String& linha3 = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0); display.println(linha1);
  display.setCursor(0, 20); display.println(linha2);
  display.setCursor(0, 40); display.println(linha3);
  display.display();
}

void mostrarOperacao() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0); display.println("TAMPAE");
  if (sessaoAtiva) {
    display.setCursor(0, 20); display.println("Usuario:");
    String nome = userName;
    if (nome.length() > 20) nome = nome.substring(0, 20);
    display.setCursor(0, 38); display.println(nome);
  } else {
    display.setCursor(0, 28); display.println("Aguardando usuario");
  }
  display.display();
}

bool prepararHttp(HTTPClient& http, const String& url) {
  if (!http.begin(url)) return false;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  return true;
}

void testarBanco() {
  HTTPClient http;
  if (!prepararHttp(http, String(SUPABASE_URL) + "/rest/v1/")) return;
  http.GET();
  http.end();
}

void consultarSessao() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/rpc/get_active_session";
  if (!prepararHttp(http, url)) return;
  JsonDocument request;
  request["p_machine_id"] = MACHINE_ID;
  request["p_device_token"] = MACHINE_TOKEN;
  String body;
  serializeJson(request, body);
  int status = http.POST(body);
  String resposta = http.getString();
  http.end();
  if (status < 200 || status >= 300) return;

  JsonDocument doc;
  if (deserializeJson(doc, resposta) != DeserializationError::Ok || !doc["session_id"].is<const char*>()) {
    if (sessaoAtiva) {
      sessaoAtiva = false;
      sessionId = userId = userName = eventId = "";
      mostrarOperacao();
    }
    return;
  }

  String novaSessao = doc["session_id"].as<String>();
  if (!sessaoAtiva || novaSessao != sessionId) {
    sessionId = novaSessao;
    userId = doc["user_id"].as<String>();
    userName = doc["nome"].as<String>();
    eventId = doc["evento_id"].as<String>();
    sessaoAtiva = true;
    mostrarOperacao();
  }
}

void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  mostrarTela("TAMPAE", "Conectando Wi-Fi...");
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  mostrarTela("TAMPAE", "Iniciando...");
  conectarWiFi();
}

void loop() {
  unsigned long agora = millis();
  if (WiFi.status() != WL_CONNECTED && agora - ultimaTentativaWiFi >= 10000) {
    ultimaTentativaWiFi = agora;
    conectarWiFi();
  }
  if (WiFi.status() == WL_CONNECTED && agora - ultimaConsulta >= 1500) {
    ultimaConsulta = agora;
    consultarSessao();
  }
  delay(20);
}
