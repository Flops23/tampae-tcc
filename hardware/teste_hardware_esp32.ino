#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// TAMPAÊ - TESTE DE HARDWARE
// ESP32 DevKit V1
//
// Testa:
// - OLED 128x64
// - Potenciômetro como peso simulado
// - LDR como detector de passagem
// - Contagem de passagens
// - Pontuação
// ============================================================

// -------------------------
// OLED
// -------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------------
// PINOS - ESP32 DEVKIT V1
// -------------------------
#define PINO_LDR 35
#define PINO_POT 34
#define SDA_PIN 21
#define SCL_PIN 22

// -------------------------
// CONFIGURAÇÕES
// -------------------------
// Ajustado para funcionar com o LDR em luz ambiente.
#define LIMIAR_LDR 300

#define DEBOUNCE_MS 500
#define PESO_MAXIMO_GRAMAS 500.0
#define PONTOS_POR_PASSAGEM 1

// -------------------------
// VARIÁVEIS
// -------------------------
int passagens = 0;
int pontos = 0;
float pesoGramas = 0;

bool objetoDetectado = false;
bool estadoAnteriorLDR = false;
unsigned long ultimaPassagem = 0;

// ============================================================
// ATUALIZA OLED
// ============================================================
void atualizarDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("TAMPAE");

  display.drawLine(0, 18, 127, 18, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print("Passagens: ");
  display.println(passagens);

  display.setCursor(0, 38);
  display.print("Peso: ");
  display.print(pesoGramas, 0);
  display.println(" g");

  display.setCursor(0, 52);
  display.print("Pontos: ");
  display.println(pontos);

  display.display();
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("TAMPAE - TESTE DE HARDWARE");
  Serial.println("================================");

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERRO: OLED nao encontrado!");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("OLED OK");

  analogReadResolution(12);
  analogSetPinAttenuation(PINO_LDR, ADC_11db);
  analogSetPinAttenuation(PINO_POT, ADC_11db);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("TAMPAE");

  display.setTextSize(1);
  display.setCursor(0, 25);
  display.println("Teste de hardware");
  display.setCursor(0, 40);
  display.println("Iniciando...");
  display.display();

  delay(2000);
  atualizarDisplay();

  Serial.println("Sistema iniciado.");
  Serial.println("Limiar LDR: 300");
  Serial.println();
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  // ----------------------------------------------------------
  // POTENCIÔMETRO -> PESO SIMULADO
  // ----------------------------------------------------------
  int valorPot = analogRead(PINO_POT);
  pesoGramas = ((float)valorPot / 4095.0) * PESO_MAXIMO_GRAMAS;

  // ----------------------------------------------------------
  // LDR
  // ----------------------------------------------------------
  int valorLDR = analogRead(PINO_LDR);

  Serial.print("LDR: ");
  Serial.print(valorLDR);
  Serial.print(" | POT: ");
  Serial.print(valorPot);
  Serial.print(" | Peso: ");
  Serial.print(pesoGramas, 0);
  Serial.print(" g | Passagens: ");
  Serial.print(passagens);
  Serial.print(" | Pontos: ");
  Serial.println(pontos);

  // ----------------------------------------------------------
  // DETECÇÃO DE PASSAGEM
  // ----------------------------------------------------------
  // Com o divisor usado no teste:
  // valor abaixo de 300 = objeto bloqueando a luz.
  objetoDetectado = (valorLDR < LIMIAR_LDR);

  // Conta somente a transição de livre -> bloqueado.
  if (objetoDetectado && !estadoAnteriorLDR) {
    unsigned long agora = millis();

    if (agora - ultimaPassagem >= DEBOUNCE_MS) {
      passagens++;
      pontos += PONTOS_POR_PASSAGEM;
      ultimaPassagem = agora;

      Serial.println();
      Serial.println(">>> PASSAGEM DETECTADA <<<");
      Serial.print("Passagem numero: ");
      Serial.println(passagens);
      Serial.print("Pontos: ");
      Serial.println(pontos);
      Serial.println();

      atualizarDisplay();
    }
  }

  estadoAnteriorLDR = objetoDetectado;

  // ----------------------------------------------------------
  // ATUALIZA OLED
  // ----------------------------------------------------------
  static unsigned long ultimaAtualizacaoDisplay = 0;

  if (millis() - ultimaAtualizacaoDisplay >= 200) {
    ultimaAtualizacaoDisplay = millis();
    atualizarDisplay();
  }

  delay(50);
}
