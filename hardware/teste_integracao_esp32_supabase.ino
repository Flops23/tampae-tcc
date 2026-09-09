#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// TAMPAÊ - TESTE DE INTEGRAÇÃO ESP32 + SUPABASE + APP
// ============================================================

const char* WIFI_SSID = "esp32";
const char* WIFI_PASSWORD = "123456";

const char* SUPABASE_URL = "https://jtmbsyharkxrpnkunbuj.supabase.co";
const char* SUPABASE_KEY = "sb_publishable_TeblGQP9D6s24o0IUiZbAg_CKxmxgPo";

const char* MACHINE_ID = "379a1459-797e-47e5-9a73-de949e72f9f5";
const char* MACHINE_TOKEN = "62263534-37bb-451e-a89f-9c77c3234ddd";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define PINO_LDR 35
#define PINO_POT 34
#define SDA_PIN 21
#define SCL_PIN 22

#define LIMIAR_LDR 300
#define DEBOUNCE_MS 500
#define PESO_MAXIMO_GRAMAS 500.0

String sessionId = "";
String userId = "";
String userName = "";
String eventId = "";
bool sessaoAtiva = false;

int passagens = 0;
int pontos = 0;
float pesoGramas = 0;

bool estadoAnteriorLDR = false;
unsigned long ultimaPassagem = 0;
unsigned long ultimaConsultaSessao = 0;

void mostrarTela(const String& linha1, const String& linha2 = "", const String& linha3 = "", const String& linha4 = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(linha1);

  if (linha2.length()) {
    display.setCursor(0, 16);
    display.println(linha2);
  }
  if (linha3.length()) {
    display.setCursor(0, 32);
    display.println(linha3);
  }
  if (linha4.length()) {
    display.setCursor(0, 48);
    display.println(linha4);
  }
  display.display();
}

void mostrarOperacao() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("TAMPAE");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 16);
  if (sessaoAtiva) {
    display.print("Usuario: ");
    display.println(userName.substring(0, 17));
  } else {
    display.println("Aguardando usuario");
  }

  display.setCursor(0, 31);
  display.print("Passagens: ");
  display.println(passagens);

  display.setCursor(0, 44);
  display.print("Peso: ");
  display.print(pesoGramas, 0);
  display.println(" g");

  display.setCursor(0, 57);
  display.print("Pontos: ");
  display.println(pontos);
  display.display();
}

// ============================================================
// HTTP SUPABASE
// ============================================================
bool prepararHttp(HTTPClient& http, WiFiClientSecure& client, const String& endpoint) {
  if (WiFi.status() != WL_CONNECTED) return false;

  client.setInsecure();

  if (!http.begin(client, endpoint)) {
    return false;
  }

  // A chave sb_publishable deve ser enviada como apikey.
  // Nao usamos Authorization: Bearer porque essa chave nao e um JWT.
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  return true;
}

bool testarBanco() {
  WiFiClientSecure client;
  HTTPClient http;

  String endpoint = String(SUPABASE_URL) + "/rest/v1/machines?id=eq." + MACHINE_ID + "&select=id";

  if (!prepararHttp(http, client, endpoint)) {
    Serial.println("ERRO: nao foi possivel iniciar conexao com Supabase.");
    return false;
  }

  int code = http.GET();
  String response = http.getString();
  http.end();

  if (code == 200) {
    Serial.println("Supabase conectado.");
    Serial.println("Banco OK: maquina encontrada.");
    return true;
  }

  Serial.print("ERRO Supabase. HTTP: ");
  Serial.println(code);
  Serial.print("Resposta Supabase: ");
  Serial.println(response);
  return false;
}

