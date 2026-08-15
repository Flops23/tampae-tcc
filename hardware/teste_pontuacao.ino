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
// TAMPAE - TESTE DE PONTUACAO
// ============================================================
// Existem DOIS modos independentes de deposito:
//
// 1) PASSAGEM / LDR:
//    Cada passagem detectada pelo LDR = EXATAMENTE 1 tampinha.
//    A balanca NAO altera essa contagem.
//
// 2) BALANCA:
//    Quando houver peso na balanca sem uma passagem pelo LDR,
//    a quantidade e calculada por 13 g = 1 tampinha.
//    Exemplos: 13 g = 1, 26 g = 2, 27 g = 3, 40 g = 4.
//
// Durante a sessao as coletas ficam acumuladas localmente.
// Ao apertar o BOTAO, o ESP32:
//    1. envia UMA registrar_coleta;
//    2. chama encerrar_sessao_maquina explicitamente;
//    3. limpa a sessao local e volta a aguardar outro usuario.
//
// A chamada de encerramento e mantida mesmo que registrar_coleta
// ja feche a sessao, porque assim garantimos a sincronizacao com
// o aplicativo quando o contrato do banco mudar.
// ============================================================

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDRESS 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define LDR_PIN 34
#define BALANCA_PIN 35
#define BOTAO_PIN 23
#define LDR_LIMITE 1500

const float GRAMAS_POR_TAMPA = 13.0f;

// Peso minimo para considerar que existe um deposito na balanca.
const float BALANCA_MIN_GRAMAS = 5.0f;

// A leitura precisa permanecer acima do minimo por este tempo
// para evitar contar ruido do ADC como uma coleta.
const unsigned long BALANCA_ESTABILIDADE_MS = 1200;

// Depois de registrar uma carga na balanca, ela precisa voltar
// praticamente a zero antes que outra carga possa ser registrada.
const float BALANCA_RESET_GRAMAS = 3.0f;

const unsigned long INTERVALO_SESSAO = 1500;
const unsigned long DEBOUNCE_LDR = 500;
const unsigned long DEBOUNCE_BOTAO = 500;

const char* WIFI_SSID = "DAYANE";
const char* WIFI_PASSWORD = "@Felipe23";
const char* SUPABASE_URL = "https://usztjfxtbagjbnupiuxm.supabase.co";
const char* SUPABASE_ANON_KEY = "sb_publishable_SZhZ1FlWxXzd85fWRQ69Fg_j9HXqqnr";
const char* EVENT_ID = "a142ed17-e276-47e0-918f-8d1145b558c0";
const char* MACHINE_NAME = "TampAê - Teste Pontuação";

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
bool envioFinalEmAndamento = false;
bool ldrBloqueado = false;
bool botaoAnterior = HIGH;

// Controle do modo balanca.
bool balancaCargaRegistrada = false;
unsigned long inicioPesoEstavel = 0;

unsigned long totalPeso = 0;
unsigned long totalTampinhas = 0;
unsigned long totalPassagens = 0;
unsigned long ultimaConsulta = 0;
unsigned long ultimoLdr = 0;
unsigned long ultimoBotao = 0;

void oledMessage(const String& a, const String& b="", const String& c="", const String& d="") {
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
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(24, 2);
  display.println("TAMPAE");
  display.setTextSize(1);
  display.setCursor(15, 30);
  display.println("Aguardando usuario");
  display.setCursor(27, 46);
  display.println("Leia o QR");
  display.display();
}

void mostrarSessao() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Usuario: ");
  display.println(nomeUsuario.length() ? nomeUsuario : "Conectado");
  display.setCursor(0, 14);
  display.print("Tampinhas: ");
  display.println(totalTampinhas);
  display.setCursor(0, 28);
  display.print("Peso total: ");
  display.print(totalPeso);
  display.println("g");
  display.setCursor(0, 42);
  display.print("Passagens: ");
  display.println(totalPassagens);
  display.setCursor(0, 56);
  display.println("Botao = finalizar");
  display.display();
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

