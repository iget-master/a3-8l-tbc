#pragma once
#include <Arduino.h>

// Acionamento do motor via LEDC — UM sentido (abrir). Duty 0..100%: 0 = coast
// (a mola leva a borboleta ao repouso — fechar é sempre passivo, ~0,3 s de
// curso completo medido em bancada). Estágio de potência: P-FET high-side
// (docs/hardware.md); PWM baixo no boot = motor solto (estado seguro).
namespace motor {
void begin();                  // configura LEDC, saída em 0, desabilitado
void enable();                 // habilita o acionamento (gate por software)
void disable();                // duty 0 — estado seguro
bool isEnabled();
void drive(float dutyPct);     // 0..100 (negativo/NaN viram 0 = coast)
float appliedDuty();           // último duty efetivamente aplicado (0..100)
void setFrequency(uint32_t hz);  // reconfigura o canal LEDC (chamada pela web)
}
