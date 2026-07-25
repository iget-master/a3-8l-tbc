#include "tps.h"

#include "pins.h"
#include "settings.h"

namespace tps {
namespace {

uint16_t s_ring[TPS_MEDIAN_MAX];
uint8_t s_idx = 0;
uint8_t s_activeN = 0;  // tamanho da janela em uso — detecta troca pela web
float s_ema = 0.0f;

// Nº de amostras da mediana em uso, saneado para o tamanho do buffer.
uint8_t medianSamples() {
  uint8_t n = settings::get().tpsMedianSamples;
  if (n < 1) n = 1;
  if (n > TPS_MEDIAN_MAX) n = TPS_MEDIAN_MAX;
  return n;
}

// Mediana de n amostras (1..TPS_MEDIAN_MAX) por insertion sort.
uint16_t medianN(const uint16_t* src, uint8_t n) {
  uint16_t v[TPS_MEDIAN_MAX];
  memcpy(v, src, (size_t)n * sizeof(uint16_t));
  for (uint8_t i = 1; i < n; i++) {
    const uint16_t key = v[i];
    int8_t j = (int8_t)(i - 1);
    while (j >= 0 && v[j] > key) {
      v[j + 1] = v[j];
      j--;
    }
    v[j + 1] = key;
  }
  return v[n / 2];
}

// Preenche a janela ativa com um valor (boot e ao trocar o tamanho pela web).
void fillWindow(uint16_t v, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) s_ring[i] = v;
  s_idx = 0;
  s_activeN = n;
}

}  // namespace

void begin() {
  analogReadResolution(12);  // global — vale também para o isense (mesmo ADC1)
  analogSetPinAttenuation(PIN_TPS, ADC_11db);
  // Pré-enche o filtro com uma leitura real: sem rampa a partir de zero
  // (que pareceria falha de TPS nos primeiros ciclos).
  const uint16_t v = (uint16_t)analogRead(PIN_TPS);
  fillWindow(v, medianSamples());
  s_ema = (float)v;
}

void poll() {
  const uint8_t n = medianSamples();
  const uint16_t sample = (uint16_t)analogRead(PIN_TPS);
  // Tamanho da janela mudou pela web → recomeça limpo nesta amostra.
  if (n != s_activeN) fillWindow(sample, n);
  s_ring[s_idx] = sample;
  s_idx = (uint8_t)((s_idx + 1) % n);
  const float alpha = settings::get().tpsEmaAlpha;
  s_ema += alpha * ((float)medianN(s_ring, n) - s_ema);
}

uint16_t raw() {
  const uint16_t v = (uint16_t)(s_ema + 0.5f);
  // Pista invertida (tensão maior fechado): espelha aqui, na fonte — todo o
  // resto do firmware enxerga raw crescendo ao abrir.
  return settings::get().tpsInvert ? (uint16_t)(4095 - v) : v;
}

bool plausible() {
  const Settings& cfg = settings::get();
  const uint16_t r = raw();
  return r >= cfg.tpsFaultLowRaw && r <= cfg.tpsFaultHighRaw;
}

}  // namespace tps
