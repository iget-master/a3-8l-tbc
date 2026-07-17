#include "hbridge.h"

#include "pins.h"
#include "settings.h"

namespace hbridge {
namespace {

constexpr uint8_t kChOpen = 0;   // canal LEDC de IN1 — sentido abrir
constexpr uint8_t kChClose = 1;  // canal LEDC de IN2 — sentido fechar
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
  // EN em LOW antes de qualquer PWM: driver dormindo durante o setup.
  // digitalWrite antes do pinMode pré-carrega o latch — sem glitch alto.
  if (PIN_HB_EN >= 0) {
    digitalWrite(PIN_HB_EN, LOW);
    pinMode(PIN_HB_EN, OUTPUT);
  }
  s_freqHz = settings::get().pwmFreqHz;
  ledcSetup(kChOpen, s_freqHz, kResBits);
  ledcSetup(kChClose, s_freqHz, kResBits);
  ledcAttachPin(PIN_HB_IN1, kChOpen);
  ledcAttachPin(PIN_HB_IN2, kChClose);
  ledcWrite(kChOpen, 0);
  ledcWrite(kChClose, 0);
  s_enabled = false;
  s_appliedDuty = 0.0f;
}

void enable() {
  if (PIN_HB_EN >= 0) digitalWrite(PIN_HB_EN, HIGH);
  s_enabled = true;
}

void disable() {
  ledcWrite(kChOpen, 0);
  ledcWrite(kChClose, 0);
  if (PIN_HB_EN >= 0) digitalWrite(PIN_HB_EN, LOW);
  s_enabled = false;
  s_appliedDuty = 0.0f;
}

bool isEnabled() { return s_enabled; }

void drive(float dutyPct) {
  float d = isnan(dutyPct) ? 0.0f : constrain(dutyPct, -100.0f, 100.0f);
  if (!s_enabled) d = 0.0f;
  // Zera o lado oposto antes de energizar o novo: sem sobreposição na troca
  // de sentido.
  if (d > 0.0f) {
    ledcWrite(kChClose, 0);
    ledcWrite(kChOpen, toCounts(d));
  } else if (d < 0.0f) {
    ledcWrite(kChOpen, 0);
    ledcWrite(kChClose, toCounts(-d));
  } else {  // coast — mola leva ao repouso
    ledcWrite(kChOpen, 0);
    ledcWrite(kChClose, 0);
  }
  s_appliedDuty = d;
}

float appliedDuty() { return s_appliedDuty; }

void setFrequency(uint32_t hz) {
  s_freqHz = hz;
  // ledcSetup reprograma o timer; duty volta em 0 por segurança — a malha
  // reaplica o esforço no próximo ciclo.
  ledcSetup(kChOpen, s_freqHz, kResBits);
  ledcSetup(kChClose, s_freqHz, kResBits);
  ledcWrite(kChOpen, 0);
  ledcWrite(kChClose, 0);
  s_appliedDuty = 0.0f;
}

}  // namespace hbridge
