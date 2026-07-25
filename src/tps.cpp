#include "tps.h"

#include "pins.h"
#include "settings.h"

namespace tps {
namespace {

// Canal de leitura filtrada (mediana + EMA) — pista 1 e, opcional, pista 2.
struct Chan {
  uint16_t ring[TPS_MEDIAN_MAX];
  uint8_t idx = 0;
  uint8_t activeN = 0;  // tamanho da janela em uso — detecta troca pela web
  float ema = 0.0f;
};

Chan s_ch1;
Chan s_ch2;

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
void chanFill(Chan& c, uint16_t v, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) c.ring[i] = v;
  c.idx = 0;
  c.activeN = n;
}

void chanBegin(Chan& c, int pin) {
  analogSetPinAttenuation(pin, ADC_11db);
  // Pré-enche o filtro com uma leitura real: sem rampa a partir de zero
  // (que pareceria falha de TPS nos primeiros ciclos).
  const uint16_t v = (uint16_t)analogRead(pin);
  chanFill(c, v, medianSamples());
  c.ema = (float)v;
}

void chanPoll(Chan& c, int pin) {
  const uint8_t n = medianSamples();
  const uint16_t sample = (uint16_t)analogRead(pin);
  // Tamanho da janela mudou pela web → recomeça limpo nesta amostra.
  if (n != c.activeN) chanFill(c, sample, n);
  c.ring[c.idx] = sample;
  c.idx = (uint8_t)((c.idx + 1) % n);
  c.ema += settings::get().tpsEmaAlpha * ((float)medianN(c.ring, n) - c.ema);
}

// Pista invertida (tensão maior fechado): espelha aqui, na fonte — todo o
// resto do firmware enxerga raw crescendo ao abrir.
uint16_t chanRaw(const Chan& c, bool invert) {
  const uint16_t v = (uint16_t)(c.ema + 0.5f);
  return invert ? (uint16_t)(4095 - v) : v;
}

}  // namespace

void begin() {
  analogReadResolution(12);  // global — vale também para o isense (mesmo ADC1)
  chanBegin(s_ch1, PIN_TPS);
  chanBegin(s_ch2, PIN_TPS2);  // pré-carrega mesmo desabilitada: habilitar pela web vale na hora
}

void poll() {
  chanPoll(s_ch1, PIN_TPS);
  if (settings::get().tps2Enabled) chanPoll(s_ch2, PIN_TPS2);
}

uint16_t raw() { return chanRaw(s_ch1, settings::get().tpsInvert != 0); }

uint16_t raw2() { return chanRaw(s_ch2, settings::get().tps2Invert != 0); }

bool plausible() {
  const Settings& cfg = settings::get();
  const uint16_t r = raw();
  return r >= cfg.tpsFaultLowRaw && r <= cfg.tpsFaultHighRaw;
}

}  // namespace tps
