#include "ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

#include "hbridge.h"
#include "settings.h"

namespace ota {

void begin() {
  ArduinoOTA.setHostname("a3-tbc");
  // Mesma credencial do AP: uma senha só para rede e gravação.
  ArduinoOTA.setPassword(settings::get().apPass);
  ArduinoOTA.onStart([]() {
    hbridge::disable();  // motor solto: a transferência bloqueia o loop()
    Serial.println("[ota] espota: iniciando gravação");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    (void)done;
    (void)total;
    feedLoopWDT();  // gravação longa dentro de handle(): segura o task WDT
  });
  ArduinoOTA.onEnd([]() { Serial.println("[ota] espota: concluído, reiniciando"); });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("[ota] espota: erro %d\n", (int)err);
  });
  // begin() registra o hostname no mDNS (http://a3-tbc.local); o addService
  // anuncia também a página para descoberta de serviços na rede.
  ArduinoOTA.begin();
  MDNS.addService("http", "tcp", 80);
}

void handle() { ArduinoOTA.handle(); }

}  // namespace ota
