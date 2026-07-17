#include "calibration.h"

#include <Preferences.h>

#include "hbridge.h"
#include "settings.h"
#include "tps.h"

namespace calibration {
namespace {

constexpr const char* kNamespace = "cal";
constexpr const char* kKeyRest = "rest";
constexpr const char* kKeyMin = "min";
constexpr const char* kKeyMax = "max";
constexpr const char* kKeyValid = "valid";
constexpr const char* kKeyLearnedMax = "lmax";

constexpr uint32_t kSettleStartMs = 300;           // assentamento mecânico inicial
constexpr uint16_t kLearnMarginCounts = 8;         // margem anti-ruído do máx aprendido
constexpr uint16_t kPersistMinDeltaCounts = 16;    // grava só se mudou o bastante...
constexpr uint32_t kPersistMinIntervalMs = 60000;  // ...e com folga entre gravações

State s_state = State::Inactive;
Data g_data;

uint32_t s_phaseStartMs = 0;

// Candidatos da corrida atual — g_data só muda se a validação passar.
uint16_t s_candRest = 0;
uint16_t s_candMin = 0;
uint16_t s_candMax = 0;

// Janela de estabilidade: recomeça sempre que a leitura sai da banda; estável
// quando uma mesma janela sobrevive calSettleMs contínuos.
uint32_t s_winStartMs = 0;
uint16_t s_winMin = 0;
uint16_t s_winMax = 0;
uint64_t s_winSum = 0;
uint32_t s_winCount = 0;

uint16_t s_learnedMax = 4095;
uint16_t s_persistedLearned = 4095;
uint32_t s_lastPersistMs = 0;

void resetStability() { s_winCount = 0; }

void feedStability(uint16_t rawValue, uint32_t now, uint16_t band) {
  if (s_winCount > 0) {
    if (rawValue < s_winMin) s_winMin = rawValue;
    if (rawValue > s_winMax) s_winMax = rawValue;
    if ((uint16_t)(s_winMax - s_winMin) <= band) {
      s_winSum += rawValue;
      s_winCount++;
      return;
    }
  }
  // Primeira amostra ou saiu da banda: janela recomeça na amostra atual.
  s_winStartMs = now;
  s_winMin = s_winMax = rawValue;
  s_winSum = rawValue;
  s_winCount = 1;
}

bool stabilityReached(uint32_t now, uint16_t settleMs) {
  return s_winCount > 0 && now - s_winStartMs >= settleMs;
}

uint16_t stabilityAverage() {
  return (uint16_t)((s_winSum + s_winCount / 2) / s_winCount);
}

void enterPhase(State st, uint32_t now) {
  s_state = st;
  s_phaseStartMs = now;
  resetStability();
}

// Checagem estrutural (mín < repouso < máx): protege positionPct de lixo na NVS.
bool structurallyValid(const Data& d) {
  return d.minRaw < d.restRaw && d.restRaw < d.maxRaw;
}

void loadDataFromNvs() {
  Data d;
  Preferences prefs;
  if (prefs.begin(kNamespace, /*readOnly=*/true)) {
    d.restRaw = prefs.getUShort(kKeyRest, 0);
    d.minRaw = prefs.getUShort(kKeyMin, 0);
    d.maxRaw = prefs.getUShort(kKeyMax, 0);
    d.valid = prefs.getBool(kKeyValid, false);
    prefs.end();
  }
  g_data = (d.valid && structurallyValid(d)) ? d : Data{};
}

void persistData() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) return;
  // valid por último: gravação interrompida nunca deixa mistura antiga/nova válida.
  prefs.putBool(kKeyValid, false);
  prefs.putUShort(kKeyRest, g_data.restRaw);
  prefs.putUShort(kKeyMin, g_data.minRaw);
  prefs.putUShort(kKeyMax, g_data.maxRaw);
  prefs.putUShort(kKeyLearnedMax, s_learnedMax);
  prefs.putBool(kKeyValid, true);
  prefs.end();
}

void finishPhase(const Settings& cfg) {
  if (s_candMin < s_candRest && s_candRest < s_candMax &&
      (uint16_t)(s_candMax - s_candMin) >= cfg.calMinRangeCounts) {
    g_data.restRaw = s_candRest;
    g_data.minRaw = s_candMin;
    g_data.maxRaw = s_candMax;
    g_data.valid = true;
    // Reconcilia o máx aprendido com a nova calibração: nunca abaixo do máx
    // calibrado; acima da faixa plausível = fallback/lixo antigo → re-baseia.
    if (s_learnedMax < g_data.maxRaw || s_learnedMax > cfg.tpsFaultHighRaw) {
      s_learnedMax = g_data.maxRaw;
    }
    persistData();
    s_persistedLearned = s_learnedMax;
    s_lastPersistMs = millis();
    s_state = State::Done;
  } else {
    abortRun();
  }
}

}  // namespace

