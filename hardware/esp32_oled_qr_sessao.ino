// ============================================================
// TAMPAÊ - ESP32 / OLED / QR CODE / SESSÃO
// Este arquivo controla a comunicação da máquina com o Supabase,
// o display OLED e o servidor web local que disponibiliza o QR Code.
// ============================================================

// Biblioteca de conexão Wi-Fi do ESP32.
#include <WiFi.h>
// Permite criar um servidor HTTP diretamente no ESP32.
#include <WebServer.h>
// Permite fazer requisições HTTP para o Supabase.
#include <HTTPClient.h>
// Permite montar e interpretar objetos JSON.
#include <ArduinoJson.h>
// Biblioteca de comunicação I2C, usada pelo OLED.
#include <Wire.h>
// Biblioteca gráfica base usada pelo driver do OLED.
#include <Adafruit_GFX.h>
// Driver do display OLED SSD1306.
#include <Adafruit_SSD1306.h>
// Biblioteca usada para gerar o QR Code.
#include <qrcode.h>

// Largura física do display OLED em pixels.
#define SCREEN_WIDTH 128
// Altura física do display OLED em pixels.
#define SCREEN_HEIGHT 64
// O display não utiliza um pino físico de reset separado.
#define OLED_RESET -1
// Endereço I2C padrão utilizado pelo OLED.
#define OLED_ADDRESS 0x3C
// Pino SDA da comunicação I2C.
#define I2C_SDA 21
// Pino SCL da comunicação I2C.
#define I2C_SCL 22

// Cria o objeto que representa o display OLED.
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// Cria o servidor web que ficará escutando na porta 80.
WebServer server(80);

// Nome da rede Wi-Fi usada pelo ESP32.
const char* WIFI_SSID = "DAYANE";
// Senha da rede Wi-Fi usada pelo ESP32.
const char* WIFI_PASSWORD = "@Felipe23";
// URL base do projeto Supabase.
const char* SUPABASE_URL = "https://usztjfxtbagjbnupiuxm.supabase.co";
// Chave pública usada nas requisições ao Supabase.
const char* SUPABASE_ANON_KEY = "sb_publishable_SZhZ1FlWxXzd85fWRQ69Fg_j9HXqqnr";
// ID do evento utilizado pela máquina nesta versão do firmware.
const char* EVENT_ID = "a142ed17-e276-47e0-918f-8d1145b558c0";
// Nome que será enviado ao banco quando a máquina for criada.
const char* MACHINE_NAME = "TampAê - ESP32";

// ID da máquina criada no Supabase.
String machineId = "";
// Token do dispositivo retornado pelo Supabase.
String deviceToken = "";
// Conteúdo JSON que será usado como payload do QR Code.
String qrPayload = "";
// Guarda a última sessão já identificada pelo ESP32.
String lastSessionId = "";
// Guarda o instante da última consulta de sessão.
unsigned long lastPoll = 0;
// Intervalo entre consultas de sessão, em milissegundos.
const unsigned long POLL_INTERVAL = 1500;

// Mostra até três linhas de texto no OLED.
// Os parâmetros b e c são opcionais e permitem adicionar uma segunda e terceira linha.
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

// Substitui caracteres especiais por entidades HTML para evitar problemas
// quando um texto for inserido diretamente na página HTML gerada pelo ESP32.
String htmlEscape(const String& s) {
  String out = s;
  out.replace("&", "&amp;");
  out.replace("<", "&lt;");
  out.replace(">", "&gt;");
  out.replace("\"", "&quot;");
  return out;
}

