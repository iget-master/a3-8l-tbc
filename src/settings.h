#pragma once
#include <Arduino.h>

// Parâmetros ajustáveis pela página web, persistidos na NVS (namespace "cfg").
// Persistência por blob versionado: mudou o layout da struct → bump SETTINGS_VERSION
// (a carga com versão diferente volta aos defaults).
constexpr uint8_t SETTINGS_VERSION = 8;

// Máx de amostras da mediana do TPS (janela ímpar); dimensiona o buffer no tps.
constexpr uint8_t TPS_MEDIAN_MAX = 15;

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

  // WiFi AP (rede própria do ESP; fallback quando a rede local não conecta)
  char apSsid[33] = "A3-TBC";
  char apPass[65] = "a3tbc123";  // WPA2 exige >= 8 caracteres

  // WiFi STA (rede local que o ESP tenta entrar no boot; SSID vazio = desativado)
  char staSsid[33] = "";
  char staPass[65] = "";

  // Filtro do TPS (mediana + EMA). Fica no FIM da struct de propósito: campos
  // novos só entram aqui para a migração append-only preservar o que já foi
  // salvo (ver settings::begin()).
  float tpsEmaAlpha = 0.2f;      // EMA: 0<α≤1 (1 = sem suavização; menor = mais suave)
  uint8_t tpsMedianSamples = 5;  // mediana: nº ímpar 1..TPS_MEDIAN_MAX (1 = desliga)

  // Sensor de corrente da ponte (IS do IBT-2). Limiares em counts do ADC
  // normalizados para 100% de duty (isense::estRaw100). Regra da migração: todo
  // bloco anexado deve COMEÇAR por um campo de 4 bytes — o blob antigo termina
  // alinhado em 4, então os campos novos caem inteiros fora dele e mantêm os
  // defaults ao migrar.
  float isMinDutyPct = 25.0f;  // só avalia com |duty| acima disto (normalização confiável)
  uint16_t isShortRaw = 3000;  // acima → curto no motor (proteção do driver ≈ satura o ADC)
  uint16_t isOpenRaw = 15;     // abaixo → motor desconectado
  uint16_t isStallRaw = 250;   // acima → fim de curso (stall mecânico)
  uint16_t isShortMs = 20;     // persistência mínima de cada condição
  uint16_t isOpenMs = 300;
  uint16_t isStallMs = 80;
  bool isenseEnabled = false;  // sem o circuito ligado o pino flutua → padrão OFF

  // TPS com pista invertida (tensão maior fechado): 1 = espelha a leitura
  // (4095 − raw) na fonte — todo o resto do firmware enxerga raw crescendo ao
  // abrir. uint32_t (e não bool) para respeitar a regra da migração: o bloco
  // anexado começa em campo de 4 bytes.
  uint32_t tpsInvert = 0;

  // Calibração: medir a fase de fechamento (drive −calDrivePct)? No corpo do
  // 8L o repouso é o batente fechado E recolher o pino descola a alavanca,
  // abrindo o switch de idle — a fase não mede nada e aborta a rotina. Padrão
  // desligado: mín = repouso. uint32_t pela regra da migração (campo de 4 B).
  uint32_t calMeasureClose = 0;

  // Rampa do setpoint (%/s; 0 = desligada): limita a variação do setpoint que
  // o PID segue. Evita a pancada no batente (e o quique que o PID caça) em
  // retornos rápidos ao repouso.
  float spSlewPctPerS = 250.0f;

  // Saída analógica: rebase dinâmico do zero na soltura do idle — 0% = posição
  // da borboleta no instante em que o pedal assume (a posição do atuador não
  // vaza pro sinal); 100% continua fixo no máx aprendido/WOT. Desligado = a
  // régua fixa antiga (mín da calibração). uint32_t pela regra da migração.
  uint32_t outBaseOnRelease = 1;
};

namespace settings {
void begin();          // carrega da NVS (defaults se vazio/versão diferente)
Settings& get();       // referência mutável — tudo roda na task do loop()
void save();           // persiste a struct inteira
void resetDefaults();  // volta aos defaults em RAM (não salva)
}
