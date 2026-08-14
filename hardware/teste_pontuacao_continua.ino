/*
  TAMPAE - TESTE DE PONTUACAO / SESSAO CONTINUA

  OLED SSD1306 128x64 I2C
    SDA -> GPIO 21
    SCL -> GPIO 22

  LDR -> GPIO 34
    tampinha detectada quando analogRead(LDR) < 1500

  Potenciometro / balanca simulada -> GPIO 35

  Botao finalizar -> GPIO 23 / GND

  REGRA:
    13 g = 1 tampinha
    quantidade = ceil(peso / 13)

  A sessao NAO termina depois de uma coleta.
  Ela termina somente:
    1) pelo celular; ou
    2) pelo botao da maquina.

  IMPORTANTE:
    Antes de gravar este firmware, execute:
    hardware/sql_teste_pontuacao_sessao_continua.sql
*/

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <qrcode.h>
#include <math.h>

// ============================================================
// CONFIGURACAO
// ============================================================
const char* WIFI_SSID = "DAYANE";
const char* WIFI_PASSWORD = "@Felipe23";
const char* SUPABASE_URL = "https://usztjfxtbagjbnupiuxm.supabase.co";
const char* SUPABASE_ANON_KEY = "sb_publishable_SZhZ1FlWxXzd85fWRQ69Fg_j9HXqqnr";
const char* EVENT_ID = "a142ed17-e276-47e0-918f-8d1145b558c0";
const char* MACHINE_NAME = "TampAê - Teste Pontuação";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define LDR_PIN 34
#define POT_PIN 35
#define BOTAO_FINALIZAR 23
#define LIMIAR_LDR 1500
#define GRAMAS_POR_TAMPINHA 13.0f

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);

String machineId;
String deviceToken;
String qrPayload;
String sessionId;
String userId;
String nomeUsuario;
String eventoSessao;

bool sessaoAtiva = false;
bool sensorBloqueado = false;
bool botaoBloqueado = false;

unsigned long ultimaConsulta = 0;
unsigned long ultimoDebounce = 0;
unsigned long ultimaTela = 0;
const unsigned long INTERVALO_SESSAO = 1500;
const unsigned long DEBOUNCE = 80;

long contadorTampinhas = 0;
float pesoUltimaColeta = 0;
int quantidadeUltimaColeta = 0;

// ============================================================
// OLED
// ============================================================
void oledText(const String& a, const String& b = "", const String& c = "", const String& d = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(a);
  if (b.length()) display.println(b);
  if (c.length()) display.println(c);
  if (d.length()) display.println(d);
  display.display();
}

void mostrarAguardando() {
  oledText("TAMPAE", "Aguardando usuario", "IP: " + WiFi.localIP().toString(), "Leia o QR no celular");
}

void mostrarSessao() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Usuario: ");
  display.println(nomeUsuario.length() ? nomeUsuario : "Conectado");
  display.setCursor(0, 13);
  display.print("Total: ");
  display.print(contadorTampinhas);
  display.println(" tampinhas");
  display.setCursor(0, 26);
  display.print("Peso: ");
  display.print(analogRead(POT_PIN));
  display.println(" ADC");
  display.setCursor(0, 39);
  display.print("Ultima: ");
  display.print(quantidadeUltimaColeta);
  display.print(" / ");
  display.print(pesoUltimaColeta, 0);
  display.println("g");
  display.setCursor(0, 53);
  display.println("Botao = finalizar");
  display.display();
}

// ============================================================
// WIFI / HTTP
// ============================================================
void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando Wi-Fi");
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi OK. IP: ");
    Serial.println(WiFi.localIP());
  } else Serial.println("Falha no Wi-Fi.");
}

String restUrl(const String& tabela) {
  String b = SUPABASE_URL;
  while (b.endsWith("/")) b.remove(b.length() - 1);
  return b + "/rest/v1/" + tabela;
}

String rpcUrl(const String& rpc) {
  String b = SUPABASE_URL;
  while (b.endsWith("/")) b.remove(b.length() - 1);
  return b + "/rest/v1/rpc/" + rpc;
}

void headers(HTTPClient& http) {
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
}

String chamarRPC(const char* nome, const String& body) {
  HTTPClient http;
  if (!http.begin(rpcUrl(nome))) return "";
  headers(http);
  int code = http.POST(body);
  String response = http.getString();
  http.end();
  Serial.print("RPC ");
  Serial.print(nome);
  Serial.print(" HTTP: ");
  Serial.println(code);
  if (code < 200 || code >= 300) {
    Serial.println(response);
    return "";
  }
  return response;
}

