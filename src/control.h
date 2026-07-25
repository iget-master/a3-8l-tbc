#pragma once
#include <Arduino.h>

// Malha principal: debounce do idle switch, setpoint (PWM de comando), PID,
// failsafes, saída analógica mascarada e modos.
//
// Semântica de posição/setpoint: −100..+100%, com 0 = repouso da mola.
// O duty do PWM de comando (0..100%) mapeia linearmente: 0% → −100 (fechar
// todo), 50% → 0 (repouso), 100% → +100 (abrir todo). Pedido dentro da zona
// morta em torno de 0 → coast (nenhuma corrente no motor; a mola posiciona).
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
float setpointPct();      // −100..+100 (0 = repouso)
float positionPct();      // normalizada pela calibração (pode sair de ±100)
float pos2Pct();          // posição pela pista 2 (0 se desabilitada/inválida)
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

// Override do setpoint pela web (bancada, sem gerador de PWM de comando):
// injeta o setpoint no lugar do sinal de comando para testar a malha fechada
// (PID). Só tem efeito no modo Run; keepalive de 3 s (rechamadas renovam), como
// o modo manual.
void setSetpointOverride(bool on, float setpointPct);
bool setpointOverrideActive();

// Falha de motor (curto/desconexão, via isense) fica RETIDA: não há
// auto-recuperação — limpar pela web (ou reboot) após inspecionar o chicote.
void clearMotorFault();
bool motorFaultLatched();
}
