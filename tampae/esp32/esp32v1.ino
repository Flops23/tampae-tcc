// ============================================================
// TAMPAÊ - ESP32 V1
// Máquina fixa da ETEC Prof. Carmine Biagio Tundisi - Atibaia
//
// Painel local da máquina:
// - QR Code da máquina/evento
// - identifica a sessão conectada
// - mostra o nome do usuário
// - permite registrar pontos manualmente
// - permite encerrar a sessão sem coleta
// ============================================================

#include <WiFi.h>
#include <WebServer.h>

// ------------------------------------------------------------
// CONFIGURAÇÃO WI-FI
// ------------------------------------------------------------
const char* WIFI_SSID = "SEU_HOTSPOT";
const char* WIFI_PASSWORD = "SUA_SENHA";

// ------------------------------------------------------------
// SUPABASE
// ------------------------------------------------------------
const char* SUPABASE_URL = "https://jtmbsyharkxrpnkunbuj.supabase.co";
const char* SUPABASE_ANON_KEY = "sb_publishable_TeblGQP9D6s24o0IUiZbAg_CKxmxgPo";

// ------------------------------------------------------------
// IDENTIFICAÇÃO DA MÁQUINA
// ------------------------------------------------------------
const char* MACHINE_ID = "379a1459-797e-47e5-9a73-de949e72f9f5";
const char* MACHINE_TOKEN = "62263534-37bb-451e-a89f-9c77c3234ddd";
const char* MACHINE_NAME = "TAMPAÊ — ETEC Prof. Carmine Biagio Tundisi";

// Evento fixo usado durante o desenvolvimento.
const char* EVENT_ID = "aa1186e6-639c-46d9-8e9a-97239479f3c7";

WebServer server(80);

