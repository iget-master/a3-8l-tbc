#pragma once

// AP WiFi + página web de debug/parametrização (WebServer síncrono, porta 80).
//
// Contrato HTTP (page e firmware DEVEM casar):
//   GET  /              → página (PROGMEM, self-contained, pt-BR)
//   GET  /api/status    → JSON: ver, uptimeMs, mode, fault, idle, setpoint,
//                         pos, duty, analogOut, rawTps, cmdDuty, cmdFreq,
//                         cmdPresent, pidP, pidI, pidD, manual,
//                         cal:{state, rest, min, max, valid, learnedMax}
//   GET  /api/params    → JSON com os campos de Settings (mesmos nomes)
//   POST /api/params    → form-encoded, subconjunto dos campos; aplica + salva
//   POST /api/cal       → dispara calibração (efetiva só se em idle)
//   POST /api/manual    → campos: on (0|1), duty (-100..100); rearma keepalive
//   POST /api/defaults  → restaura defaults e salva
namespace webui {
void begin();  // sobe o AP (settings apSsid/apPass) e registra as rotas
void loop();   // handleClient(); chamar a cada loop()
}
