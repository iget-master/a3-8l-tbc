#include "isense.h"

#include "pins.h"
#include "settings.h"

namespace isense {
namespace {

constexpr float kAlpha = 0.25f;  // RC externo já suaviza; EMA leve basta

float s_ema = 0.0f;
float s_est = 0.0f;  // counts @100% de duty (0 = não avaliável)
bool s_stall = false;
bool s_short = false;
bool s_open = false;
uint32_t s_stallSinceMs = 0;  // 0 = condição ausente
uint32_t s_shortSinceMs = 0;
uint32_t s_openSinceMs = 0;

// Condição precisa persistir por `ms` contínuos para valer.
bool window(bool cond, uint32_t now, uint32_t ms, uint32_t& since) {
  if (!cond) {
    since = 0;
    return false;
  }
  if (since == 0) since = now;
  return now - since >= ms;
}

void resetConditions() {
  s_est = 0.0f;
  s_stall = s_short = s_open = false;
  s_stallSinceMs = s_shortSinceMs = s_openSinceMs = 0;
}

}  // namespace

void begin() {
  analogSetPinAttenuation(PIN_ISENSE, ADC_11db);
  s_ema = (float)analogRead(PIN_ISENSE);
}

void poll() { s_ema += kAlpha * ((float)analogRead(PIN_ISENSE) - s_ema); }

uint16_t raw() { return (uint16_t)(s_ema + 0.5f); }

void evaluate(uint32_t nowMs, float appliedDutyPct) {
  const Settings& cfg = settings::get();
  const float d = fabsf(appliedDutyPct);
  if (!cfg.isenseEnabled || d < cfg.isMinDutyPct) {
    // Sem drive suficiente a leitura não distingue nada — zera as janelas para
    // não somar tempo de condições de ciclos antigos.
    resetConditions();
    return;
  }
  // Média no IS ~ duty × I_motor/8500 × R → normaliza para 100% de duty. A EMA
  // atrasa em relação a variações rápidas do duty (PID); as janelas de tempo
  // absorvem esse transitório.
  s_est = (float)raw() * 100.0f / d;
  s_short = window(s_est >= (float)cfg.isShortRaw, nowMs, cfg.isShortMs, s_shortSinceMs);
  s_open = window(s_est <= (float)cfg.isOpenRaw, nowMs, cfg.isOpenMs, s_openSinceMs);
  s_stall = window(s_est >= (float)cfg.isStallRaw, nowMs, cfg.isStallMs, s_stallSinceMs);
}

float estRaw100() { return s_est; }
bool stalled() { return s_stall; }
bool shortCircuit() { return s_short; }
bool openCircuit() { return s_open; }

}  // namespace isense