bool conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

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
    return true;
  }

  Serial.println("Falha Wi-Fi.");
  return false;
}

String chamarRPC(const char* nome, const String& body, int* codigoOut = nullptr) {
  HTTPClient http;
  if (!http.begin(rpcUrl(nome))) return "";

  headers(http);
  int codigo = http.POST(body);
  String resposta = http.getString();
  http.end();

  if (codigoOut) *codigoOut = codigo;

  Serial.print("RPC ");
  Serial.print(nome);
  Serial.print(" HTTP: ");
  Serial.println(codigo);

  if (codigo < 200 || codigo >= 300) {
    Serial.print("RPC resposta: ");
    Serial.println(resposta);
  }

  return resposta;
}

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
    JsonArray a = doc.as<JsonArray>();
    if (!a.size()) return false;
    row = a[0].as<JsonObject>();
  } else {
    row = doc.as<JsonObject>();
  }

  machineId = row["id"] | "";
  deviceToken = row["device_token"] | "";

  if (!machineId.length() || !deviceToken.length()) return false;

  qrPayload = String("{\"machine_id\":\"") + machineId +
              "\",\"event_id\":\"" + EVENT_ID + "\"}";

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

  if (esp_qrcode_generate(&config, qrPayload.c_str()) != ESP_OK || capture.size <= 0) {
    return "<p>Erro ao gerar QR.</p>";
  }

  const int margem = 4;
  int full = capture.size + margem * 2;
  String svg;
  svg.reserve(full * full / 2);
  svg += "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 ";
  svg += String(full) + " " + String(full) + "' shape-rendering='crispEdges' class='qr'>";
  svg += "<rect width='100%' height='100%' fill='white'/>";

  for (int y = 0; y < capture.size; y++) {
    for (int x = 0; x < capture.size; x++) {
      if (capture.modules[y][x]) {
        svg += "<rect x='" + String(x + margem) +
               "' y='" + String(y + margem) +
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
  page.reserve(svg.length() + 2200);

  page += "<!doctype html><html lang='pt-BR'><head><meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>TampAê - Teste</title><style>";
  page += "*{box-sizing:border-box}body{margin:0;min-height:100vh;background:#111;display:flex;align-items:center;justify-content:center;font-family:Arial;color:#fff;padding:8px}.card{width:100%;max-width:720px;text-align:center}.qr{display:block;width:min(96vw,680px);height:auto;aspect-ratio:1;margin:auto;background:#fff;border:8px solid #fff}.title{font-size:20px;font-weight:bold;margin:8px}.hint{font-size:13px}.id{font-size:10px;opacity:.65;word-break:break-all;margin-top:5px}</style></head><body><main class='card'>";
  page += svg;
  page += "<div class='title'>TAMPAÊ - TESTE</div><div class='hint'>Escaneie para iniciar a sessão</div>";
  page += "<div class='id'>" + machineId + "</div></main></body></html>";

  server.send(200, "text/html; charset=utf-8", page);
}

void handleInfo() {
  String json = String("{\"machine_id\":\"") + machineId +
                "\",\"event_id\":\"" + EVENT_ID +
                "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  server.send(200, "application/json", json);
}

void iniciarServidor() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/info", HTTP_GET, handleInfo);
  server.begin();

  Serial.println("========================================");
  Serial.println("SERVIDOR WEB INICIADO");
  Serial.print("URL: http://");
  Serial.println(WiFi.localIP());
  Serial.println("========================================");
}

void zerarAcumulador() {
  totalPeso = 0;
  totalTampinhas = 0;
  totalPassagens = 0;
  ldrBloqueado = false;
  balancaCargaRegistrada = false;
  inicioPesoEstavel = 0;
}

bool lerSessaoAtiva(String& novaId, String& novoUser, String& novoNome, String& novoEvento) {
  novaId = "";
  novoUser = "";
  novoNome = "";
  novoEvento = "";

  JsonDocument req;
  req["p_machine_id"] = machineId;
  req["p_device_token"] = deviceToken;

  String body;
  serializeJson(req, body);

  int codigo = 0;
  String resposta = chamarRPC("get_active_session", body, &codigo);
  if (codigo < 200 || codigo >= 300 || !resposta.length()) return false;

  JsonDocument doc;
  if (deserializeJson(doc, resposta)) return false;
  if (!doc.is<JsonArray>()) return false;

  JsonArray a = doc.as<JsonArray>();
  if (!a.size()) return false;

  JsonObject s = a[0].as<JsonObject>();
  novaId = s["session_id"] | "";
  novoUser = s["user_id"] | "";
  novoNome = s["nome"] | "";
  novoEvento = s["evento_id"] | "";

  return novaId.length() > 0;
}

void iniciarNovaSessao(const String& id, const String& uid, const String& nome, const String& evento) {
  sessionId = id;
  userId = uid;
  nomeUsuario = nome;
  eventoSessao = evento;

  sessaoAtiva = true;
  envioFinalEmAndamento = false;
  zerarAcumulador();

  Serial.println();
  Serial.println("========================================");
  Serial.println("USUARIO CONECTADO");
  Serial.print("NOME: ");
  Serial.println(nomeUsuario.length() ? nomeUsuario : userId);
  Serial.print("EVENTO_ID: ");
  Serial.println(eventoSessao.length() ? eventoSessao : "NULL");
  Serial.print("SESSION_ID: ");
  Serial.println(sessionId);
  Serial.println("Modo LDR: cada passagem = 1 tampinha.");
  Serial.println("Modo balanca: 13 g = 1 tampinha.");
  Serial.println("Coletas serao acumuladas localmente.");
  Serial.println("========================================");

  mostrarSessao();
}

float lerPeso() {
  int adc = analogRead(BALANCA_PIN);
  return (adc / 4095.0f) * 1000.0f;
}

void registrarPassagemLdr() {
  totalTampinhas += 1;
  totalPassagens += 1;

  Serial.println();
  Serial.println("TAMPINHA DETECTADA PELO LDR");
  Serial.print("LDR: ");
  Serial.println(analogRead(LDR_PIN));
  Serial.println("Quantidade desta passagem: 1 tampinha");
  Serial.print("TOTAL TAMPINHAS: ");
  Serial.println(totalTampinhas);
  Serial.println("Modo balanca NAO foi usado nesta passagem.");
  Serial.println("NAO enviado ao banco.");

  mostrarSessao();
}

void detectarLdr() {
  int ldr = analogRead(LDR_PIN);
  bool bloqueado = (ldr < LDR_LIMITE);

  if (bloqueado && !ldrBloqueado && millis() - ultimoLdr >= DEBOUNCE_LDR) {
    ldrBloqueado = true;
    ultimoLdr = millis();

    // No modo de passagem, a regra e sempre 1 passagem = 1 tampinha.
    // O peso da balanca nao entra nesta conta.
    registrarPassagemLdr();
  }

  if (!bloqueado && ldrBloqueado) {
    ldrBloqueado = false;
  }
}

void detectarBalanca() {
  float peso = lerPeso();

  // Se o usuario estiver usando o LDR, nao tentamos criar uma segunda
  // coleta pela balanca no mesmo instante.
  if (ldrBloqueado) {
    inicioPesoEstavel = 0;
    return;
  }

  // A carga foi retirada. Libera a balanca para o proximo deposito.
  if (peso <= BALANCA_RESET_GRAMAS) {
    balancaCargaRegistrada = false;
    inicioPesoEstavel = 0;
    return;
  }

  // Ja registramos esta carga; aguardamos ela voltar a zero.
  if (balancaCargaRegistrada) return;

  if (peso < BALANCA_MIN_GRAMAS) {
    inicioPesoEstavel = 0;
    return;
  }

  // Comeca a janela de estabilidade.
  if (inicioPesoEstavel == 0) {
    inicioPesoEstavel = millis();
    return;
  }

  if (millis() - inicioPesoEstavel < BALANCA_ESTABILIDADE_MS) return;

  unsigned long quantidade = (unsigned long)ceil(peso / GRAMAS_POR_TAMPA);
  if (quantidade == 0) return;

  totalPeso += (unsigned long)round(peso);
  totalTampinhas += quantidade;
  totalPassagens += 1;
  balancaCargaRegistrada = true;
  inicioPesoEstavel = 0;

  Serial.println();
  Serial.println("DEPOSITO DETECTADO PELA BALANCA");
  Serial.print("Peso desta carga: ");
  Serial.print(peso, 1);
  Serial.println(" g");
  Serial.print("Regra: ");
  Serial.print(quantidade);
  Serial.println(" tampinha(s) = ceil(peso / 13)");
  Serial.print("TOTAL TAMPINHAS: ");
  Serial.println(totalTampinhas);
  Serial.print("TOTAL PESO: ");
  Serial.print(totalPeso);
  Serial.println(" g");
  Serial.println("Modo LDR nao foi usado nesta carga.");
  Serial.println("NAO enviado ao banco.");

  mostrarSessao();
}

void detectarPassagem() {
  if (!sessaoAtiva || envioFinalEmAndamento) return;

  // Os dois metodos podem ser usados na mesma sessao.
  // LDR: uma passagem = uma tampinha.
  // Balanca: uma carga = ceil(peso / 13) tampinhas.
  detectarLdr();
  detectarBalanca();
}

bool encerrarSessaoNaMaquina() {
  if (!machineId.length() || !deviceToken.length() || !sessionId.length()) return false;

  JsonDocument req;
  req["p_machine_id"] = machineId;
  req["p_device_token"] = deviceToken;
  req["p_session_id"] = sessionId;

  String body;
  serializeJson(req, body);

  // Tenta mais de uma vez porque a prioridade e garantir que o aplicativo
  // veja status=concluida mesmo se houver uma falha momentanea de rede.
  for (int tentativa = 1; tentativa <= 3; tentativa++) {
    int codigo = 0;
    String resposta = chamarRPC("encerrar_sessao_maquina", body, &codigo);

    Serial.print("ENCERRAMENTO DA SESSAO - tentativa ");
    Serial.print(tentativa);
    Serial.print(" HTTP: ");
    Serial.println(codigo);

    if (codigo >= 200 && codigo < 300) {
      Serial.print("Resposta encerrar_sessao_maquina: ");
      Serial.println(resposta);
      return true;
    }

    delay(300);
  }

  return false;
}

bool enviarResultadoEFecharSessao() {
  if (!sessaoAtiva || envioFinalEmAndamento) return false;

  envioFinalEmAndamento = true;

  Serial.println();
  Serial.println("========================================");
  Serial.println("FINALIZANDO SESSAO PELO ESP32");
  Serial.println("Uma unica registrar_coleta sera enviada.");
  Serial.print("SESSION_ID: ");
  Serial.println(sessionId);
  Serial.print("TAMPINHAS: ");
  Serial.println(totalTampinhas);
  Serial.print("PESO TOTAL: ");
  Serial.print(totalPeso);
  Serial.println(" g");
  Serial.print("PASSAGENS/CARGAS: ");
  Serial.println(totalPassagens);

  // Contrato da RPC registrar_coleta:
  // p_machine_id, p_device_token, p_session_id, p_tipo_coleta,
  // p_quantidade_real, p_quantidade_estimada,
  // p_peso_real_gramas, p_peso_estimado_gramas.
  JsonDocument req;
  req["p_machine_id"] = machineId;
  req["p_device_token"] = deviceToken;
  req["p_session_id"] = sessionId;
  req["p_tipo_coleta"] = "unidade";
  req["p_quantidade_real"] = (int)totalTampinhas;
  req["p_quantidade_estimada"] = nullptr;
  req["p_peso_real_gramas"] = (double)totalPeso;
  req["p_peso_estimado_gramas"] = nullptr;

  String body;
  serializeJson(req, body);

  Serial.print("BODY FINAL: ");
  Serial.println(body);

  int codigo = 0;
  String resposta = chamarRPC("registrar_coleta", body, &codigo);

  if (codigo < 200 || codigo >= 300) {
    Serial.println("ERRO: a coleta final NAO foi gravada.");
    Serial.println("A sessao permanece ativa para tentar novamente.");
    envioFinalEmAndamento = false;
    return false;
  }

  Serial.println("COLETA FINAL ENVIADA COM SUCESSO");
  Serial.print("COLLECTION_ID/RESPOSTA: ");
  Serial.println(resposta);

  // Nao dependemos somente do registrar_coleta para fechar a sessao.
  // Esta chamada sincroniza explicitamente o status com o aplicativo.
  bool sessaoFechada = encerrarSessaoNaMaquina();

  if (!sessaoFechada) {
    Serial.println("ERRO: coleta gravada, mas nao foi possivel confirmar o encerramento da sessao.");
    Serial.println("A sessao local permanece ativa para nova tentativa.");
    envioFinalEmAndamento = false;
    return false;
  }

  Serial.println("========================================");
  Serial.println("SESSAO ENCERRADA COM SUCESSO");
  Serial.println("O aplicativo devera detectar status=concluida.");
  Serial.println("========================================");

  sessaoAtiva = false;
  envioFinalEmAndamento = false;
  sessionId = "";
  userId = "";
  nomeUsuario = "";
  eventoSessao = "";
  zerarAcumulador();
  mostrarAguardando();

  return true;
}

void verificarBotao() {
  bool atual = digitalRead(BOTAO_PIN);

  if (botaoAnterior == HIGH && atual == LOW && millis() - ultimoBotao >= DEBOUNCE_BOTAO) {
    ultimoBotao = millis();

    if (sessaoAtiva) {
      Serial.println("BOTAO: finalizar sessao solicitado.");
      enviarResultadoEFecharSessao();
    } else {
      Serial.println("BOTAO pressionado, mas nao existe sessao ativa.");
    }
  }

  botaoAnterior = atual;
}

void verificarSessao() {
  if (envioFinalEmAndamento) return;

  String novaId, novoUser, novoNome, novoEvento;
  bool encontrou = lerSessaoAtiva(novaId, novoUser, novoNome, novoEvento);

  if (encontrou) {
    if (!sessaoAtiva) {
      iniciarNovaSessao(novaId, novoUser, novoNome, novoEvento);
    } else if (sessionId != novaId) {
      // Nao troca de usuario no meio de uma coleta local.
      Serial.println("ATENCAO: outra sessao apareceu enquanto a atual esta ativa.");
      Serial.println("A sessao atual continua sendo usada ate o botao finalizar.");
    }
  } else if (!sessaoAtiva) {
    mostrarAguardando();
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(LDR_PIN, INPUT);
  pinMode(BALANCA_PIN, INPUT);
  pinMode(BOTAO_PIN, INPUT_PULLUP);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERRO: OLED nao encontrado.");
    while (true) delay(1000);
  }

  oledMessage("TAMPAE", "Teste pontuacao");

  if (!conectarWiFi()) {
    oledMessage("TAMPAE", "Falha Wi-Fi");
    return;
  }

  if (!criarMaquina()) {
    oledMessage("TAMPAE", "Erro maquina");
    return;
  }

  iniciarServidor();
  mostrarAguardando();

  Serial.println("========================================");
  Serial.println("PRONTO PARA TESTE");
  Serial.print("Abra no celular: http://");
  Serial.println(WiFi.localIP());
  Serial.println("Escaneie o QR para criar a sessao.");
  Serial.println();
  Serial.println("REGRAS DE CONTAGEM:");
  Serial.println("- LDR: cada passagem = 1 tampinha");
  Serial.println("- BALANCA: cada 13 g = 1 tampinha");
  Serial.println("- Os dois modos podem ser usados na mesma sessao");
  Serial.println("========================================");
}

void loop() {
  server.handleClient();
  verificarBotao();
  detectarPassagem();

  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
    delay(500);
    return;
  }

  if (millis() - ultimaConsulta >= INTERVALO_SESSAO) {
    ultimaConsulta = millis();
    verificarSessao();
  }

  delay(2);
}
