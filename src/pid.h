#pragma once

// PID básico: derivada sobre a medição (sem "derivative kick") e anti-windup
// por integração condicional + clamp do integrador nos limites de saída.
class Pid {
 public:
  void setGains(float kp, float ki, float kd);
  void setOutputLimit(float maxAbs);  // saturação simétrica (±maxAbs)
  void reset();                       // zera integrador e histórico da derivada
  float update(float setpoint, float measurement, float dtSec);
  float pTerm() const { return p_; }
  float iTerm() const { return i_; }
  float dTerm() const { return d_; }

 private:
  float kp_ = 0.0f, ki_ = 0.0f, kd_ = 0.0f;
  float limit_ = 100.0f;
  float p_ = 0.0f, i_ = 0.0f, d_ = 0.0f;
  float lastMeas_ = 0.0f;
  bool first_ = true;
};
