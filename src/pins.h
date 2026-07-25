#pragma once

// Pinout do ESP32 DevKit V1 — espelho de docs/hardware.md (manter sincronizado).
// ADC2 é inutilizável com WiFi ativo: analógicas só em ADC1 (GPIO32–39).
constexpr int PIN_TPS        = 34;  // ADC1_CH6 — potenciômetro de posição (somente entrada)
constexpr int PIN_TPS2       = 36;  // ADC1_CH0 — pista 2 contraposta do TPS (opcional, verificação cruzada)
constexpr int PIN_CMD_PWM    = 35;  // entrada PWM de comando (somente entrada, já condicionada)
constexpr int PIN_IDLE_SW    = 32;  // switch de idle — pull-up interno, fecha p/ GND em idle
constexpr int PIN_HB_IN1     = 18;  // ponte H IN1 — sentido abrir
constexpr int PIN_HB_IN2     = 19;  // ponte H IN2 — sentido fechar
constexpr int PIN_HB_EN      = 21;  // ponte H enable/sleep (-1 se o driver não tiver)
constexpr int PIN_ISENSE     = 33;  // ADC1_CH5 — corrente da ponte (R_IS+L_IS do IBT-2)
constexpr int PIN_ANALOG_OUT = 25;  // DAC1 — posição mascarada 0–100%
constexpr int PIN_LED        = 2;   // LED onboard (status)
