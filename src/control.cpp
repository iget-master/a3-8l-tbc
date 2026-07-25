#include "control.h"

#include <math.h>

#include "analog_out.h"
#include "calibration.h"
#include "hbridge.h"
#include "isense.h"
#include "pid.h"
#include "pins.h"
#include "pwm_input.h"
#include "settings.h"
#include "tps.h"

namespace control {
namespace {

constexpr uint32_t kBootSettleMs = 500;     // espera sinais assentarem no boot
constexpr uint32_t kTpsBadMs = 100;         // implausível contínuo → Fault
constexpr uint32_t kTpsRecoverMs = 500;     // plausível contínuo → recupera
constexpr uint32_t kManualTimeoutMs = 3000; // keepalive do modo manual
constexpr uint32_t kSpOverrideTimeoutMs = 3000; // keepalive do override de setpoint

constexpr const char* kFaultTps = "TPS implausível";
constexpr const char* kFaultNoCal = "sem calibração";
constexpr const char* kFaultMotorShort = "curto no motor";
constexpr const char* kFaultMotorOpen = "motor desconectado";

Mode s_mode = Mode::Boot;
const char* s_faultReason = "";
Pid s_pid;

uint32_t s_bootMs = 0;

// Debounce do idle switch
bool s_idleRaw = false;
bool s_idleStable = false;
uint32_t s_idleChangeMs = 0;

// Base da saída analógica: TPS no instante da soltura do idle (0 = nunca
// capturada nesta sessão → mapeamento fixo até a primeira soltura).
uint16_t s_outBaseCandRaw = 0;  // candidato no flanco cru do switch
uint16_t s_outBaseRaw = 0;      // confirmado pelo debounce
constexpr uint16_t kOutBaseMinSpanCounts = 100;  // span mínimo até o máx

// Agendamento da malha
uint32_t s_lastTickMs = 0;
uint32_t s_lastCycleUs = 0;

// Watchdog de plausibilidade do TPS
bool s_tpsPlaus = true;
uint32_t s_tpsBadSinceMs = 0;
uint32_t s_tpsGoodSinceMs = 0;

// Modo manual
float s_manualDuty = 0.0f;
uint32_t s_manualKickMs = 0;

// Falha de motor (curto/desconexão) é retida até limpeza explícita
bool s_motorFaultLatched = false;

// Override do setpoint pela web (bancada, sem PWM de comando)
bool s_spOverride = false;
float s_spOverrideValue = 0.0f;
uint32_t s_spOverrideKickMs = 0;

// Setpoint efetivo (pós-rampa) — o que o PID segue e a telemetria mostra.
float s_spEffective = 0.0f;

// Telemetria
float s_setpointPct = 0.0f;
float s_positionPct = 0.0f;
float s_analogOutPct = 0.0f;

bool readIdleRaw() {
  const bool low = digitalRead(PIN_IDLE_SW) == LOW;
  return settings::get().idleActiveLow ? low : !low;
}

void updateIdleDebounce(uint32_t now) {
  const bool raw = readIdleRaw();
  if (raw != s_idleRaw) {
    s_idleRaw = raw;
    s_idleChangeMs = now;
    // Flanco cru de soltura: captura o TPS AGORA (o pedal assumiu aqui; no
    // fim do debounce a borboleta já andou). Só vale se o debounce confirmar.
    if (!raw) s_outBaseCandRaw = tps::raw();
  }
  if (raw != s_idleStable &&
      now - s_idleChangeMs >= settings::get().idleDebounceMs) {
    s_idleStable = raw;
    if (!raw) s_outBaseRaw = s_outBaseCandRaw;  // soltura confirmada
  }
}

void updateTpsWatch(uint32_t now) {
  const bool plaus = tps::plausible();
  if (plaus != s_tpsPlaus) {
    s_tpsPlaus = plaus;
    if (plaus) s_tpsGoodSinceMs = now;
    else s_tpsBadSinceMs = now;
  }
}

bool tpsBadPersistent(uint32_t now) {
  return !s_tpsPlaus && now - s_tpsBadSinceMs > kTpsBadMs;
}

bool tpsRecovered(uint32_t now) {
  return s_tpsPlaus && now - s_tpsGoodSinceMs > kTpsRecoverMs;
}

void setFault(const char* reason) {
  if (calibration::running()) calibration::abortRun();
  hbridge::disable();
  s_pid.reset();
  s_faultReason = reason;
  s_mode = Mode::Fault;
}

// Sai para o modo normal reavaliando idle e falhas (motor sempre solto aqui;
// o Run re-habilita a ponte no próprio ciclo).
void enterNormal(uint32_t now) {
  hbridge::disable();
  s_pid.reset();
  s_spEffective = s_positionPct;  // engate suave: rampa parte de onde está
  if (tpsBadPersistent(now)) {
    setFault(kFaultTps);
    return;
  }
  if (!calibration::data().valid) {
    setFault(kFaultNoCal);
    return;
  }
  s_mode = s_idleStable ? Mode::Run : Mode::DriverActive;
}

void runCycle(uint32_t now, float dt) {
  const Settings& cfg = settings::get();
  const bool idle = s_idleStable;

  updateTpsWatch(now);

  // Telemetria base, válida em qualquer modo.
  s_positionPct = calibration::positionPct(tps::raw());

  // Setpoint: normalmente do PWM de comando (0/50/100% → −100/0/+100). Na
  // bancada (sem gerador de PWM) um override pela web injeta o setpoint e conta
  // como "comando presente"; expira sem keepalive (3 s), como o modo manual.
  const bool spOvr =
      s_spOverride && now - s_spOverrideKickMs < kSpOverrideTimeoutMs;
  if (!spOvr) s_spOverride = false;
  const bool cmdPresent = spOvr || pwm_input::signalPresent();
  float spTarget = spOvr ? s_spOverrideValue
                         : (pwm_input::signalPresent()
                                ? 2.0f * pwm_input::dutyPct() - 100.0f
                                : 0.0f);

  // Corpo sem faixa abaixo do repouso (mín == repouso, calibrado assim):
  // pedido negativo vira pedido de repouso — senão o PID estolaria o motor
  // contra o batente tentando fechar o que a mola já fechou.
  {
    const calibration::Data& cd = calibration::data();
    if (cd.valid && cd.minRaw >= cd.restRaw && spTarget < 0.0f) {
      spTarget = 0.0f;
    }
  }

  // Rampa do setpoint: transição rápida vira descida/subida controlada — sem
  // pancada no batente (nem o quique que o PID caçaria) ao voltar ao repouso.
  const float slew = cfg.spSlewPctPerS;
  if (slew > 0.0f && dt > 0.0f) {
    const float maxStep = slew * dt;
    const float delta = spTarget - s_spEffective;
    if (delta > maxStep) s_spEffective += maxStep;
    else if (delta < -maxStep) s_spEffective -= maxStep;
    else s_spEffective = spTarget;
  } else {
    s_spEffective = spTarget;
  }
  s_setpointPct = s_spEffective;

  // TPS implausível derruba qualquer modo — exceto Boot, Fault e Manual:
  // o modo manual de bancada opera em malha aberta e serve justamente para
  // testar o motor sem sensor (a saída dele não depende do TPS).
  if (s_mode != Mode::Boot && s_mode != Mode::Fault && s_mode != Mode::Manual &&
      tpsBadPersistent(now)) {
    setFault(kFaultTps);
  }

  // Corrente da ponte: curto/desconexão derrubam qualquer modo que acione o
  // motor — inclusive Manual e Calibrating (setFault já aborta a calibração).
  // A falha fica retida (sem auto-recuperação): chicote se inspeciona, não se
  // "tenta de novo".
  isense::evaluate(now, hbridge::appliedDuty());
  if (s_mode != Mode::Boot && s_mode != Mode::Fault) {
    if (isense::shortCircuit()) {
      s_motorFaultLatched = true;
      setFault(kFaultMotorShort);
    } else if (isense::openCircuit()) {
      s_motorFaultLatched = true;
      setFault(kFaultMotorOpen);
    }
  }

  switch (s_mode) {
    case Mode::Boot:
      if (now - s_bootMs >= kBootSettleMs) {
        if (idle && calibration::start()) s_mode = Mode::Calibrating;
        else if (calibration::data().valid) enterNormal(now);
        else setFault(kFaultNoCal);
      }
      break;

    case Mode::Calibrating:
      calibration::run(idle);
      if (!calibration::running()) {
        // Done → segue; Failed com calibração antiga válida → segue também
        if (calibration::data().valid) enterNormal(now);
        else setFault(kFaultNoCal);
      }
      break;

    case Mode::Run: {
      if (!idle) {  // pedal acionado: motor solto, PID zerado
        hbridge::disable();
        s_pid.reset();
        s_mode = Mode::DriverActive;
        break;
      }
      if (!cmdPresent) {
        // Failsafe: sem comando (nem PWM nem override de bancada) → motor solto,
        // a mola leva a borboleta ao repouso (não fechar ativamente até o mínimo).
        hbridge::disable();
        s_pid.reset();
        break;
      }
      if (fabsf(s_setpointPct) <= cfg.deadbandPct) {
        // Pedido de repouso: zero absoluto = nenhuma corrente no motor.
        // Quem posiciona é a mola, não o PID.
        hbridge::drive(0.0f);
        s_pid.reset();
        break;
      }
      s_pid.setGains(cfg.kp, cfg.ki, cfg.kd);
      s_pid.setOutputLimit(cfg.maxDutyPct);
      const float err = s_setpointPct - s_positionPct;
      // Zona morta SUAVE (subtrativa): dentro dela o erro efetivo é zero;
      // fora, conta só o excedente — contínuo na borda. A versão dura (erro
      // inteiro ao cruzar a borda) fazia o P saltar a cada excursão de ruído
      // do TPS: ciclo-limite vibrando o setpoint fixo.
      float errEff = 0.0f;
      if (err > cfg.deadbandPct) errEff = err - cfg.deadbandPct;
      else if (err < -cfg.deadbandPct) errEff = err + cfg.deadbandPct;
      const float out = s_pid.update(s_positionPct + errEff, s_positionPct, dt);
      hbridge::enable();
      hbridge::drive(out);
      break;
    }

    case Mode::DriverActive:
      if (idle) {
        // Reengate suave: PID zerado e rampa partindo da posição atual —
        // sem isto, pedal voltando com setpoint alto reengata com erro
        // gigante de uma vez (chute de duty + caçada com a planta ainda
        // em movimento). O re-enable acontece no handler do Run.
        s_pid.reset();
        s_spEffective = s_positionPct;
        s_mode = Mode::Run;
        break;
      }
      calibration::observeRaw(tps::raw(), false);
      break;

    case Mode::Fault:
      hbridge::disable();  // garante estado seguro a cada ciclo
      if (s_motorFaultLatched) break;  // retida: só sai por clearMotorFault()
      if (tpsBadPersistent(now)) s_faultReason = kFaultTps;
      else if (!calibration::data().valid) s_faultReason = kFaultNoCal;
      if (tpsRecovered(now) && calibration::data().valid) enterNormal(now);
      break;

    case Mode::Manual:
      if (now - s_manualKickMs >= kManualTimeoutMs) {  // keepalive expirou
        enterNormal(now);
        break;
      }
      hbridge::enable();
      hbridge::drive(constrain(s_manualDuty, -cfg.maxDutyPct, cfg.maxDutyPct));
      break;
  }

  // Saída analógica mascarada — sempre atualizada
  float outPct = 0.0f;
  if (!idle) {
    const uint16_t raw = tps::raw();
    uint16_t mn =
        cfg.outMinRaw != 0 ? cfg.outMinRaw : calibration::data().minRaw;
    const uint16_t mx =
        cfg.outMaxRaw != 0 ? cfg.outMaxRaw : calibration::learnedMaxRaw();
    // Rebase dinâmico: 0% = posição na soltura do idle (a posição do atuador
    // não vaza pro sinal); 100% segue fixo em mx. Sem span útil até o máx
    // (soltura quase no WOT) ou sem captura ainda → régua fixa.
    if (cfg.outBaseOnRelease && s_outBaseRaw != 0 &&
        (int32_t)mx - (int32_t)s_outBaseRaw >= (int32_t)kOutBaseMinSpanCounts) {
      mn = s_outBaseRaw;
    }
    if (mx > mn) {
      outPct = constrain(100.0f * ((float)raw - (float)mn) / (float)(mx - mn),
                         0.0f, 100.0f);
    }
  }
  s_analogOutPct = outPct;
  analog_out::write(outPct);

  calibration::maybePersistLearned();
}

}  // namespace

void begin() {
  pinMode(PIN_IDLE_SW, INPUT_PULLUP);
  const uint32_t now = millis();
  s_idleRaw = s_idleStable = readIdleRaw();
  s_idleChangeMs = now;
  s_bootMs = now;
  s_lastTickMs = now;
  s_lastCycleUs = micros();
  s_tpsPlaus = tps::plausible();
  s_tpsBadSinceMs = now;
  s_tpsGoodSinceMs = now;
  s_faultReason = "";
  s_motorFaultLatched = false;
  s_spEffective = 0.0f;
  s_outBaseCandRaw = 0;
  s_outBaseRaw = 0;
  s_mode = Mode::Boot;
}

void loop() {
  const uint32_t now = millis();
  updateIdleDebounce(now);  // toda chamada: debounce não depende do loopHz

  const uint16_t hz = settings::get().loopHz > 0 ? settings::get().loopHz : 200;
  uint32_t intervalMs = 1000u / hz;
  if (intervalMs == 0) intervalMs = 1;
  if (now - s_lastTickMs < intervalMs) return;
  s_lastTickMs += intervalMs;  // acumulador: mantém a taxa média
  if (now - s_lastTickMs >= intervalMs) s_lastTickMs = now;  // atrasou: ressincroniza

  const uint32_t nowUs = micros();
  float dt = (float)(nowUs - s_lastCycleUs) * 1e-6f;
  s_lastCycleUs = nowUs;
  if (dt > 0.1f) dt = 0.1f;  // protege o integrador após pausas longas

  runCycle(now, dt);
}

Mode mode() { return s_mode; }

const char* modeName() {
  switch (s_mode) {
    case Mode::Boot: return "Boot";
    case Mode::Calibrating: return "Calibrating";
    case Mode::Run: return "Run";
    case Mode::DriverActive: return "DriverActive";
    case Mode::Fault: return "Fault";
    case Mode::Manual: return "Manual";
  }
  return "?";
}

const char* faultReason() {
  return s_mode == Mode::Fault ? s_faultReason : "";
}

float setpointPct() { return s_setpointPct; }
float positionPct() { return s_positionPct; }
float appliedDutyPct() { return hbridge::appliedDuty(); }
float analogOutPct() { return s_analogOutPct; }
bool idleActive() { return s_idleStable; }
float pidP() { return s_pid.pTerm(); }
float pidI() { return s_pid.iTerm(); }
float pidD() { return s_pid.dTerm(); }

void requestCalibration() {
  if (!s_idleStable || s_mode == Mode::Calibrating) return;
  if (!calibration::start()) return;
  hbridge::disable();  // a calibração assume o motor a partir daqui
  s_pid.reset();
  s_mode = Mode::Calibrating;
}

void setManual(bool on, float dutyPct) {
  if (!on) {
    if (s_mode == Mode::Manual) enterNormal(millis());
    return;
  }
  if (s_mode == Mode::Calibrating) return;  // bancada não interrompe calibração
  const float lim = settings::get().maxDutyPct;
  s_manualDuty = isnan(dutyPct) ? 0.0f : constrain(dutyPct, -lim, lim);
  s_manualKickMs = millis();  // rearma o keepalive
  if (s_mode != Mode::Manual) s_pid.reset();
  s_mode = Mode::Manual;
  hbridge::enable();
  hbridge::drive(s_manualDuty);
}

bool manualActive() { return s_mode == Mode::Manual; }

void setSetpointOverride(bool on, float setpointPct) {
  if (!on) {
    s_spOverride = false;
    return;
  }
  s_spOverrideValue =
      isnan(setpointPct) ? 0.0f : constrain(setpointPct, -100.0f, 100.0f);
  s_spOverrideKickMs = millis();  // rearma o keepalive
  s_spOverride = true;
}

bool setpointOverrideActive() {
  return s_spOverride && millis() - s_spOverrideKickMs < kSpOverrideTimeoutMs;
}

void clearMotorFault() {
  // Solta o latch; o caso Fault reavalia TPS/calibração e recupera sozinho se
  // estiver tudo são (mesmo caminho da recuperação de TPS).
  s_motorFaultLatched = false;
}

bool motorFaultLatched() { return s_motorFaultLatched; }

}  // namespace control
