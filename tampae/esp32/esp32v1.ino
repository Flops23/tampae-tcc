// ============================================================
// TAMPAÊ - ESP32 V1
// Máquina fixa da ETEC Prof. Carmine Biagio Tundisi - Atibaia
//
// Primeira versão de desenvolvimento do firmware.
// Nesta etapa o objetivo é preparar a comunicação Wi-Fi e
// disponibilizar um servidor HTTP local para testes.
// Sensores/OLED serão integrados nas próximas versões.
// ============================================================

#include <WiFi.h>
#include <WebServer.h>

// ------------------------------------------------------------
// CONFIGURAÇÃO WI-FI
// ------------------------------------------------------------
// Preencha com o nome e senha do hotspot do celular.
const char* WIFI_SSID = "SEU_HOTSPOT";
const char* WIFI_PASSWORD = "SUA_SENHA";

// ------------------------------------------------------------
// IDENTIFICAÇÃO DA MÁQUINA
// ------------------------------------------------------------
const char* MACHINE_ID = "379a1459-797e-47e5-9a73-de949e72f9f5";
const char* MACHINE_TOKEN = "62263534-37bb-451e-a89f-9c77c3234ddd";

// Evento fixo usado durante o desenvolvimento.
const char* EVENT_ID = "aa1186e6-639c-46d9-8e9a-97239479f3c7";

WebServer server(80);

void handleRoot() {
  String html = R"HTML(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>TAMPAÊ - ESP32 V1</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 24px; background: #f6faf7; color: #18301f; }
    main { max-width: 520px; margin: auto; background: white; padding: 24px; border-radius: 16px; box-shadow: 0 4px 20px #0001; }
    h1 { margin-top: 0; }
    .status { padding: 12px; border-radius: 10px; background: #e7f5ea; margin: 16px 0; }
    code { word-break: break-all; }
  </style>
</head>
<body>
<main>
  <h1>TAMPAÊ — ESP32 V1</h1>
  <div class="status">ESP32 conectado e servidor local funcionando.</div>
  <p><strong>Máquina:</strong><br><code>%MACHINE_ID%</code></p>
  <p><strong>Evento:</strong><br><code>%EVENT_ID%</code></p>
  <p>Próxima etapa: sessão de coleta, leitura dos sensores e envio ao Supabase.</p>
</main>
</body>
</html>
)HTML";

  html.replace("%MACHINE_ID%", MACHINE_ID);
  html.replace("%EVENT_ID%", EVENT_ID);
  server.send(200, "text/html; charset=utf-8", html);
}

void handleHealth() {
  server.send(200, "application/json", "{\"status\":\"ok\",\"firmware\":\"esp32v1\"}");
}

void connectWiFi() {
  Serial.println();
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi conectado.");
    Serial.print("IP do ESP32: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Falha ao conectar ao Wi-Fi.");
    Serial.println("Confira SSID e senha do hotspot.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("========================================");
  Serial.println("TAMPAÊ - ESP32 V1");
  Serial.println("Máquina ETEC Atibaia");
  Serial.println("========================================");

  Serial.print("Machine ID: ");
  Serial.println(MACHINE_ID);
  Serial.print("Event ID: ");
  Serial.println(EVENT_ID);

  connectWiFi();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/health", HTTP_GET, handleHealth);
  server.begin();

  Serial.println("Servidor HTTP iniciado na porta 80.");
}

void loop() {
  server.handleClient();

  // Reconecta automaticamente caso o hotspot seja interrompido.
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect >= 10000) {
      lastReconnect = millis();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
}
