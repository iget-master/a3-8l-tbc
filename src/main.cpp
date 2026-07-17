#include <Arduino.h>

#include "analog_out.h"
#include "calibration.h"
#include "control.h"
#include "hbridge.h"
#include "pins.h"
#include "pwm_input.h"
#include "settings.h"
#include "tps.h"
#include "version.h"
#include "webui.h"

namespace {

// Padrão de pisca do LED de status por modo (sem delay).
bool ledOn(uint32_t ms) {
  switch (control::mode()) {
    case control::Mode::Run: return ms % 1000 < 500;        // 1 Hz
    case control::Mode::Calibrating: return ms % 200 < 100; // 5 Hz
    case control::Mode::Fault: return true;                 // aceso fixo
    case control::Mode::DriverActive: {                     // 2 piscadas curtas/s
      const uint32_t p = ms % 1000;
      return p < 100 || (p >= 200 && p < 300);
    }
    case control::Mode::Manual: return ms % 100 < 50;       // 10 Hz
    case control::Mode::Boot: break;
  }
  return false;  // Boot: apagado
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("A3-8L-TBC — controlador de marcha lenta (Audi A3 8L 1.8T)");
  Serial.println("Firmware v" FW_VERSION);

  // Watchdog de tarefa na task do loop(): travamento → panic/reset, e o boot
  // reinicia com a ponte H desabilitada (estado seguro). Timeout padrão ~5 s.
  enableLoopWDT();

  settings::begin();
  tps::begin();
  pwm_input::begin();
  hbridge::begin();
  analog_out::begin();
  calibration::begin();
  control::begin();
  webui::begin();

  pinMode(PIN_LED, OUTPUT);
}

void loop() {
  tps::poll();
  pwm_input::poll();
  control::loop();
  webui::loop();
  digitalWrite(PIN_LED, ledOn(millis()) ? HIGH : LOW);
}