// Gera o QR Code e transforma seus módulos em um SVG.
// O SVG é enviado posteriormente para a página web hospedada pelo ESP32.
String makeQrSvg(const String& payload) {
  // Estrutura temporária que armazena a matriz de módulos do QR Code.
  struct Capture {
    int size;
    bool modules[177][177];
  };
  static Capture capture;
  capture.size = 0;

  // Configuração usada pela biblioteca de QR Code.
  esp_qrcode_config_t config = {};
  config.max_qrcode_version = 10;
  config.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
  // Função chamada pela biblioteca para entregar cada módulo do QR.
  config.display_func = [](esp_qrcode_handle_t qr) {
    capture.size = esp_qrcode_get_size(qr);
    for (int y = 0; y < capture.size; y++) {
      for (int x = 0; x < capture.size; x++) {
        capture.modules[y][x] = esp_qrcode_get_module(qr, x, y);
      }
    }
  };

  // Gera o QR a partir do payload informado.
  esp_err_t result = esp_qrcode_generate(&config, payload.c_str());
  // Se a geração falhar, retorna uma mensagem HTML de erro.
  if (result != ESP_OK || capture.size <= 0) return "<p>Erro ao gerar QR Code.</p>";

  // Margem branca ao redor do QR Code.
  const int qz = 4;
  int full = capture.size + qz * 2;
  // String que armazenará o SVG completo.
  String svg;
  svg.reserve(full * full / 2);
  svg += "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 ";
  svg += String(full) + " " + String(full) + "' shape-rendering='crispEdges' class='qr'>";
  svg += "<rect width='100%' height='100%' fill='white'/>";
  // Percorre todos os módulos do QR Code.
  for (int y = 0; y < capture.size; y++) {
    for (int x = 0; x < capture.size; x++) {
      // Para cada módulo ativo, cria um quadrado preto no SVG.
      if (capture.modules[y][x]) {
        svg += "<rect x='" + String(x + qz) + "' y='" + String(y + qz) + "' width='1' height='1' fill='black'/>";
      }
    }
  }
  svg += "</svg>";
  return svg;
}

// Trata o acesso à página principal do servidor web do ESP32.
// A página mostra o QR Code que contém os dados da máquina/evento.
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
  // Envia a página HTML para quem acessar a raiz do servidor.
  server.send(200, "text/html; charset=utf-8", page);
}

// Retorna informações básicas da máquina em formato JSON.
void handleInfo() {
  String json = "{\"machine_id\":\"" + machineId + "\",\"event_id\":\"" + String(EVENT_ID) + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  server.send(200, "application/json", json);
}

