#include "analog_out.h"

#include <Arduino.h>

#include "pins.h"

namespace analog_out {

void begin() {
  dacWrite(PIN_ANALOG_OUT, 0);  // já sobe indicando 0% (estado de idle)
}

void write(float pct) {
  const float p = isnan(pct) ? 0.0f : constrain(pct, 0.0f, 100.0f);
  dacWrite(PIN_ANALOG_OUT, (uint8_t)(p * 255.0f / 100.0f + 0.5f));
}

}  // namespace analog_out
