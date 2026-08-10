#include "motor.h"

#include "pins.h"
#include "settings.h"

namespace motor {
namespace {

constexpr uint8_t kCh = 0;
constexpr uint8_t kResBits = 10;
constexpr uint32_t kMaxCounts = (1u << kResBits) - 1;

bool s_enabled = false;
float s_appliedDuty = 0.0f;
uint32_t s_freqHz = 20000;

uint32_t toCounts(float pct) {
  return (uint32_t)(pct * (float)kMaxCounts / 100.0f + 0.5f);
}

}  // namespace

void begin() {
  s_freqHz = settings::get().pwmFreqHz;
  ledcSetup(kCh, s_freqHz, kResBits);
  ledcAttachPin(PIN_MOTOR_PWM, kCh);
  ledcWrite(kCh, 0);
  s_enabled = false;
  s_appliedDuty = 0.0f;
}

void enable() { s_enabled = true; }

void disable() {
  ledcWrite(kCh, 0);
  s_enabled = false;
  s_appliedDuty = 0.0f;
}

bool isEnabled() { return s_enabled; }

void drive(float dutyPct) {
  // Um sentido só: pedido negativo (fechar ativo) não existe neste hardware —
  // vira 0 (coast) e quem fecha é a mola.
  float d = isnan(dutyPct) ? 0.0f : constrain(dutyPct, 0.0f, 100.0f);
  if (!s_enabled) d = 0.0f;
  ledcWrite(kCh, toCounts(d));
  s_appliedDuty = d;
}

float appliedDuty() { return s_appliedDuty; }

void setFrequency(uint32_t hz) {
  s_freqHz = hz;
  // ledcSetup reprograma o timer; duty volta em 0 por segurança — a malha
  // reaplica o esforço no próximo ciclo.
  ledcSetup(kCh, s_freqHz, kResBits);
  ledcWrite(kCh, 0);
  s_appliedDuty = 0.0f;
}

}  // namespace motor
