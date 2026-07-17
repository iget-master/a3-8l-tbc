#pragma once
#include <Arduino.h>

// Parâmetros ajustáveis pela página web, persistidos na NVS (namespace "cfg").
// Persistência por blob versionado: mudou o layout da struct → bump SETTINGS_VERSION
// (a carga com versão diferente volta aos defaults).
constexpr uint8_t SETTINGS_VERSION = 1;

struct Settings {
  // PID (% de duty por % de erro de posição)
  float kp = 4.0f;
  float ki = 8.0f;            // por segundo
  float kd = 0.05f;
  float deadbandPct = 0.5f;   // |erro| abaixo disso é tratado como zero (segura o integrador)
  float maxDutyPct = 100.0f;  // saturação simétrica do esforço
  uint16_t loopHz = 200;      // frequência da malha de controle

  // Entrada de comando (PWM)
  uint16_t cmdTimeoutMs = 250;    // sem bordas além disso → nível estático/sinal ausente
  bool cmdStuckHighIs100 = true;  // nível alto estático vale 100%?

  // Ponte H
  uint32_t pwmFreqHz = 20000;

  // TPS — plausibilidade (fora da faixa → falha, motor desligado)
  uint16_t tpsFaultLowRaw = 30;
  uint16_t tpsFaultHighRaw = 4080;

  // Auto calibração
  uint16_t calSettleMs = 500;        // tempo com leitura estável para registrar
  uint16_t calStabilityCounts = 12;  // variação máxima (ADC) considerada estável
  uint32_t calTimeoutMs = 4000;      // timeout por fase
  uint16_t calMinRangeCounts = 200;  // faixa mínima aceitável entre mín e máx
  float calDrivePct = 100.0f;        // duty usado nas fases de máx/mín

  // Saída analógica (mapeamento do TPS fora de idle); 0 = automático
  uint16_t outMinRaw = 0;      // 0 → usa minRaw da calibração
  uint16_t outMaxRaw = 0;      // 0 → usa máx aprendido (ou maxRaw da calibração)
  bool outAutoLearnMax = true; // aprende o maior raw visto fora de idle

  // Switch de idle
  bool idleActiveLow = true;   // fecha para GND quando em idle
  uint16_t idleDebounceMs = 20;

  // WiFi AP
  char apSsid[33] = "A3-TBC";
  char apPass[65] = "a3tbc123";  // WPA2 exige >= 8 caracteres
};

namespace settings {
void begin();          // carrega da NVS (defaults se vazio/versão diferente)
Settings& get();       // referência mutável — tudo roda na task do loop()
void save();           // persiste a struct inteira
void resetDefaults();  // volta aos defaults em RAM (não salva)
}