// ------------------------------------------------------------
// PAINEL WEB DA MÁQUINA
// ------------------------------------------------------------
void handleRoot() {
  String html = R"HTML(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>TAMPAÊ — Máquina</title>
  <script src="https://cdn.jsdelivr.net/npm/qrcodejs@1.0.0/qrcode.min.js"></script>
  <style>
    * { box-sizing: border-box; }
    body { font-family: Arial, sans-serif; margin: 0; padding: 18px; background: #f6faf7; color: #18301f; }
    main { max-width: 620px; margin: auto; }
    .card { background: white; padding: 22px; border-radius: 18px; box-shadow: 0 4px 20px #0001; margin-bottom: 16px; }
    h1 { margin: 0 0 6px; font-size: 28px; }
    h2 { margin: 0 0 8px; font-size: 20px; }
    .machine { color: #555; margin-bottom: 18px; }
    .status { padding: 12px; border-radius: 12px; background: #e7f5ea; font-weight: bold; }
    .status.waiting { background: #fff4d6; }
    .qr { display: flex; justify-content: center; padding: 18px 0 8px; }
    #qrcode { padding: 12px; background: white; border-radius: 12px; }
    .qr-help { text-align: center; color: #666; font-size: 14px; }
    .user { margin-top: 18px; padding: 18px; border-radius: 14px; background: #f1f7f2; }
    .user-name { font-size: 25px; font-weight: bold; margin-top: 4px; }
    .session-id { color: #777; font-size: 12px; word-break: break-all; margin-top: 8px; }
    label { display: block; font-weight: bold; margin: 16px 0 7px; }
    input { width: 100%; padding: 14px; border: 1px solid #ccd6cf; border-radius: 10px; font-size: 20px; }
    button { width: 100%; border: 0; border-radius: 10px; padding: 14px; margin-top: 12px; font-size: 16px; font-weight: bold; cursor: pointer; }
    .primary { background: #15803d; color: white; }
    .danger { background: #b91c1c; color: white; }
    button:disabled { opacity: .5; cursor: not-allowed; }
    .message { min-height: 22px; margin-top: 12px; font-size: 14px; }
    .details { font-size: 13px; color: #666; line-height: 1.5; }
    code { word-break: break-all; }
  </style>
</head>
<body>
<main>
  <section class="card">
    <h1>TAMPAÊ</h1>
    <div class="machine">%MACHINE_NAME%</div>
    <div class="status" id="status">Aguardando usuário...</div>
  </section>

  <section class="card">
    <h2>QR Code da máquina</h2>
    <div class="qr"><div id="qrcode"></div></div>
    <div class="qr-help">Abra o TAMPAÊ no celular e escaneie este QR Code.</div>
  </section>

  <section class="card" id="sessionCard" style="display:none">
    <h2>Usuário conectado</h2>
    <div class="user">
      <div>Nome:</div>
      <div class="user-name" id="userName">—</div>
      <div class="session-id" id="sessionId"></div>
    </div>

    <label for="points">Pontos da coleta</label>
    <input id="points" type="number" min="1" step="1" value="1" inputmode="numeric">
    <button class="primary" id="registerBtn">Registrar pontos</button>
    <button class="danger" id="closeBtn">Encerrar sessão sem pontuar</button>
    <div class="message" id="message"></div>
  </section>

  <section class="card details">
    <strong>Machine ID:</strong> <code>%MACHINE_ID%</code><br>
    <strong>Event ID:</strong> <code>%EVENT_ID%</code><br>
    <strong>MAC:</strong> <span id="mac">carregando...</span><br>
    <strong>IP:</strong> <span id="ip">carregando...</span>
  </section>
</main>

<script>
const SUPABASE_URL = '%SUPABASE_URL%';
const SUPABASE_KEY = '%SUPABASE_KEY%';
const MACHINE_ID = '%MACHINE_ID%';
const MACHINE_TOKEN = '%MACHINE_TOKEN%';
const EVENT_ID = '%EVENT_ID%';
let currentSession = null;
let busy = false;

const $ = id => document.getElementById(id);

function setStatus(text, waiting = false) {
  $('status').textContent = text;
  $('status').className = 'status' + (waiting ? ' waiting' : '');
}

function setMessage(text) {
  $('message').textContent = text;
}

function showWaiting() {
  currentSession = null;
  $('sessionCard').style.display = 'none';
  setStatus('Aguardando usuário...', true);
  setMessage('');
}

function showSession(data) {
  currentSession = data;
  $('sessionCard').style.display = 'block';
  $('userName').textContent = data.nome || 'Usuário';
  $('sessionId').textContent = 'Sessão: ' + data.session_id;
  setStatus('Usuário conectado');
}

async function rpc(name, body) {
  const response = await fetch(SUPABASE_URL + '/rest/v1/rpc/' + name, {
    method: 'POST',
    headers: {
      'apikey': SUPABASE_KEY,
      'Authorization': 'Bearer ' + SUPABASE_KEY,
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(body)
  });
  const text = await response.text();
  let data = null;
  try { data = text ? JSON.parse(text) : null; } catch { data = text; }
  if (!response.ok) {
    throw new Error(typeof data === 'object' ? (data.message || data.hint || JSON.stringify(data)) : String(data));
  }
  return data;
}

async function checkSession() {
  if (busy) return;
  try {
    const data = await rpc('get_active_session', {
      p_machine_id: MACHINE_ID,
      p_device_token: MACHINE_TOKEN
    });

    const session = Array.isArray(data) ? data[0] : data;
    if (session && session.session_id) {
      if (!currentSession || currentSession.session_id !== session.session_id || currentSession.nome !== session.nome) {
        showSession(session);
      }
    } else if (currentSession) {
      showWaiting();
    }
  } catch (error) {
    console.error(error);
    setStatus('Erro ao consultar a sessão', true);
    setMessage(error.message || 'Falha na comunicação com o Supabase.');
  }
}

async function registerPoints() {
  if (!currentSession || busy) return;
  const points = Number($('points').value);
  if (!Number.isInteger(points) || points < 1) {
    setMessage('Digite uma quantidade de pontos válida.');
    return;
  }

  busy = true;
  $('registerBtn').disabled = true;
  $('closeBtn').disabled = true;
  setMessage('Registrando coleta...');

  try {
    await rpc('registrar_coleta', {
      p_machine_id: MACHINE_ID,
      p_device_token: MACHINE_TOKEN,
      p_session_id: currentSession.session_id,
      p_tipo_coleta: 'unitaria',
      p_quantidade_real: points,
      p_quantidade_estimada: null,
      p_peso_real_gramas: null,
      p_peso_estimado_gramas: null
    });
    setMessage('Pontos registrados com sucesso.');
    currentSession = null;
    setTimeout(showWaiting, 700);
  } catch (error) {
    console.error(error);
    setMessage(error.message || 'Não foi possível registrar os pontos.');
  } finally {
    busy = false;
    $('registerBtn').disabled = false;
    $('closeBtn').disabled = false;
  }
}

async function closeSession() {
  if (!currentSession || busy) return;
  busy = true;
  $('registerBtn').disabled = true;
  $('closeBtn').disabled = true;
  setMessage('Encerrando sessão...');

  try {
    const data = await rpc('encerrar_sessao_maquina', {
      p_machine_id: MACHINE_ID,
      p_device_token: MACHINE_TOKEN,
      p_session_id: currentSession.session_id
    });
    const row = Array.isArray(data) ? data[0] : data;
    setMessage('Sessão encerrada. Pontos registrados: ' + Number(row?.pontos_sessao || 0));
    setTimeout(showWaiting, 700);
  } catch (error) {
    console.error(error);
    setMessage(error.message || 'Não foi possível encerrar a sessão.');
  } finally {
    busy = false;
    $('registerBtn').disabled = false;
    $('closeBtn').disabled = false;
  }
}

function createQr() {
  const payload = JSON.stringify({ machine_id: MACHINE_ID, event_id: EVENT_ID });
  new QRCode($('qrcode'), {
    text: payload,
    width: 230,
    height: 230,
    correctLevel: QRCode.CorrectLevel.M
  });
}

$('registerBtn').addEventListener('click', registerPoints);
$('closeBtn').addEventListener('click', closeSession);
createQr();
checkSession();
setInterval(checkSession, 1500);

fetch('/info').then(r => r.json()).then(info => {
  $('mac').textContent = info.mac;
  $('ip').textContent = info.ip;
}).catch(() => {});
</script>
</body>
</html>
)HTML";

  html.replace("%MACHINE_NAME%", MACHINE_NAME);
  html.replace("%MACHINE_ID%", MACHINE_ID);
  html.replace("%MACHINE_TOKEN%", MACHINE_TOKEN);
  html.replace("%EVENT_ID%", EVENT_ID);
  html.replace("%SUPABASE_URL%", SUPABASE_URL);
  html.replace("%SUPABASE_KEY%", SUPABASE_ANON_KEY);
  server.send(200, "text/html; charset=utf-8", html);
}

void handleHealth() {
  server.send(200, "application/json", "{\"status\":\"ok\",\"firmware\":\"esp32v1\"}");
}

void handleInfo() {
  String json = "{\"mac\":\"" + WiFi.macAddress() + "\",\"ip\":\"" + WiFi.localIP().toString() + ""}";
  server.send(200, "application/json", json);
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
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
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
  server.on("/info", HTTP_GET, handleInfo);
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
