#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// TAMPAE - ESP32 + SUPABASE + LDR + BALANCA SIMULADA
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
#define PINO_LDR 35
#define PINO_POT 34
#define SDA_PIN 21
#define SCL_PIN 22

#define LIMIAR_LDR 300
#define DEBOUNCE_MS 500
#define PESO_MAXIMO_GRAMAS 500.0
#define TAMANHO_FILA 20

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String sessionId = "";
String userId = "";
String userName = "";
String eventId = "";
volatile bool sessaoAtiva = false;
int passagens = 0;
int pontos = 0;
float pesoGramas = 0;
int valorLDR = 0;

// Fila de tampinhas detectadas pelo sensor.
// O LDR continua sendo lido mesmo enquanto o ESP32 espera uma resposta HTTP.
float filaPesos[TAMANHO_FILA];
volatile int filaInicio = 0;
volatile int filaFim = 0;
volatile int filaQuantidade = 0;
portMUX_TYPE filaMux = portMUX_INITIALIZER_UNLOCKED;

unsigned long ultimaConsultaSessao = 0;
unsigned long ultimaAtualizacaoTela = 0;

void mostrarTela(const String& l1, const String& l2 = "", const String& l3 = "", const String& l4 = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0); display.println(l1);
  if (l2.length()) { display.setCursor(0, 16); display.println(l2); }
  if (l3.length()) { display.setCursor(0, 32); display.println(l3); }
  if (l4.length()) { display.setCursor(0, 48); display.println(l4); }
  display.display();
}

void mostrarOperacao() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0); display.println("TAMPAE");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 15);
  if (sessaoAtiva) {
    display.print("Usuario: ");
    String nome = userName;
    if (nome.length() > 17) nome = nome.substring(0, 17);
    display.println(nome);
  } else {
    display.println("Aguardando usuario");
  }

  display.setCursor(0, 29);
  display.print("Passagens: ");
  display.println(passagens);

  display.setCursor(0, 42);
  display.print("Peso: ");
  display.print(pesoGramas, 0);
  display.println(" g");

  display.setCursor(0, 55);
  display.print("Pontos: ");
  display.println(pontos);
  display.display();
}

bool prepararHttp(HTTPClient& http, WiFiClientSecure& client, const String& endpoint) {
  if (WiFi.status() != WL_CONNECTED) return false;
  client.setInsecure();
  if (!http.begin(client, endpoint)) return false;
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  return true;
}

bool testarBanco() {
  WiFiClientSecure client;
  HTTPClient http;
  String endpoint = String(SUPABASE_URL) + "/rest/v1/machines?id=eq." + MACHINE_ID + "&select=id";
  if (!prepararHttp(http, client, endpoint)) return false;
  int code = http.GET();
  String response = http.getString();
  http.end();
  if (code == 200) {
    Serial.println("Supabase conectado.");
    return true;
  }
  Serial.print("ERRO Supabase HTTP: ");
  Serial.println(code);
  Serial.println(response);
  return false;
}

void limparFila() {
  portENTER_CRITICAL(&filaMux);
  filaInicio = 0;
  filaFim = 0;
  filaQuantidade = 0;
  portEXIT_CRITICAL(&filaMux);
}

bool adicionarFila(float peso) {
  bool adicionou = false;
  portENTER_CRITICAL(&filaMux);
  if (filaQuantidade < TAMANHO_FILA) {
    filaPesos[filaFim] = peso;
    filaFim = (filaFim + 1) % TAMANHO_FILA;
    filaQuantidade++;
    adicionou = true;
  }
  portEXIT_CRITICAL(&filaMux);
  return adicionou;
}

bool retirarFila(float& peso) {
  bool retirou = false;
  portENTER_CRITICAL(&filaMux);
  if (filaQuantidade > 0) {
    peso = filaPesos[filaInicio];
    filaInicio = (filaInicio + 1) % TAMANHO_FILA;
    filaQuantidade--;
    retirou = true;
  }
  portEXIT_CRITICAL(&filaMux);
  return retirou;
}

