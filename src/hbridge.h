#pragma once
#include <Arduino.h>

// Ponte H via LEDC (core arduino-esp32 2.x: ledcSetup/ledcAttachPin).
// Duty com sinal: >0 abre (PWM em IN1), <0 fecha (PWM em IN2), 0 = coast
// (ambos em nível baixo — a mola leva a borboleta ao repouso).
namespace hbridge {
void begin();                  // configura LEDC, EN desabilitado, saídas em 0
void enable();                 // habilita EN (se houver)
void disable();                // duty 0 + EN baixo — estado seguro
bool isEnabled();
void drive(float dutyPct);     // -100..+100 (clamp interno); vira coast se desabilitado
float appliedDuty();           // último duty com sinal efetivamente aplicado
void setFrequency(uint32_t hz);  // reconfigura os canais LEDC (chamada pela web)
}