// ============================================================
// MAQUINA
// ============================================================
bool criarMaquina() {
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
  Serial.print("machines HTTP: "); Serial.println(code);
  Serial.print("machines resposta: "); Serial.println(response);
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

  Serial.println("========================================");
  Serial.println("MAQUINA CRIADA");
  Serial.print("MACHINE_ID: "); Serial.println(machineId);
  Serial.print("DEVICE_TOKEN: "); Serial.println(deviceToken);
  Serial.print("EVENT_ID: "); Serial.println(EVENT_ID);
  Serial.println("========================================");
  return true;
}

// ============================================================
// QR WEB
// ============================================================
String makeQrSvg() {
  struct Capture { int size; bool modules[177][177]; };
  static Capture c;
  c.size = 0;

  esp_qrcode_config_t config = {};
  config.max_qrcode_version = 10;
  config.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
  config.display_func = [](esp_qrcode_handle_t qr) {
    c.size = esp_qrcode_get_size(qr);
    for (int y = 0; y < c.size; y++)
      for (int x = 0; x < c.size; x++)
        c.modules[y][x] = esp_qrcode_get_module(qr, x, y);
  };

  if (esp_qrcode_generate(&config, qrPayload.c_str()) != ESP_OK || c.size <= 0)
    return "<p>Erro ao gerar QR.</p>";

  const int qz = 4;
  int full = c.size + qz * 2;
  String svg;
  svg.reserve(full * full / 2);
  svg += "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 " + String(full) + " " + String(full) + "' shape-rendering='crispEdges' class='qr'>";
  svg += "<rect width='100%' height='100%' fill='white'/>";
  for (int y = 0; y < c.size; y++) {
    for (int x = 0; x < c.size; x++) {
      if (c.modules[y][x]) {
        svg += "<rect x='" + String(x + qz) + "' y='" + String(y + qz) + "' width='1' height='1' fill='black'/>";
      }
    }
  }
  svg += "</svg>";
  return svg;
}

void handleRoot() {
  String svg = makeQrSvg();
  String page;
  page.reserve(svg.length() + 1800);
  page += "<!doctype html><html lang='pt-BR'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>TampAê</title><style>body{margin:0;min-height:100vh;background:#111;color:#fff;font-family:Arial;display:flex;align-items:center;justify-content:center;padding:8px;box-sizing:border-box}.card{width:100%;max-width:720px;text-align:center}.qr{display:block;width:min(96vw,680px);height:auto;aspect-ratio:1;background:#fff;border:8px solid #fff;margin:auto}.t{font-size:20px;font-weight:bold;margin:8px}.h{font-size:13px}.id{font-size:10px;opacity:.6;word-break:break-all;margin-top:5px}</style></head><body><main class='card'>";
  page += svg;
  page += "<div class='t'>TAMPAÊ - TESTE</div><div class='h'>Escaneie para iniciar a sessão</div><div class='id'>" + machineId + "</div></main></body></html>";
  server.send(200, "text/html; charset=utf-8", page);
}

void iniciarServidor() {
  server.on("/", HTTP_GET, handleRoot);
  server.begin();
  Serial.println("========================================");
  Serial.println("SERVIDOR WEB INICIADO");
  Serial.print("URL: http://"); Serial.println(WiFi.localIP());
  Serial.println("========================================");
}

// ============================================================
// SESSAO CONTINUA
// ============================================================
bool consultarSessao() {
  JsonDocument req;
  req["p_machine_id"] = machineId;
  req["p_device_token"] = deviceToken;
  String body;
  serializeJson(req, body);

  String response = chamarRPC("get_active_session_continua", body);
  if (!response.length()) return false;
  if (response == "[]") {
    if (sessaoAtiva) {
      Serial.println("Sessao fechada pelo celular ou finalizada.");
      sessaoAtiva = false;
      sessionId = "";
      userId = "";
      nomeUsuario = "";
      contadorTampinhas = 0;
      quantidadeUltimaColeta = 0;
      mostrarAguardando();
    }
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) return false;
  JsonArray a = doc.as<JsonArray>();
  if (a.isNull() || !a.size()) return false;
  JsonObject s = a[0].as<JsonObject>();
  String nova = s["session_id"] | "";
  if (!nova.length()) return false;

  if (!sessaoAtiva || sessionId != nova) {
    sessionId = nova;
    userId = s["user_id"] | "";
    nomeUsuario = s["nome"] | "";
    eventoSessao = s["evento_id"] | "";
    sessaoAtiva = true;
    contadorTampinhas = 0;
    quantidadeUltimaColeta = 0;
    pesoUltimaColeta = 0;
    sensorBloqueado = false;

    Serial.println();
    Serial.println("========================================");
    Serial.println("USUARIO CONECTADO");
    Serial.print("NOME: "); Serial.println(nomeUsuario);
    Serial.print("USER_ID: "); Serial.println(userId);
    Serial.print("EVENTO_ID: "); Serial.println(eventoSessao);
    Serial.print("SESSION_ID: "); Serial.println(sessionId);
    Serial.println("Sessao continua apos cada coleta.");
    Serial.println("========================================");
  }
  return true;
}

