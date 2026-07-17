#include "pwm_input.h"

#include "pins.h"
#include "settings.h"

namespace pwm_input {
namespace {

constexpr uint32_t kMinPeriodUs = 100;      // < 100 us = glitch
constexpr uint32_t kMaxPeriodUs = 1000000;  // > 1 s não é PWM válido

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

// Escritos pela ISR; copiados no poll() sob seção crítica.
volatile uint32_t s_riseUs = 0;      // última borda de subida
volatile uint32_t s_fallUs = 0;      // última borda de descida
volatile uint32_t s_edgeCount = 0;   // total de bordas (base do timeout)
volatile uint32_t s_periodUs = 0;    // ciclo anterior válido (0 = nenhum)
volatile uint32_t s_highUs = 0;      // tempo em alto do mesmo ciclo

// Estado derivado no poll() — só a task do loop() acessa.
float s_dutyPct = 0.0f;
float s_freqHz = 0.0f;
bool s_present = false;
uint32_t s_ageMs = 0;
uint32_t s_seenEdgeCount = 0;   // último s_edgeCount visto pelo poll()
uint32_t s_lastNewEdgeMs = 0;   // millis() em que o poll() viu borda nova
bool s_timedOut = false;        // latch: só limpa com borda nova de verdade

void IRAM_ATTR isrEdge() {
  // Subtração uint32_t de micros() é imune ao overflow de ~71 min.
  const uint32_t now = micros();
  if (digitalRead(PIN_CMD_PWM)) {  // subida: fecha o ciclo anterior
    const uint32_t period = now - s_riseUs;
    const uint32_t high = s_fallUs - s_riseUs;
    // high > period = bordas fora de ordem (ruído) — descarta o ciclo
    if (period >= kMinPeriodUs && period <= kMaxPeriodUs && high <= period) {
      portENTER_CRITICAL_ISR(&s_mux);
      s_periodUs = period;
      s_highUs = high;
      portEXIT_CRITICAL_ISR(&s_mux);
    }
    s_riseUs = now;
  } else {
    s_fallUs = now;
  }
  s_edgeCount = s_edgeCount + 1;
}

}  // namespace

void begin() {
  pinMode(PIN_CMD_PWM, INPUT);  // GPIO35: somente entrada, sem pull interno
  const uint32_t now = micros();
  s_riseUs = now;
  s_fallUs = now;
  s_lastNewEdgeMs = millis();
  attachInterrupt(digitalPinToInterrupt(PIN_CMD_PWM), isrEdge, CHANGE);
}

void poll() {
  uint32_t periodUs, highUs, edgeCount;
  portENTER_CRITICAL(&s_mux);
  periodUs = s_periodUs;
  highUs = s_highUs;
  edgeCount = s_edgeCount;
  portEXIT_CRITICAL(&s_mux);

  // Timeout por contagem de bordas + latch: comparar idade em micros() geraria
  // uma janela falsa de "sinal recente" a cada wrap de ~71,6 min sem bordas.
  const uint32_t nowMs = millis();
  if (edgeCount != s_seenEdgeCount) {
    s_seenEdgeCount = edgeCount;
    s_lastNewEdgeMs = nowMs;
    s_timedOut = false;
  }
  s_ageMs = nowMs - s_lastNewEdgeMs;

  const Settings& cfg = settings::get();
  if (s_timedOut || s_ageMs >= cfg.cmdTimeoutMs) {
    s_timedOut = true;
    if (s_ageMs < cfg.cmdTimeoutMs) s_ageMs = cfg.cmdTimeoutMs;  // wrap: satura
    // Sem bordas: o nível estático do pino decide. Zera a medição velha para
    // não reportar duty obsoleto quando o sinal voltar.
    portENTER_CRITICAL(&s_mux);
    s_periodUs = 0;
    s_highUs = 0;
    portEXIT_CRITICAL(&s_mux);
    s_freqHz = 0.0f;
    if (digitalRead(PIN_CMD_PWM) && cfg.cmdStuckHighIs100) {
      s_dutyPct = 100.0f;
      s_present = true;
    } else {
      // baixo (ou alto com cmdStuckHighIs100 desligado): ausente, duty 0
      s_dutyPct = 0.0f;
      s_present = false;
    }
    return;
  }

  if (periodUs == 0) {  // bordas recentes, mas nenhum ciclo completo ainda
    s_dutyPct = 0.0f;
    s_freqHz = 0.0f;
    s_present = false;
    return;
  }

  s_freqHz = 1e6f / (float)periodUs;
  s_dutyPct = constrain(100.0f * (float)highUs / (float)periodUs, 0.0f, 100.0f);
  s_present = true;
}

float dutyPct() { return s_dutyPct; }
float freqHz() { return s_freqHz; }
bool signalPresent() { return s_present; }
uint32_t lastEdgeAgeMs() { return s_ageMs; }

}  // namespace pwm_input