// Tarefa independente para o LDR.
// Assim a leitura nao para quando uma requisicao HTTP estiver acontecendo.
void tarefaSensor(void* parameter) {
  int leituraAnterior = analogRead(PINO_LDR);
  bool bloqueadoAnterior = (leituraAnterior < LIMIAR_LDR);
  unsigned long ultimaDeteccao = 0;

  for (;;) {
    int leitura = analogRead(PINO_LDR);
    valorLDR = leitura;
    bool bloqueado = (leitura < LIMIAR_LDR);

    if (sessaoAtiva && bloqueado && !bloqueadoAnterior) {
      unsigned long agora = millis();
      if (agora - ultimaDeteccao >= DEBOUNCE_MS) {
        int leituraPot = analogRead(PINO_POT);
        float peso = ((float)leituraPot / 4095.0) * PESO_MAXIMO_GRAMAS;

        if (adicionarFila(peso)) {
          ultimaDeteccao = agora;
          Serial.print("LDR detectou tampa | LDR: ");
          Serial.print(leitura);
          Serial.print(" | peso: ");
          Serial.print(peso, 2);
          Serial.println(" g");
        } else {
          Serial.println("AVISO: fila de coletas cheia.");
        }
      }
    }

    bloqueadoAnterior = bloqueado;
    leituraAnterior = leitura;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void consultarSessao() {
  WiFiClientSecure client;
  HTTPClient http;
  String endpoint = String(SUPABASE_URL) + "/rest/v1/rpc/get_active_session";
  if (!prepararHttp(http, client, endpoint)) return;

  String body = "{\"p_machine_id\":\"" + String(MACHINE_ID) + "\",\"p_device_token\":\"" + String(MACHINE_TOKEN) + "\"}";
  int code = http.POST(body);
  if (code != 200) {
    http.end();
    return;
  }

  String response = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, response)) return;

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
      limparFila();
      passagens = 0;
      pontos = 0;
      Serial.println("Sessao encerrada ou expirada.");
      mostrarOperacao();
    }
    return;
  }

  String novaSessao = item["session_id"].as<String>();
  if (!sessaoAtiva || sessionId != novaSessao) {
    limparFila();
    sessionId = novaSessao;
    userId = item["user_id"].as<String>();
    userName = item["nome"].as<String>();
    eventId = item["evento_id"].as<String>();
    sessaoAtiva = true;
    passagens = 0;
    pontos = 0;

    Serial.println("Usuario conectado pelo app.");
    Serial.print("Nome: ");
    Serial.println(userName);
    mostrarOperacao();
  }
}

bool registrarTampa(float peso) {
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
  body += "\"p_peso_real_gramas\":" + String(peso, 2) + ",";
  body += "\"p_peso_estimado_gramas\":null";
  body += "}";

  int code = http.POST(body);
  String response = http.getString();
  http.end();

  if (code >= 200 && code < 300) {
    passagens++;
    pontos++;
    pesoGramas = peso;
    Serial.println("Coleta registrada no banco.");
    Serial.print("Peso enviado: ");
    Serial.print(peso, 2);
    Serial.println(" g");
    return true;
  }

  Serial.print("ERRO ao registrar coleta. HTTP: ");
  Serial.println(code);
  Serial.println(response);
  return false;
}

void processarFilaColetas() {
  if (!sessaoAtiva) return;

  float peso;
  if (!retirarFila(peso)) return;

  // Atualiza a tela imediatamente, antes de esperar a internet.
  pesoGramas = peso;
  passagens++;
  pontos++;
  mostrarOperacao();

  // A coleta ja foi detectada e contabilizada localmente.
  // O envio ao banco acontece em seguida.
  if (!registrarTampa(peso)) {
    // Se o envio falhar, desfaz a exibicao local para nao apresentar
    // uma coleta que nao foi aceita pelo banco.
    passagens--;
    pontos--;
    Serial.println("Coleta nao confirmada pelo banco.");
  }
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

  valorLDR = analogRead(PINO_LDR);

  mostrarTela("TAMPAE", "Iniciando...");
  Serial.println("========================================");
  Serial.println("TAMPAE - INTEGRACAO + LDR + BALANCA");
  Serial.println("========================================");
  Serial.print("LDR inicial: ");
  Serial.println(valorLDR);
  Serial.print("Limiar LDR: ");
  Serial.println(LIMIAR_LDR);

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

  delay(1000);
  mostrarOperacao();

  // O sensor roda em tarefa separada para nao parar durante requisicoes HTTP.
  xTaskCreatePinnedToCore(
    tarefaSensor,
    "TarefaLDR",
    4096,
    nullptr,
    1,
    nullptr,
    0
  );
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

  // Peso da balanca simulada sempre acompanha o potenciometro.
  int valorPot = analogRead(PINO_POT);
  pesoGramas = ((float)valorPot / 4095.0) * PESO_MAXIMO_GRAMAS;

  // OLED atualizado continuamente, sem depender de resposta do Supabase.
  if (millis() - ultimaAtualizacaoTela >= 200) {
    ultimaAtualizacaoTela = millis();
    mostrarOperacao();
  }

  // Consulta a sessao em intervalos maiores para nao interferir no restante.
  if (millis() - ultimaConsultaSessao >= 1500) {
    ultimaConsultaSessao = millis();
    consultarSessao();
  }

  // Envia as tampinhas detectadas pelo sensor em fila.
  processarFilaColetas();

  delay(10);
}
