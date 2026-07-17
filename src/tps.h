#pragma once
#include <Arduino.h>

// Leitura filtrada do potenciômetro de posição (PIN_TPS, ADC1, atenuação 11 dB).
// Filtro: mediana de 5 amostras + média exponencial.
namespace tps {
void begin();
void poll();          // uma amostra por chamada; chamar a cada loop()
uint16_t raw();       // 0..4095 filtrado
bool plausible();     // dentro de [tpsFaultLowRaw, tpsFaultHighRaw]
}
