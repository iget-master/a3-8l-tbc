#include "tps.h"

#include "pins.h"
#include "settings.h"

namespace tps {
namespace {

constexpr uint8_t kSamples = 5;
constexpr float kAlpha = 0.2f;  // EMA: mediana mata spikes, EMA suaviza

uint16_t s_ring[kSamples];
uint8_t s_idx = 0;
float s_ema = 0.0f;

uint16_t median5(const uint16_t* src) {
  uint16_t v[kSamples];
  memcpy(v, src, sizeof(v));
  for (uint8_t i = 1; i < kSamples; i++) {  // insertion sort de 5
    const uint16_t key = v[i];
    int8_t j = (int8_t)(i - 1);
    while (j >= 0 && v[j] > key) {
      v[j + 1] = v[j];
      j--;
    }
    v[j + 1] = key;
  }
  return v[2];
}

}  // namespace

void begin() {
  analogReadResolution(12);  // global — única entrada analógica do projeto
  analogSetPinAttenuation(PIN_TPS, ADC_11db);
  // Pré-enche o filtro com uma leitura real: sem rampa a partir de zero
  // (que pareceria falha de TPS nos primeiros ciclos).
  const uint16_t v = (uint16_t)analogRead(PIN_TPS);
  for (uint8_t i = 0; i < kSamples; i++) s_ring[i] = v;
  s_ema = (float)v;
}

void poll() {
  s_ring[s_idx] = (uint16_t)analogRead(PIN_TPS);
  s_idx = (uint8_t)((s_idx + 1) % kSamples);
  s_ema += kAlpha * ((float)median5(s_ring) - s_ema);
}

uint16_t raw() { return (uint16_t)(s_ema + 0.5f); }

bool plausible() {
  const Settings& cfg = settings::get();
  const uint16_t r = raw();
  return r >= cfg.tpsFaultLowRaw && r <= cfg.tpsFaultHighRaw;
}

}  // namespace tps
