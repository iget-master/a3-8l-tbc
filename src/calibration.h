#pragma once
#include <Arduino.h>

// Auto calibração do sensor de posição: encontra repouso (motor solto),
// abertura máxima (drive +calDrivePct) e mínima (drive -calDrivePct).
// Máquina de estados não-bloqueante: comanda hbridge e lê tps diretamente.
// Persistência na NVS (namespace "cal"). Se o idle for solto durante uma fase
// ativa, aborta e mantém a última calibração válida.
namespace calibration {

enum class State : uint8_t {
  Inactive,     // nunca iniciada nesta sessão
  SettleStart,  // espera inicial (mecânica assentar) antes de medir o repouso
  Rest,         // motor coast — registra repouso
  OpenMax,      // drive no sentido abrir — registra máximo
  CloseMin,     // drive no sentido fechar — registra mínimo
  Finish,       // motor solto, valida e salva
  Done,
  Failed,
};

struct Data {
  uint16_t restRaw = 0;
  uint16_t minRaw = 0;
  uint16_t maxRaw = 0;
  bool valid = false;
};

void begin();               // carrega calibração e máx aprendido da NVS
bool start();               // arma a máquina (chamador garante idle ativo)
void abortRun();            // aborta, motor coast, restaura última calibração
void run(bool idleActive);  // avança a máquina; chamar a cada loop() enquanto running()
bool running();
State state();
const char* stateName();
const Data& data();

// Normalização com sinal pela calibração: −100% = minRaw, 0% = restRaw
// (repouso da mola), +100% = maxRaw. Linear por partes — rampas distintas
// abaixo e acima do repouso (o repouso raramente é o centro geométrico).
// Satura em ±100%: leitura além do máx/mín calibrado não extrapola a faixa.
float positionPct(uint16_t rawValue);

// Máximo do TPS aprendido fora de idle (usado no mapeamento da saída analógica).
uint16_t learnedMaxRaw();
void observeRaw(uint16_t rawValue, bool idleActive);  // aprende se outAutoLearnMax
void maybePersistLearned();  // salva na NVS com throttling (>=60 s entre gravações)
}
