#include <WiFi.h>
#include <sys/time.h>
#include "time.h"
#include "ESP32_WS2812_Lib.h"

// 20 LEDs de bits (2 + 4 + 3 + 4 + 3 + 4) + 5 LEDs delimitadores = 25 LEDs
#define LEDS_COUNT 25
#define LEDS_PIN   8
#define CHANNEL    0

ESP32_WS2812 strip = ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_BGR);

// --- Flag de controle NTP ---
const bool USAR_NTP = true; // true = sincroniza via NTP | false = usa RTC interno a partir de 00:00:00

// --- Configuracao Wi-Fi e NTP (Brasilia UTC-3) ---
const char* ssid               = "SEU_WIFI";
const char* password           = "SUA_SENHA";
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = -3 * 3600;
const int   daylightOffset_sec = 0;

// --- Cores base em 100% (R, G, B) ---
uint8_t corHora[3]        = { 255, 0, 255 };  
uint8_t corMinuto[3]      = { 0, 0, 255 };  
uint8_t corSegundo[3]     = { 0, 255, 0 };    
uint8_t corDelimitador[3] = { 255, 0, 0 }; 

// Fatores de brilho: 50% para bit 1 e delimitador, 5% para bit 0
const float BRILHO_BIT_1       = 0.20f;
const float BRILHO_BIT_0       = 0.01f;
const float BRILHO_DELIMITADOR = 0.20f;

void desligarWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("Wi-Fi desligado.");
}

// Define uma hora inicial (em horario local UTC-3) no RTC do sistema
void definirHoraRTC(int ano, int mes, int dia, int hora, int minuto, int segundo) {
  struct tm tmLocal = {};
  tmLocal.tm_year  = ano - 1900;
  tmLocal.tm_mon   = mes - 1;
  tmLocal.tm_mday  = dia;
  tmLocal.tm_hour  = hora;
  tmLocal.tm_min   = minuto;
  tmLocal.tm_sec   = segundo;
  tmLocal.tm_isdst = -1;

  time_t epochUtc = mktime(&tmLocal);
  struct timeval tv = { .tv_sec = epochUtc, .tv_usec = 0 };
  settimeofday(&tv, nullptr);
}

bool sincronizarHoraNTP(uint32_t timeoutMs = 15000) {
  Serial.println("Conectando ao Wi-Fi para sincronizar NTP...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  uint32_t inicio = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - inicio >= timeoutMs) {
      Serial.println("\nFalha ao conectar no Wi-Fi para NTP.");
      return false;
    }
    delay(250);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi conectado! Consultando NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  uint32_t inicioNtp = millis();
  while (millis() - inicioNtp < 10000) {
    if (getLocalTime(&timeinfo, 200)) {
      Serial.printf("Hora sincronizada via NTP: %02d:%02d:%02d\n",
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      return true;
    }
  }

  Serial.println("Nao foi possivel obter hora do NTP.");
  return false;
}

void acenderLedEscalado(int ledIdx, uint8_t corBase[3], float fator) {
  if (ledIdx < 0 || ledIdx >= LEDS_COUNT) return;
  uint8_t r    = (uint8_t)(corBase[0] * fator);
  uint8_t g    = (uint8_t)(corBase[1] * fator);
  uint8_t bCol = (uint8_t)(corBase[2] * fator);
  strip.setLedColorData(ledIdx, r, g, bCol);
}

void mostrarDigito(int valor, int startIdx, int numBits, uint8_t corBase[3]) {
  for (int b = 0; b < numBits; b++) {
    // Ordem invertida dentro do grupo: MSB em startIdx, LSB em startIdx + numBits - 1
    int ledIdx = startIdx + (numBits - 1 - b);
    bool bitAtivo = (valor >> b) & 0x01;
    float fator = bitAtivo ? BRILHO_BIT_1 : BRILHO_BIT_0;
    acenderLedEscalado(ledIdx, corBase, fator);
  }
}

void mostrarDelimitadores() {
  const int indicesDelimitadores[] = { 2, 7, 11, 16, 20 };
  const int qtd = sizeof(indicesDelimitadores) / sizeof(indicesDelimitadores[0]);

  for (int i = 0; i < qtd; i++) {
    acenderLedEscalado(indicesDelimitadores[i], corDelimitador, BRILHO_DELIMITADOR);
  }
}

void atualizarFita(int h, int m, int s) {
  int h1 = h / 10, h2 = h % 10;
  int m1 = m / 10, m2 = m % 10;
  int s1 = s / 10, s2 = s % 10;

  mostrarDigito(h1, 0,  2, corHora);    // LEDs 0..1
  mostrarDigito(h2, 3,  4, corHora);    // LEDs 3..6
  mostrarDigito(m1, 8,  3, corMinuto);  // LEDs 8..10
  mostrarDigito(m2, 12, 4, corMinuto);  // LEDs 12..15
  mostrarDigito(s1, 17, 3, corSegundo); // LEDs 17..19
  mostrarDigito(s2, 21, 4, corSegundo); // LEDs 21..24

  mostrarDelimitadores();

  strip.show();
}

void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.setBrightness(255);

  mostrarDelimitadores();

  strip.show();

  // Fuso horario de Brasilia (UTC-3)
  setenv("TZ", "<-03>3", 1);
  tzset();

  if (USAR_NTP) {
    sincronizarHoraNTP();
  } else {
    definirHoraRTC(2026, 11, 20, 0, 0, 0);
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 50)) {
    atualizarFita(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    Serial.printf("Relogio iniciado em %02d:%02d:%02d\n",
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }

  // Desliga o Wi-Fi assim que o setup termina
  desligarWiFi();
}

void loop() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 50)) {
    atualizarFita(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }

  delay(200);
}
