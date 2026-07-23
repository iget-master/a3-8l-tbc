#pragma once

// Atualização de firmware pela rede (espota/ArduinoOTA, porta 3232) + mDNS:
// o aparelho atende por a3-tbc.local (página e espota).
// Senha = apPass vigente no boot (a mesma do AP). Uso com PlatformIO:
//   pio run -e esp32dev_ota -t upload --upload-port a3-tbc.local
// Ao iniciar uma gravação a ponte H é desabilitada (o loop fica bloqueado
// durante a transferência) e o task WDT é alimentado no progresso.
// O upload pelo navegador (POST /update) fica no webui, com os mesmos cuidados.
namespace ota {
void begin();   // chamar depois do webui::begin() (WiFi já configurado)
void handle();  // chamar a cada loop()
}
