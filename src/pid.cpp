#include "pid.h"

#include <math.h>

namespace {

float clampAbs(float v, float lim) {
  return v > lim ? lim : (v < -lim ? -lim : v);
}

}  // namespace

void Pid::setGains(float kp, float ki, float kd) {
  kp_ = kp;
  ki_ = ki;
  kd_ = kd;
}

void Pid::setOutputLimit(float maxAbs) { limit_ = fabsf(maxAbs); }

void Pid::reset() {
  p_ = 0.0f;
  i_ = 0.0f;
  d_ = 0.0f;
  lastMeas_ = 0.0f;
  first_ = true;
}

float Pid::update(float setpoint, float measurement, float dtSec) {
  if (dtSec <= 0.0f) {
    // dt inválido: repete a última saída sem tocar no estado
    return clampAbs(p_ + i_ - d_, limit_);
  }

  const float e = setpoint - measurement;
  p_ = kp_ * e;

  // Derivada na medição (sem "derivative kick" na troca de setpoint).
  if (first_) {
    d_ = 0.0f;
    first_ = false;
  } else {
    d_ = kd_ * (measurement - lastMeas_) / dtSec;
  }
  lastMeas_ = measurement;

  // Anti-windup: só integra se a saída não estiver saturada no sentido do erro.
  const float out = p_ + i_ - d_;
  const bool satSameDir =
      (out >= limit_ && e > 0.0f) || (out <= -limit_ && e < 0.0f);
  if (!satSameDir) i_ = clampAbs(i_ + ki_ * e * dtSec, limit_);

  return clampAbs(p_ + i_ - d_, limit_);
}
