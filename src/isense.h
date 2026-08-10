#pragma once
#include <Arduino.h>

// Sensor de corrente do motor (PIN_ISENSE, ADC1) — DORMENTE no hardware atual
// (acionamento de um sentido sem sensoriamento; isenseEnabled desligado).
// Preparado para um shunt low-side + amplificador (ex.: 20 mΩ + INA180A1) no
// retorno de GND do motor. A média lida escala com o duty do PWM; evaluate()
// normaliza para 100% de duty antes de comparar limiares.
//
// Condições (sustentadas pelos tempos configurados; avaliadas apenas com
// |duty| >= isMinDutyPct e isenseEnabled):
//   stalled()      — corrente de fim de curso (motor no batente); informativo
//   shortCircuit() — curto no motor/chicote (ou proteção do driver atuando)
//   openCircuit()  — motor desconectado (corrente ~zero sob drive)
// O latch de falha (e a decisão de desligar a ponte) fica no control.
namespace isense {
void begin();
void poll();  // uma amostra filtrada por chamada; chamar a cada loop()
void evaluate(uint32_t nowMs, float appliedDutyPct);  // chamar no tick da malha
uint16_t raw();      // 0..4095 filtrado (média no pino, sem normalizar)
float estRaw100();   // counts normalizados p/ 100% de duty (0 = não avaliável)
bool stalled();
bool shortCircuit();
bool openCircuit();
}
