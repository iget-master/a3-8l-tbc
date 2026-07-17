#pragma once
#include <Arduino.h>

// Medição do duty cycle do sinal PWM de comando (PIN_CMD_PWM) via ISR em CHANGE.
// Sem bordas por cmdTimeoutMs: nível alto → 100% (se cmdStuckHighIs100) e o
// sinal segue "presente"; nível baixo → 0% e sinal "ausente" (indistinguível
// de desconectado — e o failsafe trata ambos igual: setpoint 0).
namespace pwm_input {
void begin();
void poll();            // trata timeout/nível estático; chamar a cada loop()
float dutyPct();        // 0..100
float freqHz();         // 0 quando sem bordas recentes
bool signalPresent();
uint32_t lastEdgeAgeMs();
}
