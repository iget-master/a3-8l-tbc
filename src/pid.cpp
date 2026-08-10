#include "pid.h"

#include <math.h>

void Pid::setGains(float kp, float ki, float kd) {
  kp_ = kp;
  ki_ = ki;
  kd_ = kd;
}

void Pid::setOutputRange(float outMin, float outMax) {
  outMin_ = outMin;
  outMax_ = outMax;
  if (outMin_ > outMax_) outMin_ = outMax_;
}

float Pid::clampOut(float v) const {
  return v > outMax_ ? outMax_ : (v < outMin_ ? outMin_ : v);
}

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
    return clampOut(p_ + i_ - d_);
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

  // Anti-windup: só integra se a saída não estiver saturada no sentido do
  // erro; o integrador vive dentro da própria faixa de saída.
  const float out = p_ + i_ - d_;
  const bool satSameDir =
      (out >= outMax_ && e > 0.0f) || (out <= outMin_ && e < 0.0f);
  if (!satSameDir) i_ = clampOut(i_ + ki_ * e * dtSec);

  return clampOut(p_ + i_ - d_);
}
