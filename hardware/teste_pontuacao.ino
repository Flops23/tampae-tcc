/*
  ============================================================
  TAMPAE - TESTE DE PONTUACAO / ESP32
  ============================================================

  OLED 128x64 SSD1306 I2C
    SDA -> GPIO 21
    SCL -> GPIO 22

  LDR
    OUT -> GPIO 34
    Tampinha detectada quando analogRead(LDR) < 1500

  Potenciometro / balanca simulada
    Pino central -> GPIO 35

  Botao finalizar sessao
    Pino -> GPIO 23
    Outro pino -> GND
    INPUT_PULLUP

  FLUXO REAL:
    ESP32 cria uma maquina no Supabase
    -> servidor web local mostra QR grande no celular
    -> QR leva machine_id + event_id
    -> app cria a sessao
    -> ESP32 encontra a sessao por get_active_session
    -> LDR detecta tampinha
    -> registra_coleta grava a coleta real no Supabase
    -> potenciometro fornece o peso da coleta
    -> botao encerra o teste localmente

  IMPORTANTE:
    A pontuacao nao e calculada artificialmente neste firmware.
    A coleta e enviada pela RPC registrar_coleta, deixando o
    backend aplicar as regras de pontuacao que ja existem no app.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <qrcode.h>

// ============================================================
// CONFIGURACAO
// ============================================================

const char* WIFI_SSID = "DAYANE";
const char* WIFI_PASSWORD = "@Felipe23";

const char* SUPABASE_URL = "https://usztjfxtbagjbnupiuxm.supabase.co";
const char* SUPABASE_ANON_KEY = "sb_publishable_SZhZ1FlWxXzd85fWRQ69Fg_j9HXqqnr";

const char* EVENT_ID = "a142ed17-e276-47e0-918f-8d1145b558c0";
const char* MACHINE_NAME = "TampAê - Teste Pontuação";

// ============================================================
// OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);

// ============================================================
// PINOS
// ============================================================

#define LDR_PIN 34
#define POT_PIN 35
#define BOTAO_FINALIZAR 23
#define LIMIAR_LDR 1500

// ============================================================
// ESTADO
// ============================================================

String machineId = "";
String deviceToken = "";
String qrPayload = "";

String sessionId = "";
String userId = "";
String nomeUsuario = "";
String eventoSessao = "";
String ultimaSessionId = "";

bool sessaoAtiva = false;
bool sessaoFinalizada = false;
bool sensorBloqueado = false;

int contadorTampinhas = 0;
int pesoAtual = 0;
int pesoFinal = 0;

unsigned long ultimaConsultaSessao = 0;
unsigned long ultimaTela = 0;
unsigned long ultimoDebounceBotao = 0;

const unsigned long INTERVALO_SESSAO = 1500;
const unsigned long INTERVALO_TELA = 250;

// ============================================================
// OLED
// ============================================================

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

void mostrarAguardando() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(24, 2);
  display.println("TAMPAE");

  display.setTextSize(1);
  display.setCursor(17, 29);
  display.println("Aguardando usuario");
  display.setCursor(28, 45);
  display.println("Leia o QR");

  display.display();
}

void mostrarSessao() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("Usuario: ");
  if (nomeUsuario.length()) display.println(nomeUsuario);
  else display.println("Conectado");

  display.setCursor(0, 14);
  display.print("Tampinhas: ");
  display.println(contadorTampinhas);

  display.setCursor(0, 28);
  display.print("Peso: ");
  display.print(pesoAtual);
  display.println(" g");

  display.setCursor(0, 43);
  display.println("LDR < 1500 = coleta");

  display.setCursor(0, 56);
  display.println("Botao = finalizar");

  display.display();
}

void mostrarFinalizacao() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(20, 0);
  display.println("TESTE FINALIZADO");

  display.setCursor(0, 16);
  display.print("Usuario: ");
  display.println(nomeUsuario);

  display.setCursor(0, 30);
  display.print("Coletas: ");
  display.println(contadorTampinhas);

  display.setCursor(0, 44);
  display.print("Peso final: ");
  display.print(pesoFinal);
  display.println(" g");

  display.display();
}

void mostrarErro(const String& mensagem) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(34, 3);
  display.println("ERRO");
  display.setTextSize(1);
  display.setCursor(2, 32);
  display.println(mensagem);
  display.display();
}

// ============================================================
// WIFI / SUPABASE
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
  } else {
    Serial.println("Falha no Wi-Fi.");
  }
}

String restUrl(const String& tabela) {
  String base = SUPABASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/rest/v1/" + tabela;
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

String chamarRPC(const char* funcao, const String& body) {
  HTTPClient http;

  if (!http.begin(rpcUrl(funcao))) {
    Serial.println("Falha ao iniciar HTTP RPC.");
    return "";
  }

  headers(http);

  int codigo = http.POST(body);
  String resposta = http.getString();
  http.end();

  Serial.print("RPC ");
  Serial.print(funcao);
  Serial.print(" HTTP: ");
  Serial.println(codigo);

  if (codigo < 200 || codigo >= 300) {
    Serial.print("Resposta erro: ");
    Serial.println(resposta);
    return "";
  }

  return resposta;
}

// ============================================================
// CRIAR MAQUINA
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

  int codigo = http.POST(body);
  String resposta = http.getString();
  http.end();

  Serial.print("machines HTTP: ");
  Serial.println(codigo);
  Serial.print("machines resposta: ");
  Serial.println(resposta);

  if (codigo < 200 || codigo >= 300) return false;

  JsonDocument doc;
  if (deserializeJson(doc, resposta)) return false;

  JsonObject row;
  if (doc.is<JsonArray>()) {
    JsonArray arr = doc.as<JsonArray>();
    if (!arr.size()) return false;
    row = arr[0].as<JsonObject>();
  } else {
    row = doc.as<JsonObject>();
  }

  machineId = row["id"] | "";
  deviceToken = row["device_token"] | "";

  if (!machineId.length() || !deviceToken.length()) return false;

  qrPayload = String("{\"machine_id\":\"") + machineId +
              "\",\"event_id\":\"" + EVENT_ID + "\"}";

  Serial.println();
  Serial.println("========================================");
  Serial.println("MAQUINA CRIADA");
  Serial.print("MACHINE_ID: "); Serial.println(machineId);
  Serial.print("DEVICE_TOKEN: "); Serial.println(deviceToken);
  Serial.print("EVENT_ID: "); Serial.println(EVENT_ID);
  Serial.println("========================================");

  return true;
}

// ============================================================
// QR NO CELULAR
// ============================================================

String makeQrSvg() {
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

  esp_err_t result = esp_qrcode_generate(&config, qrPayload.c_str());
  if (result != ESP_OK || capture.size <= 0) {
    return "<p>Erro ao gerar QR.</p>";
  }

  const int qz = 4;
  int full = capture.size + qz * 2;

  String svg;
  svg.reserve(full * full / 2);
  svg += "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 ";
  svg += String(full) + " " + String(full);
  svg += "' shape-rendering='crispEdges' class='qr'>";
  svg += "<rect width='100%' height='100%' fill='white'/>";

  for (int y = 0; y < capture.size; y++) {
    for (int x = 0; x < capture.size; x++) {
      if (capture.modules[y][x]) {
        svg += "<rect x='" + String(x + qz) +
               "' y='" + String(y + qz) +
               "' width='1' height='1' fill='black'/>";
      }
    }
  }

  svg += "</svg>";
  return svg;
}

void handleRoot() {
  String svg = makeQrSvg();

  String page;
  page.reserve(svg.length() + 2500);

  page += "<!doctype html><html lang='pt-BR'><head>";
  page += "<meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>TampAê - Teste</title><style>";
  page += "*{box-sizing:border-box}body{margin:0;min-height:100vh;background:#111;display:flex;align-items:center;justify-content:center;font-family:Arial;color:#fff;padding:8px}.card{width:100%;max-width:720px;text-align:center}.qr{display:block;width:min(96vw,680px);height:auto;aspect-ratio:1;margin:auto;background:#fff;border:8px solid #fff}.title{font-size:20px;font-weight:bold;margin:8px}.hint{font-size:13px}.id{font-size:10px;opacity:.65;word-break:break-all;margin-top:5px}</style></head><body><main class='card'>";
  page += svg;
  page += "<div class='title'>TAMPAÊ - TESTE</div>";
  page += "<div class='hint'>Escaneie para iniciar a sessão</div>";
  page += "<div class='id'>" + machineId + "</div>";
  page += "</main></body></html>";

  server.send(200, "text/html; charset=utf-8", page);
}

void handleInfo() {
  String json = "{\"machine_id\":\"" + machineId +
                "\",\"event_id\":\"" + EVENT_ID +
                "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  server.send(200, "application/json", json);
}

void iniciarServidor() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/info", HTTP_GET, handleInfo);
  server.begin();

  Serial.println();
  Serial.println("========================================");
  Serial.println("SERVIDOR WEB INICIADO");
  Serial.print("URL: http://");
  Serial.println(WiFi.localIP());
  Serial.println("QR DISPONIVEL");
  Serial.println("========================================");
}

// ============================================================
// SESSAO
// ============================================================

bool consultarSessao() {
  if (WiFi.status() != WL_CONNECTED || !machineId.length() || !deviceToken.length()) {
    return false;
  }

  JsonDocument req;
  req["p_machine_id"] = machineId;
  req["p_device_token"] = deviceToken;

  String body;
  serializeJson(req, body);

  String resposta = chamarRPC("get_active_session", body);

  if (!resposta.length()) return false;

  if (resposta == "[]") {
    if (!sessaoAtiva) mostrarAguardando();
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, resposta)) {
    Serial.println("Erro JSON na sessao.");
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (arr.isNull() || !arr.size()) return false;

  JsonObject sessao = arr[0].as<JsonObject>();

  String novaSessionId = sessao["session_id"] | "";
  String novoUserId = sessao["user_id"] | "";
  String novoNome = sessao["nome"] | "";
  String novoEvento = sessao["evento_id"] | "";

  Serial.println();
  Serial.println("========================================");
  Serial.println("SESSAO ENCONTRADA");
  Serial.print("Session ID: "); Serial.println(novaSessionId);
  Serial.print("Nome: "); Serial.println(novoNome);
  Serial.print("Evento: "); Serial.println(novoEvento);
  Serial.println("========================================");

  if (novoEvento != EVENT_ID) {
    Serial.println("Sessao ignorada: evento diferente.");
    return false;
  }

  if (!novaSessionId.length()) return false;

  if (novaSessionId != ultimaSessionId) {
    sessionId = novaSessionId;
    userId = novoUserId;
    nomeUsuario = novoNome;
    eventoSessao = novoEvento;
    ultimaSessionId = novaSessionId;

    contadorTampinhas = 0;
    pesoAtual = 0;
    pesoFinal = 0;
    sensorBloqueado = false;
    sessaoAtiva = true;
    sessaoFinalizada = false;

    Serial.println("USUARIO CONECTADO");
    Serial.print("NOME: "); Serial.println(nomeUsuario);
    Serial.print("EVENTO_ID: "); Serial.println(eventoSessao);

    mostrarSessao();
  }

  return true;
}

// ============================================================
// BALANCA SIMULADA
// ============================================================

int lerPeso() {
  int valor = analogRead(POT_PIN);
  return constrain(map(valor, 0, 4095, 0, 1000), 0, 1000);
}

// ============================================================
// REGISTRAR COLETA REAL
// ============================================================

bool registrarColeta() {
  if (!sessaoAtiva || !sessionId.length()) return false;

  int peso = lerPeso();

  JsonDocument req;
  req["p_machine_id"] = machineId;
  req["p_device_token"] = deviceToken;
  req["p_session_id"] = sessionId;
  req["p_tipo_coleta"] = "unitaria";
  req["p_quantidade_real"] = 1;
  req["p_quantidade_estimada"] = nullptr;
  req["p_peso_real_gramas"] = nullptr;
  req["p_peso_estimado_gramas"] = peso;

  String body;
  serializeJson(req, body);

  Serial.println();
  Serial.println("----------------------------------------");
  Serial.println("TAMPINHA DETECTADA");
  Serial.print("LDR: "); Serial.println(analogRead(LDR_PIN));
  Serial.print("Peso simulado: "); Serial.print(peso); Serial.println(" g");
  Serial.println("Enviando registrar_coleta...");
  Serial.println(body);

  String resposta = chamarRPC("registrar_coleta", body);

  if (!resposta.length()) {
    Serial.println("FALHA: coleta nao registrada.");
    return false;
  }

  Serial.print("COLETA REGISTRADA: ");
  Serial.println(resposta);
  Serial.println("Pontuacao deve ser atualizada pelo backend.");
  Serial.println("----------------------------------------");

  return true;
}

// ============================================================
// LDR
// ============================================================

void verificarLDR() {
  int valor = analogRead(LDR_PIN);

  if (valor < LIMIAR_LDR) {
    if (!sensorBloqueado) {
      sensorBloqueado = true;

      if (registrarColeta()) {
        contadorTampinhas++;
        pesoAtual = lerPeso();

        Serial.print("CONTADOR: ");
        Serial.println(contadorTampinhas);
      }
    }
  } else {
    sensorBloqueado = false;
  }
}

// ============================================================
// BOTAO FINALIZAR
// ============================================================

void finalizarSessaoLocal() {
  if (!sessaoAtiva) return;

  pesoFinal = lerPeso();
  sessaoAtiva = false;
  sessaoFinalizada = true;

  mostrarFinalizacao();

  Serial.println();
  Serial.println("========================================");
  Serial.println("TESTE DE SESSAO FINALIZADO");
  Serial.print("Usuario: "); Serial.println(nomeUsuario);
  Serial.print("Session ID: "); Serial.println(sessionId);
  Serial.print("Coletas registradas: "); Serial.println(contadorTampinhas);
  Serial.print("Peso final: "); Serial.print(pesoFinal); Serial.println(" g");
  Serial.println("========================================");

  // Neste teste, nao inventamos uma RPC de encerramento que nao foi
  // encontrada no projeto. O botao finaliza o estado do firmware.
  // As coletas ja foram gravadas pelo registrar_coleta.
}

void verificarBotao() {
  if (digitalRead(BOTAO_FINALIZAR) != LOW) return;

  if (millis() - ultimoDebounceBotao < 500) return;
  ultimoDebounceBotao = millis();

  delay(40);
  if (digitalRead(BOTAO_FINALIZAR) != LOW) return;

  finalizarSessaoLocal();

  while (digitalRead(BOTAO_FINALIZAR) == LOW) delay(10);

  delay(3000);

  sessaoFinalizada = false;
  sessionId = "";
  userId = "";
  nomeUsuario = "";
  eventoSessao = "";
  ultimaSessionId = "";
  contadorTampinhas = 0;
  pesoFinal = 0;
  pesoAtual = 0;
  sensorBloqueado = false;

  mostrarAguardando();
  Serial.println("Aguardando novo usuario.");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LDR_PIN, INPUT);
  pinMode(POT_PIN, INPUT);
  pinMode(BOTAO_FINALIZAR, INPUT_PULLUP);

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERRO: OLED nao encontrado.");
    while (true) delay(1000);
  }

  oledMessage("TAMPAE", "Teste pontuacao", "Iniciando...");

  conectarWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    mostrarErro("Sem WiFi");
    return;
  }

  oledMessage("TAMPAE", "Criando maquina...");

  if (!criarMaquina()) {
    mostrarErro("Erro machine");
    return;
  }

  iniciarServidor();
  mostrarAguardando();

  Serial.println("Teste pronto.");
  Serial.print("Abra no celular: http://");
  Serial.println(WiFi.localIP());
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
    delay(500);
    return;
  }

  if (!sessaoAtiva && !sessaoFinalizada) {
    if (millis() - ultimaConsultaSessao >= INTERVALO_SESSAO) {
      ultimaConsultaSessao = millis();
      consultarSessao();
    }

    delay(10);
    return;
  }

  if (sessaoAtiva && !sessaoFinalizada) {
    verificarLDR();
    verificarBotao();

    pesoAtual = lerPeso();

    if (millis() - ultimaTela >= INTERVALO_TELA) {
      ultimaTela = millis();
      mostrarSessao();
    }
  }

  delay(5);
}