// Registra as rotas HTTP e inicia o servidor web do ESP32.
void startWebServer() {
  // Página principal, acessível em http://IP_DO_ESP32/
  server.on("/", HTTP_GET, handleRoot);
  // Endpoint que retorna informações da máquina em JSON.
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

// Tenta conectar o ESP32 à rede Wi-Fi configurada.
bool connectWiFi() {
  // Se já estiver conectado, não faz uma nova conexão.
  if (WiFi.status() == WL_CONNECTED) return true;
  oledMessage("TAMPAE", "Conectando Wi-Fi...");
  // Coloca o ESP32 no modo estação.
  WiFi.mode(WIFI_STA);
  // Inicia a conexão com SSID e senha definidos acima.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  // Aguarda até 15 segundos pela conexão.
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

// Monta a URL da API REST do Supabase para uma determinada tabela.
String restUrl(const String& table) {
  String base = SUPABASE_URL;
  // Remove barras finais para evitar URLs com // no caminho.
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/rest/v1/" + table;
}

// Monta a URL da API RPC do Supabase para uma determinada função.
String rpcUrl(const String& rpc) {
  String base = SUPABASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/rest/v1/rpc/" + rpc;
}

// Adiciona os cabeçalhos necessários às requisições HTTP para o Supabase.
void headers(HTTPClient& http) {
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
}

// Cria o registro da máquina na tabela machines do Supabase.
bool createMachine() {
  HTTPClient http;
  // Abre uma conexão HTTP com a tabela machines.
  if (!http.begin(restUrl("machines"))) return false;
  headers(http);
  // Solicita que a API devolva o registro criado.
  http.addHeader("Prefer", "return=representation");

  // Monta o JSON que será enviado ao banco.
  JsonDocument req;
  req["nome"] = MACHINE_NAME;
  req["latitude"] = 0.0;
  req["longitude"] = 0.0;
  req["status"] = "ativa";
  String body;
  serializeJson(req, body);

  // Faz a requisição POST para criar a máquina.
  int code = http.POST(body);
  String response = http.getString();
  http.end();

  // Mostra no Serial o resultado da operação.
  Serial.print("machines HTTP: ");
  Serial.println(code);
  Serial.print("machines resposta: ");
  Serial.println(response);
  if (code < 200 || code >= 300) return false;

  // Interpreta a resposta JSON enviada pelo Supabase.
  JsonDocument doc;
  if (deserializeJson(doc, response)) return false;
  JsonObject row;
  // A API normalmente retorna um array contendo o registro criado.
  if (doc.is<JsonArray>()) {
    JsonArray a = doc.as<JsonArray>();
    if (!a.size()) return false;
    row = a[0].as<JsonObject>();
  } else row = doc.as<JsonObject>();

  // Guarda no ESP32 o ID e o token retornados pelo banco.
  machineId = row["id"] | "";
  deviceToken = row["device_token"] | "";
  if (!machineId.length() || !deviceToken.length()) return false;

  // Monta o conteúdo que será colocado no QR Code.
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

// Consulta no Supabase se existe uma sessão ativa para esta máquina.
void checkSession() {
  // Sem Wi-Fi, machineId ou deviceToken, não é possível consultar a sessão.
  if (WiFi.status() != WL_CONNECTED || !machineId.length() || !deviceToken.length()) return;
  HTTPClient http;
  // Chama a função RPC get_active_session do Supabase.
  if (!http.begin(rpcUrl("get_active_session"))) return;
  headers(http);

  // Monta os parâmetros esperados pela função RPC.
  JsonDocument req;
  req["p_machine_id"] = machineId;
  req["p_device_token"] = deviceToken;
  String body;
  serializeJson(req, body);

  // Executa a chamada RPC.
  int code = http.POST(body);
  String response = http.getString();
  http.end();

  // Se o Supabase devolver erro HTTP, mostra a resposta no Serial.
  if (code < 200 || code >= 300) {
    Serial.print("RPC HTTP: "); Serial.println(code);
    Serial.println(response);
    return;
  }

  // Interpreta a resposta JSON da função RPC.
  JsonDocument doc;
  if (deserializeJson(doc, response)) return;
  JsonVariant row;
  // A resposta pode vir como array ou objeto.
  if (doc.is<JsonArray>()) {
    JsonArray a = doc.as<JsonArray>();
    if (!a.size()) return;
    row = a[0];
  } else if (doc.is<JsonObject>()) row = doc.as<JsonObject>();
  else return;

  // Obtém o ID da sessão retornado pelo banco.
  String sessionId = row["session_id"] | "";
  // Ignora respostas sem sessão ou a mesma sessão já processada.
  if (!sessionId.length() || sessionId == lastSessionId) return;
  lastSessionId = sessionId;

  // Obtém o nome do usuário e o evento associado à sessão.
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

// Executado uma vez quando o ESP32 é ligado ou reiniciado.
void setup() {
  // Inicializa a comunicação Serial para depuração.
  Serial.begin(115200);
  delay(300);
  // Inicializa o barramento I2C nos pinos definidos anteriormente.
  Wire.begin(I2C_SDA, I2C_SCL);
  // Inicializa o display OLED.
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERRO: OLED nao encontrado.");
    // Para a execução caso o OLED não seja encontrado.
    while (true) delay(1000);
  }

  oledMessage("TAMPAE", "Iniciando...");
  // Primeiro conecta o ESP32 à internet.
  if (!connectWiFi()) return;

  oledMessage("TAMPAE", "Criando maquina...");
  // Depois cria/obtém o registro da máquina no Supabase.
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

// Executado continuamente depois do setup().
void loop() {
  // Processa as requisições recebidas pelo servidor web.
  server.handleClient();
  // Se o Wi-Fi cair, tenta reconectar antes de continuar.
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    delay(1000);
    return;
  }
  // Consulta a sessão somente quando o intervalo configurado tiver passado.
  if (millis() - lastPoll >= POLL_INTERVAL) {
    lastPoll = millis();
    checkSession();
  }
  // Pequeno atraso para evitar ocupar o processador continuamente.
  delay(2);
}
