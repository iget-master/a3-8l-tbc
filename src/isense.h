#pragma once
#include <Arduino.h>

// Sensor de corrente da ponte H (R_IS+L_IS do IBT-2/BTS7960 → PIN_ISENSE,
// ADC1). O IS espelha a corrente do motor (I_IS = I_motor/8500) e, em falha do
// driver (curto/sobrecorrente), injeta ~4,5 mA fixos — assinatura que satura o
// ADC. Como o IS só conduz com o high-side ligado, a média lida escala com o
// duty; evaluate() normaliza para 100% de duty antes de comparar limiares.
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