void begin() {
  loadDataFromNvs();
  uint16_t lm = 0;
  Preferences prefs;
  if (prefs.begin(kNamespace, /*readOnly=*/true)) {
    lm = prefs.getUShort(kKeyLearnedMax, 0);
    prefs.end();
  }
  // Sem valor aprendido salvo → parte do máx calibrado (ou fundo de escala).
  if (lm == 0) lm = g_data.valid ? g_data.maxRaw : 4095;
  // Com calibração válida, mantém o aprendido coerente: nunca abaixo do máx
  // calibrado, nunca acima da faixa plausível (lixo de NVS antiga).
  if (g_data.valid &&
      (lm < g_data.maxRaw || lm > settings::get().tpsFaultHighRaw)) {
    lm = g_data.maxRaw;
  }
  s_learnedMax = lm;
  s_persistedLearned = lm;
  s_lastPersistMs = millis();
}

bool start() {
  if (running()) return false;
  hbridge::disable();  // coast: mola leva ao repouso durante o assentamento
  s_candRest = s_candMin = s_candMax = 0;
  enterPhase(State::SettleStart, millis());
  return true;
}

void abortRun() {
  if (!running()) return;
  hbridge::disable();  // estado seguro: duty 0 + EN baixo
  loadDataFromNvs();   // volta à última calibração válida
  s_state = State::Failed;
}

void run(bool idleActive) {
  if (!running()) return;
  if (!idleActive) {  // pedal acionado no meio da rotina → aborta
    abortRun();
    return;
  }

  const uint32_t now = millis();
  const Settings& cfg = settings::get();

  switch (s_state) {
    case State::SettleStart:
      if (now - s_phaseStartMs >= kSettleStartMs) enterPhase(State::Rest, now);
      break;

    case State::Rest:
    case State::OpenMax:
    case State::CloseMin:
      // Reaplica o drive da fase a cada iteração: se algo zerar o LEDC no meio
      // (ex.: troca de pwmFreqHz pela web), o duty volta no próximo tick em vez
      // de a fase estabilizar numa posição errada.
      if (s_state == State::OpenMax) hbridge::drive(cfg.calDrivePct);
      else if (s_state == State::CloseMin) hbridge::drive(-cfg.calDrivePct);
      feedStability(tps::raw(), now, cfg.calStabilityCounts);
      if (stabilityReached(now, cfg.calSettleMs)) {
        const uint16_t avg = stabilityAverage();
        if (s_state == State::Rest) {
          s_candRest = avg;
          hbridge::enable();
          hbridge::drive(cfg.calDrivePct);
          enterPhase(State::OpenMax, now);
        } else if (s_state == State::OpenMax) {
          s_candMax = avg;
          hbridge::drive(-cfg.calDrivePct);
          enterPhase(State::CloseMin, now);
        } else {
          s_candMin = avg;
          hbridge::disable();  // motor solto: borboleta volta ao repouso
          enterPhase(State::Finish, now);
        }
      } else if (now - s_phaseStartMs > cfg.calTimeoutMs) {
        abortRun();
      }
      break;

    case State::Finish:
      finishPhase(cfg);
      break;

    default:
      break;
  }
}

bool running() {
  return s_state >= State::SettleStart && s_state <= State::Finish;
}

State state() { return s_state; }

const char* stateName() {
  switch (s_state) {
    case State::Inactive: return "Inactive";
    case State::SettleStart: return "SettleStart";
    case State::Rest: return "Rest";
    case State::OpenMax: return "OpenMax";
    case State::CloseMin: return "CloseMin";
    case State::Finish: return "Finish";
    case State::Done: return "Done";
    case State::Failed: return "Failed";
  }
  return "?";
}

const Data& data() { return g_data; }

float positionPct(uint16_t rawValue) {
  if (!g_data.valid || g_data.maxRaw <= g_data.minRaw) return 0.0f;
  return 100.0f * (float)((int32_t)rawValue - (int32_t)g_data.minRaw) /
         (float)(g_data.maxRaw - g_data.minRaw);
}

uint16_t learnedMaxRaw() { return s_learnedMax; }

void observeRaw(uint16_t rawValue, bool idleActive) {
  const Settings& cfg = settings::get();
  if (idleActive || !cfg.outAutoLearnMax) return;
  // Leitura implausível (sensor em curto/solto) não pode envenenar o aprendido
  // — um glitch persistido na NVS degradaria a saída analógica para sempre.
  if (rawValue < cfg.tpsFaultLowRaw || rawValue > cfg.tpsFaultHighRaw) return;
  if ((uint32_t)rawValue > (uint32_t)s_learnedMax + kLearnMarginCounts) {
    s_learnedMax = rawValue;
  }
}

void maybePersistLearned() {
  // s_learnedMax só cresce em RAM; delta + intervalo mínimos protegem a flash.
  if ((uint16_t)(s_learnedMax - s_persistedLearned) < kPersistMinDeltaCounts) return;
  const uint32_t now = millis();
  if (now - s_lastPersistMs < kPersistMinIntervalMs) return;
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) return;
  prefs.putUShort(kKeyLearnedMax, s_learnedMax);
  prefs.end();
  s_persistedLearned = s_learnedMax;
  s_lastPersistMs = now;
}

}  // namespace calibration