// ============================================================
// BALANCA
// ============================================================
float lerPesoGramas() {
  int adc = analogRead(POT_PIN);
  return ((float)adc / 4095.0f) * 1000.0f;
}

int calcularTampinhas(float peso) {
  if (peso <= 0.0f) return 0;
  return (int)ceilf(peso / GRAMAS_POR_TAMPINHA);
}

// ============================================================
// COLETA
// ============================================================
bool registrarColeta() {
  if (!sessaoAtiva || !sessionId.length()) {
    Serial.println("Sem sessao ativa.");
    return false;
  }

  float peso = lerPesoGramas();
  int quantidade = calcularTampinhas(peso);

  if (quantidade <= 0) {
    Serial.println("Peso insuficiente para registrar coleta.");
    return false;
  }

  JsonDocument req;
  req["p_machine_id"] = machineId;
  req["p_device_token"] = deviceToken;
  req["p_session_id"] = sessionId;
  req["p_tipo_coleta"] = "unitaria";
  req["p_quantidade_real"] = quantidade;
  req["p_quantidade_estimada"] = nullptr;
  req["p_peso_real_gramas"] = nullptr;
  req["p_peso_estimado_gramas"] = peso;

  String body;
  serializeJson(req, body);

  Serial.println();
  Serial.println("TAMPINHA DETECTADA");
  Serial.print("Peso: "); Serial.print(peso, 1); Serial.println(" g");
  Serial.print("Quantidade: "); Serial.println(quantidade);
  Serial.println("13 g = 1 tampinha; arredondamento para cima.");
  Serial.println(body);

  String response = chamarRPC("registrar_coleta_continua", body);
  if (!response.length()) {
    Serial.println("Falha ao registrar coleta.");
    return false;
  }

  contadorTampinhas += quantidade;
  quantidadeUltimaColeta = quantidade;
  pesoUltimaColeta = peso;

  Serial.print("COLETA REGISTRADA: "); Serial.println(response);
  Serial.print("TOTAL DA SESSAO: "); Serial.println(contadorTampinhas);
  Serial.println("SESSAO CONTINUA ATIVA.");
  return true;
}

void verificarLDR() {
  int valor = analogRead(LDR_PIN);
  if (valor < LIMIAR_LDR) {
    if (!sensorBloqueado) {
      sensorBloqueado = true;
      registrarColeta();
    }
  } else {
    sensorBloqueado = false;
  }
}

// ============================================================
// BOTAO
// ============================================================
void finalizarSessao() {
  if (!sessaoAtiva || !sessionId.length()) return;

  JsonDocument req;
  req["p_machine_id"] = machineId;
  req["p_device_token"] = deviceToken;
  req["p_session_id"] = sessionId;
  String body;
  serializeJson(req, body);

  Serial.println();
  Serial.println("BOTAO DE FINALIZACAO");
  String response = chamarRPC("encerrar_sessao_maquina", body);

  if (response.length()) {
    Serial.println("SESSAO FINALIZADA PELA MAQUINA.");
    Serial.print("TOTAL: "); Serial.println(contadorTampinhas);
    sessaoAtiva = false;
    sessionId = "";
    userId = "";
    nomeUsuario = "";
    contadorTampinhas = 0;
    quantidadeUltimaColeta = 0;
    sensorBloqueado = false;
    mostrarAguardando();
  }
}

void verificarBotao() {
  if (digitalRead(BOTAO_FINALIZAR) != LOW) {
    botaoBloqueado = false;
    return;
  }
  if (botaoBloqueado || millis() - ultimoDebounce < DEBOUNCE) return;
  ultimoDebounce = millis();
  botaoBloqueado = true;
  finalizarSessao();
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(LDR_PIN, INPUT);
  pinMode(POT_PIN, INPUT);
  pinMode(BOTAO_FINALIZAR, INPUT_PULLUP);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERRO: OLED nao encontrado.");
    while (true) delay(1000);
  }

  oledText("TAMPAE", "Iniciando...");
  conectarWiFi();
  if (WiFi.status() != WL_CONNECTED) return;

  oledText("TAMPAE", "Criando maquina...");
  if (!criarMaquina()) {
    oledText("ERRO", "Nao criou maquina");
    return;
  }

  iniciarServidor();
  mostrarAguardando();
}

void loop() {
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
    delay(500);
    return;
  }

  if (millis() - ultimaConsulta >= INTERVALO_SESSAO) {
    ultimaConsulta = millis();
    consultarSessao();
  }

  if (sessaoAtiva) {
    verificarLDR();
    verificarBotao();
    if (millis() - ultimaTela >= 250) {
      ultimaTela = millis();
      mostrarSessao();
    }
  }

  delay(10);
}
