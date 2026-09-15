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
#define PIN_POTENCIOMETRO 34
#define PIN_LDR 35
#define PIN_BOTAO 25

const float GRAMAS_POR_TAMPINHA = 12.0f;
const int LIMIAR_POTENCIOMETRO = 80;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool sessaoAtiva = false;
String sessionId, userId, userName, eventId;
unsigned long ultimaConsulta = 0;
unsigned long ultimaTentativaWiFi = 0;
int ultimaLeituraPot = 0;
int tampinhasSimuladas = 0;

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
    display.setCursor(0, 16); display.println("Usuario:");
    String nome = userName;
    if (nome.length() > 20) nome = nome.substring(0, 20);
    display.setCursor(0, 28); display.println(nome);
    display.setCursor(0, 44); display.print(tampinhasSimuladas); display.println(" tampinhas");
  } else {
    display.setCursor(0, 28); display.println("Aguardando usuario");
  }
  display.display();
}

bool prepararHttp(HTTPClient& http, const String& url) {
  if (!http.begin(url)) {
    Serial.println("[HTTP] Falha ao iniciar conexao");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  return true;
}

void testarBanco() {
  Serial.println("[SUPABASE] Testando conexao com o banco...");
  HTTPClient http;
  if (!prepararHttp(http, String(SUPABASE_URL) + "/rest/v1/")) return;
  int status = http.GET();
  Serial.printf("[SUPABASE] Status HTTP: %d\n", status);
  Serial.println(http.getString());
  http.end();
}

void registrarColeta(int quantidade) {
  if (!sessaoAtiva || quantidade <= 0 || WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/collections";
  if (!prepararHttp(http, url)) return;

  JsonDocument doc;
  doc["session_id"] = sessionId;
  doc["machine_id"] = MACHINE_ID;
  doc["user_id"] = userId;
  doc["event_id"] = eventId;
  doc["quantity"] = quantidade;
  doc["weight_grams"] = quantidade * GRAMAS_POR_TAMPINHA;
  String body;
  serializeJson(doc, body);

  int status = http.POST(body);
  Serial.printf("[COLETA] %d tampinha(s), %.0f g | HTTP %d\n", quantidade, quantidade * GRAMAS_POR_TAMPINHA, status);
  if (status >= 200 && status < 300) {
    tampinhasSimuladas += quantidade;
    mostrarOperacao();
  } else {
    Serial.println(http.getString());
  }
  http.end();
}

void lerPotenciometro() {
  int leitura = analogRead(PIN_POTENCIOMETRO);
  int diferenca = abs(leitura - ultimaLeituraPot);
  ultimaLeituraPot = leitura;

  if (!sessaoAtiva || diferenca < LIMIAR_POTENCIOMETRO) return;

  int quantidade = max(1, (int)round((float)leitura / 4095.0f * 10.0f));
  registrarColeta(quantidade);
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
  Serial.printf("[SESSAO] HTTP %d | %s\n", status, resposta.c_str());

  if (status < 200 || status >= 300) return;

  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, resposta);
  if (erro != DeserializationError::Ok || !doc["session_id"].is<const char*>()) {
    if (sessaoAtiva) {
      sessaoAtiva = false;
      sessionId = userId = userName = eventId = "";
      tampinhasSimuladas = 0;
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
    tampinhasSimuladas = 0;
    Serial.println("[SESSAO] Nova sessao encontrada");
    Serial.println("[SESSAO] Usuario: " + userName);
    mostrarOperacao();
  }
}

void conectarWiFi() {
  Serial.println("[WIFI] Tentando conectar...");
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.disconnect(true);
  delay(300);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  mostrarTela("TAMPAE", "Conectando Wi-Fi...");

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WIFI] Conectado");
    Serial.print("[WIFI] MAC: "); Serial.println(WiFi.macAddress());
    Serial.print("[WIFI] IP: "); Serial.println(WiFi.localIP());
    mostrarOperacao();
    testarBanco();
  } else {
    mostrarTela("TAMPAE", "Falha Wi-Fi", "Ver Serial");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("TAMPAE - ESP32V1 INICIANDO");

  pinMode(PIN_POTENCIOMETRO, INPUT);
  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_BOTAO, INPUT_PULLUP);

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[OLED] Falha ao iniciar display");
  }

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

  lerPotenciometro();
  delay(80);
}
