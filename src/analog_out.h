#pragma once

// Saída analógica de posição (PIN_ANALOG_OUT, DAC1): 0..100% → 0..3,3 V (8 bits).
namespace analog_out {
void begin();
void write(float pct);  // clamp 0..100
}
