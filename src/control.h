#pragma once
#include <Arduino.h>

// Malha principal: debounce do idle switch, setpoint (PWM de comando → % da
// faixa calibrada), PID, failsafes, saída analógica mascarada e modos.
//
// Modos:
//   Boot         — espera sinais assentarem (~500 ms) e decide calibrar ou não
//   Calibrating  — auto calibração em andamento
//   Run          — em idle: PID atuando no motor
//   DriverActive — pedal acionado: motor coast/desabilitado, PID resetado
//   Fault        — TPS implausível ou sem calibração válida: motor desabilitado
//   Manual       — bancada (via web): duty direto; expira sem keepalive (3 s)
//
// Saída analógica (sempre atualizada): em idle → 0%; fora de idle → TPS
// normalizado pela faixa [outMinRaw..outMaxRaw] (0 = automático pela calibração
// e máx aprendido).
namespace control {

enum class Mode : uint8_t { Boot, Calibrating, Run, DriverActive, Fault, Manual };

void begin();
void loop();  // roda a malha a settings loopHz; chamar sempre no loop()

Mode mode();
const char* modeName();
const char* faultReason();  // "" quando sem falha

// Telemetria para a web
float setpointPct();
float positionPct();      // normalizada pela calibração (pode sair de 0..100)
float appliedDutyPct();
float analogOutPct();     // valor mascarado enviado ao DAC
bool idleActive();        // já com debounce
float pidP();
float pidI();
float pidD();

// Ações da web
void requestCalibration();               // só inicia se idle ativo
void setManual(bool on, float dutyPct);  // keepalive: rechamadas renovam o prazo
bool manualActive();
}