void consultarSessao() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  HTTPClient http;
  String endpoint = String(SUPABASE_URL) + "/rest/v1/rpc/get_active_session";

  if (!prepararHttp(http, client, endpoint)) return;

  String body = "{\"p_machine_id\":\"" + String(MACHINE_ID) + "\",\"p_device_token\":\"" + String(MACHINE_TOKEN) + "\"}";
  int code = http.POST(body);

  if (code != 200) {
    if (sessaoAtiva) {
      Serial.print("Sessao: erro na consulta. HTTP: ");
      Serial.println(code);
      Serial.print("Resposta Supabase: ");
      Serial.println(http.getString());
    }
    http.end();
    return;
  }

  String response = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error) return;

  JsonVariant item;

  if (doc.is<JsonArray>()) {
    JsonArray arr = doc.as<JsonArray>();
    if (arr.size() > 0) item = arr[0];
  } else if (doc.is<JsonObject>()) {
    item = doc.as<JsonObject>();
  }

  if (item.isNull() || item["session_id"].isNull()) {
    if (sessaoAtiva) {
      sessaoAtiva = false;
      sessionId = "";
      userId = "";
      userName = "";
      eventId = "";
      Serial.println("Sessao encerrada ou expirada.");
      mostrarOperacao();
    }
    return;
  }

  String novaSessao = item["session_id"].as<String>();
  String novoNome = item["nome"].as<String>();

  if (!sessaoAtiva || sessionId != novaSessao) {
    sessionId = novaSessao;
    userId = item["user_id"].as<String>();
    userName = novoNome;
    eventId = item["evento_id"].as<String>();
    sessaoAtiva = true;

    Serial.println("Usuario conectado pelo app.");
    Serial.print("Nome: ");
    Serial.println(userName);
    mostrarOperacao();
  }
}

bool registrarTampa() {
  if (!sessaoAtiva || sessionId.length() == 0) return false;

  WiFiClientSecure client;
  HTTPClient http;
  String endpoint = String(SUPABASE_URL) + "/rest/v1/rpc/registrar_coleta";

  if (!prepararHttp(http, client, endpoint)) return false;

  String body = "{";
  body += "\"p_machine_id\":\"" + String(MACHINE_ID) + "\",";
  body += "\"p_device_token\":\"" + String(MACHINE_TOKEN) + "\",";
  body += "\"p_session_id\":\"" + sessionId + "\",";
  body += "\"p_tipo_coleta\":\"unitaria\",";
  body += "\"p_quantidade_real\":1,";
  body += "\"p_quantidade_estimada\":null,";
  body += "\"p_peso_real_gramas\":null,";
  body += "\"p_peso_estimado_gramas\":null";
  body += "}";

  int code = http.POST(body);
  String response = http.getString();
  http.end();

  if (code >= 200 && code < 300) {
    passagens++;
    pontos++;
    Serial.println("Coleta registrada no banco.");
    mostrarOperacao();
    return true;
  }

  Serial.print("ERRO ao registrar coleta. HTTP: ");
  Serial.println(code);
  Serial.print("Resposta Supabase: ");
  Serial.println(response);
  return false;
}

void conectarWiFi() {
  Serial.println();
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long inicio = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi conectado.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
  } else {
    Serial.println("ERRO: Wi-Fi nao conectado.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERRO: OLED nao encontrado.");
    while (true) delay(1000);
  }

  analogReadResolution(12);
  analogSetPinAttenuation(PINO_LDR, ADC_11db);
  analogSetPinAttenuation(PINO_POT, ADC_11db);

  mostrarTela("TAMPAE", "Iniciando...");

  Serial.println("========================================");
  Serial.println("TAMPAE - INTEGRACAO ESP32 + APP + BANCO");
  Serial.println("========================================");

  conectarWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    mostrarTela("Wi-Fi conectado", "Testando banco...");

    if (testarBanco()) {
      mostrarTela("Banco conectado", "Aguardando app...");
    } else {
      mostrarTela("Banco: ERRO", "Verifique conexao");
    }
  } else {
    mostrarTela("Wi-Fi: ERRO", "Verifique rede");
  }

  delay(1500);
  mostrarOperacao();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long ultimaTentativaWiFi = 0;

    if (millis() - ultimaTentativaWiFi >= 10000) {
      ultimaTentativaWiFi = millis();
      conectarWiFi();
    }

    delay(100);
    return;
  }

  if (millis() - ultimaConsultaSessao >= 1500) {
    ultimaConsultaSessao = millis();
    consultarSessao();
  }

  int valorPot = analogRead(PINO_POT);
  pesoGramas = ((float)valorPot / 4095.0) * PESO_MAXIMO_GRAMAS;

  int valorLDR = analogRead(PINO_LDR);
  bool objetoDetectado = (valorLDR < LIMIAR_LDR);

  if (sessaoAtiva && objetoDetectado && !estadoAnteriorLDR) {
    unsigned long agora = millis();

    if (agora - ultimaPassagem >= DEBOUNCE_MS) {
      if (registrarTampa()) {
        ultimaPassagem = agora;
      }
    }
  }

  estadoAnteriorLDR = objetoDetectado;
  delay(50);
}
