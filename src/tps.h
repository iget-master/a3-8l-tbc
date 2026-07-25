#pragma once
#include <Arduino.h>

// Leitura filtrada do potenciômetro de posição (PIN_TPS, ADC1, atenuação 11 dB).
// Filtro: mediana (nº ímpar de amostras) + média exponencial (α); ambos
// configuráveis pela web (settings tpsMedianSamples / tpsEmaAlpha). Pista
// invertida (tensão maior fechado): settings tpsInvert espelha o raw() na
// fonte (4095 − raw).
namespace tps {
void begin();
void poll();          // uma amostra por chamada; chamar a cada loop()
uint16_t raw();       // pista 1: 0..4095 filtrado (espelhado se tpsInvert)
uint16_t raw2();      // pista 2 (PIN_TPS2): filtrado (espelhado se tps2Invert)
bool plausible();     // pista 1 dentro de [tpsFaultLowRaw, tpsFaultHighRaw]
}
