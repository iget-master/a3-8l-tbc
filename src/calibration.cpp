#include "calibration.h"

#include <stdarg.h>

#include <Preferences.h>

#include "motor.h"
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
constexpr const char* kKeyRest2 = "rest2";
constexpr const char* kKeyMin2 = "min2";
constexpr const char* kKeyMax2 = "max2";
constexpr const char* kKeyBootN = "bootn";

constexpr uint32_t kSettleStartMs = 300;           // assentamento mecânico inicial
constexpr uint16_t kLearnMarginCounts = 8;         // margem anti-ruído do máx aprendido
constexpr uint16_t kPersistMinDeltaCounts = 16;    // grava só se mudou o bastante...
constexpr uint32_t kPersistMinIntervalMs = 60000;  // ...e com folga entre gravações

State s_state = State::Inactive;
Data g_data;

// Motivo da última falha (mostrado na página e no serial): tira a adivinhação
// do diagnóstico em bancada.
char s_failReason[112] = "";

void setFail(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(s_failReason, sizeof(s_failReason), fmt, ap);
  va_end(ap);
}

uint32_t s_phaseStartMs = 0;

// Candidatos da corrida atual — g_data só muda se a validação passar.
uint16_t s_candRest = 0;
uint16_t s_candMin = 0;
uint16_t s_candMax = 0;
// Pista 2: capturada no fechamento de cada fase (a borboleta está parada e o
// filtro assentado — dispensa janela de estabilidade própria).
uint16_t s_candRest2 = 0;
uint16_t s_candMin2 = 0;
uint16_t s_candMax2 = 0;

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

// Contador de boots (auto-calibração periódica)
uint16_t s_bootCount = 0;

// Auto-rastreio do repouso: EMA em float (resolução sub-count), âncora da
// última calibração completa e cópias persistidas (throttling próprio).
constexpr float kRestTrackAlpha = 0.0002f;        // τ ≈ 25 s @ 200 Hz
constexpr uint16_t kRestTrackMaxCounts = 80;      // desvio máx da âncora
constexpr uint16_t kRestTrackStructMargin = 100;  // rest sempre < máx − margem
constexpr uint16_t kRestPersistDeltaCounts = 8;
constexpr uint32_t kRestPersistIntervalMs = 60000;
float s_restTrackF = 0.0f;
float s_rest2TrackF = 0.0f;
uint16_t s_restAnchor = 0;
uint16_t s_rest2Anchor = 0;
uint16_t s_restPersisted = 0;
uint16_t s_rest2Persisted = 0;
uint32_t s_restPersistMs = 0;

// (Re)inicializa o estado do rastreio a partir da calibração vigente.
void initTrackState() {
  s_restTrackF = (float)g_data.restRaw;
  s_rest2TrackF = (float)g_data.rest2Raw;
  s_restAnchor = g_data.restRaw;
  s_rest2Anchor = g_data.rest2Raw;
  s_restPersisted = g_data.restRaw;
  s_rest2Persisted = g_data.rest2Raw;
  s_restPersistMs = millis();
}

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

// Checagem estrutural (mín <= repouso < máx): protege positionPct de lixo na
// NVS. O mín pode empatar com o repouso — corpos cujo repouso da mola é o
// próprio batente fechado não têm faixa abaixo do repouso.
bool structurallyValid(const Data& d) {
  return d.minRaw <= d.restRaw && d.restRaw < d.maxRaw;
}

// Pista 2 segue a mesma regra estrutural (no domínio já espelhado por
// tps2Invert). Inversão mal configurada → máx2 < rep2 → inválida: só a
// verificação cruzada desativa, o controle segue na pista 1.
bool structurallyValid2(const Data& d) {
  return d.min2Raw <= d.rest2Raw && d.rest2Raw < d.max2Raw;
}

void loadDataFromNvs() {
  Data d;
  Preferences prefs;
  if (prefs.begin(kNamespace, /*readOnly=*/true)) {
    d.restRaw = prefs.getUShort(kKeyRest, 0);
    d.minRaw = prefs.getUShort(kKeyMin, 0);
    d.maxRaw = prefs.getUShort(kKeyMax, 0);
    d.valid = prefs.getBool(kKeyValid, false);
    d.rest2Raw = prefs.getUShort(kKeyRest2, 0);
    d.min2Raw = prefs.getUShort(kKeyMin2, 0);
    d.max2Raw = prefs.getUShort(kKeyMax2, 0);
    prefs.end();
  }
  // Pista 2 só vale junto com uma calibração principal válida (chaves ausentes
  // viram 0 → estruturalmente inválida → verificação cruzada desativada).
  d.valid2 = d.valid && structurallyValid2(d);
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
  prefs.putUShort(kKeyRest2, g_data.rest2Raw);
  prefs.putUShort(kKeyMin2, g_data.min2Raw);
  prefs.putUShort(kKeyMax2, g_data.max2Raw);
  prefs.putUShort(kKeyLearnedMax, s_learnedMax);
  prefs.putBool(kKeyValid, true);
  prefs.end();
}

void finishPhase(const Settings& cfg) {
  // Corpo cujo repouso é o batente fechado: a fase "fechar" não sai do lugar e
  // o mín empata com o repouso (diferença de ruído) — clampa e aceita. A faixa
  // mínima é exigida só do lado da abertura, que é o lado que o controle usa.
  // Motor com fios trocados continua reprovando: a fase "abrir" fecharia, o
  // máx empataria com o repouso e a faixa de abertura não fecha a conta.
  if (s_candMin > s_candRest) s_candMin = s_candRest;
  if (!(s_candRest < s_candMax &&
        (uint16_t)(s_candMax - s_candRest) >= cfg.calMinRangeCounts)) {
    setFail("faixa de abertura insuficiente: rep %u, máx %u (Δ %d < %u)",
            (unsigned)s_candRest, (unsigned)s_candMax,
            (int)s_candMax - (int)s_candRest, (unsigned)cfg.calMinRangeCounts);
    abortRun();
    return;
  }
  g_data.restRaw = s_candRest;
  g_data.minRaw = s_candMin;
  g_data.maxRaw = s_candMax;
  g_data.valid = true;
  // Pista 2: melhor esforço — só quando habilitada; inválida não reprova a
  // rotina, apenas desativa a verificação cruzada.
  if (settings::get().tps2Enabled) {
    if (s_candMin2 > s_candRest2) s_candMin2 = s_candRest2;
    g_data.rest2Raw = s_candRest2;
    g_data.min2Raw = s_candMin2;
    g_data.max2Raw = s_candMax2;
  } else {
    g_data.rest2Raw = g_data.min2Raw = g_data.max2Raw = 0;
  }
  g_data.valid2 = structurallyValid2(g_data);
  if (settings::get().tps2Enabled && !g_data.valid2) {
    Serial.println("[cal] pista 2 inválida (checar tps2Invert/fiação) — verificação cruzada OFF");
  }
  // Reconcilia o máx aprendido com a nova calibração: nunca abaixo do máx
  // calibrado; acima da faixa plausível = fallback/lixo antigo → re-baseia.
  if (s_learnedMax < g_data.maxRaw || s_learnedMax > cfg.tpsFaultHighRaw) {
    s_learnedMax = g_data.maxRaw;
  }
  persistData();
  s_persistedLearned = s_learnedMax;
  s_lastPersistMs = millis();
  // Calibração completa zera o ciclo periódico e re-ancora o rastreio.
  s_bootCount = 0;
  {
    Preferences prefs;
    if (prefs.begin(kNamespace, /*readOnly=*/false)) {
      prefs.putUShort(kKeyBootN, 0);
      prefs.end();
    }
  }
  initTrackState();
  s_state = State::Done;
  Serial.printf("[cal] OK: rep %u · mín %u · máx %u\n", (unsigned)g_data.restRaw,
                (unsigned)g_data.minRaw, (unsigned)g_data.maxRaw);
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

  // Contador de boots da auto-calibração periódica (incrementa a cada boot;
  // zera numa calibração completa bem-sucedida).
  Preferences prefs2;
  if (prefs2.begin(kNamespace, /*readOnly=*/false)) {
    s_bootCount = prefs2.getUShort(kKeyBootN, 0);
    if (s_bootCount < 65535) s_bootCount++;
    prefs2.putUShort(kKeyBootN, s_bootCount);
    prefs2.end();
  }

  initTrackState();
}

bool start() {
  if (running()) return false;
  motor::disable();  // coast: mola leva ao repouso durante o assentamento
  s_candRest = s_candMin = s_candMax = 0;
  s_candRest2 = s_candMin2 = s_candMax2 = 0;
  s_failReason[0] = '\0';
  Serial.printf("[cal] início (raw=%u)\n", (unsigned)tps::raw());
  enterPhase(State::SettleStart, millis());
  return true;
}

void abortRun() {
  if (!running()) return;
  motor::disable();  // estado seguro: duty 0 + EN baixo
  loadDataFromNvs();   // volta à última calibração válida
  initTrackState();    // rastreio re-ancora na calibração restaurada
  // Abortos internos preenchem o motivo antes; aqui só o caso externo.
  if (s_failReason[0] == '\0') {
    strlcpy(s_failReason, "abortada de fora (TPS implausível ou troca de modo)",
            sizeof(s_failReason));
  }
  Serial.printf("[cal] FALHA: %s\n", s_failReason);
  s_state = State::Failed;
}

void run(bool idleActive) {
  if (!running()) return;
  if (!idleActive) {  // pedal acionado no meio da rotina → aborta
    setFail("idle solto durante a rotina (switch abriu na fase %s)",
            stateName());
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
      if (s_state == State::OpenMax) motor::drive(cfg.calDrivePct);
      else if (s_state == State::CloseMin) motor::drive(-cfg.calDrivePct);
      feedStability(tps::raw(), now, cfg.calStabilityCounts);
      if (stabilityReached(now, cfg.calSettleMs)) {
        const uint16_t avg = stabilityAverage();
        if (s_state == State::Rest) {
          s_candRest = avg;
          s_candRest2 = tps::raw2();  // pista 2: snapshot com a borboleta parada
          Serial.printf("[cal] repouso=%u → abrindo\n", (unsigned)avg);
          motor::enable();
          motor::drive(cfg.calDrivePct);
          enterPhase(State::OpenMax, now);
        } else {
          // Acionamento de um sentido só: não existe "fechar ativo" — o mín é
          // o próprio repouso (a mola fecha). A antiga fase CloseMin também
          // abriria o switch de idle neste corpo (pino descola da alavanca).
          s_candMax = avg;
          s_candMax2 = tps::raw2();
          s_candMin = s_candRest;
          s_candMin2 = s_candRest2;
          Serial.printf("[cal] máx=%u → validando (mín=rep)\n", (unsigned)avg);
          motor::disable();  // motor solto: mola leva ao repouso
          enterPhase(State::Finish, now);
        }
      } else if (now - s_phaseStartMs > cfg.calTimeoutMs) {
        setFail("timeout na fase %s: TPS não estabilizou (banda %u counts por "
                "%u ms; raw=%u)",
                stateName(), (unsigned)cfg.calStabilityCounts,
                (unsigned)cfg.calSettleMs, (unsigned)tps::raw());
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

const char* failReason() { return s_failReason; }

const Data& data() { return g_data; }

float positionPct(uint16_t rawValue) {
  if (!g_data.valid) return 0.0f;
  const int32_t raw = rawValue;
  const int32_t rest = g_data.restRaw;
  // Duas rampas em torno do repouso: acima → 0..+100 até maxRaw,
  // abaixo → 0..−100 até minRaw.
  const int32_t span = raw >= rest ? (int32_t)g_data.maxRaw - rest
                                   : rest - (int32_t)g_data.minRaw;
  if (span <= 0) return 0.0f;
  const float pct = 100.0f * (float)(raw - rest) / (float)span;
  // Satura em ±100%: leitura além do máx/mín calibrado não extrapola a faixa.
  return pct > 100.0f ? 100.0f : (pct < -100.0f ? -100.0f : pct);
}

float positionPct2(uint16_t rawValue) {
  if (!g_data.valid2) return 0.0f;
  const int32_t raw = rawValue;
  const int32_t rest = g_data.rest2Raw;
  const int32_t span = raw >= rest ? (int32_t)g_data.max2Raw - rest
                                   : rest - (int32_t)g_data.min2Raw;
  if (span <= 0) return 0.0f;
  const float pct = 100.0f * (float)(raw - rest) / (float)span;
  return pct > 100.0f ? 100.0f : (pct < -100.0f ? -100.0f : pct);
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

bool bootCalibrationDue() {
  const uint32_t n = settings::get().calEveryBoots;
  return n > 0 && (uint32_t)s_bootCount >= n;
}

void trackRest() {
  const Settings& cfg = settings::get();
  if (!cfg.restTrackEnabled || !g_data.valid || running()) return;

  // Pista 1: EMA lenta, presa à âncora (±80 counts da última calibração) e à
  // estrutura (rest sempre bem abaixo do máx).
  s_restTrackF += kRestTrackAlpha * ((float)tps::raw() - s_restTrackF);
  {
    int32_t v = (int32_t)(s_restTrackF + 0.5f);
    const int32_t lo = (int32_t)s_restAnchor - kRestTrackMaxCounts;
    const int32_t hi = (int32_t)s_restAnchor + kRestTrackMaxCounts;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    const int32_t structHi = (int32_t)g_data.maxRaw - kRestTrackStructMargin;
    if (v > structHi) v = structHi;
    if (v < 0) v = 0;
    const bool minLocked = g_data.minRaw >= g_data.restRaw;  // caso 8L: mín = rep
    g_data.restRaw = (uint16_t)v;
    if (minLocked) g_data.minRaw = g_data.restRaw;
    else if (g_data.minRaw > g_data.restRaw) g_data.minRaw = g_data.restRaw;
  }

  // Pista 2 (quando em uso): mesmo tratamento, senão o rastreio da pista 1
  // criaria offset artificial na verificação cruzada.
  if (cfg.tps2Enabled && g_data.valid2) {
    s_rest2TrackF += kRestTrackAlpha * ((float)tps::raw2() - s_rest2TrackF);
    int32_t v = (int32_t)(s_rest2TrackF + 0.5f);
    const int32_t lo = (int32_t)s_rest2Anchor - kRestTrackMaxCounts;
    const int32_t hi = (int32_t)s_rest2Anchor + kRestTrackMaxCounts;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    const int32_t structHi = (int32_t)g_data.max2Raw - kRestTrackStructMargin;
    if (v > structHi) v = structHi;
    if (v < 0) v = 0;
    const bool minLocked = g_data.min2Raw >= g_data.rest2Raw;
    g_data.rest2Raw = (uint16_t)v;
    if (minLocked) g_data.min2Raw = g_data.rest2Raw;
    else if (g_data.min2Raw > g_data.rest2Raw) g_data.min2Raw = g_data.rest2Raw;
  }

  // Persistência com throttling. Ordem das gravações preserva mín <= rep em
  // qualquer ponto de queda (cada putUShort é atômico na NVS).
  const uint32_t now = millis();
  const uint16_t d1 = g_data.restRaw > s_restPersisted
                          ? (uint16_t)(g_data.restRaw - s_restPersisted)
                          : (uint16_t)(s_restPersisted - g_data.restRaw);
  const uint16_t d2 = g_data.rest2Raw > s_rest2Persisted
                          ? (uint16_t)(g_data.rest2Raw - s_rest2Persisted)
                          : (uint16_t)(s_rest2Persisted - g_data.rest2Raw);
  if (d1 < kRestPersistDeltaCounts && d2 < kRestPersistDeltaCounts) return;
  if (now - s_restPersistMs < kRestPersistIntervalMs) return;
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) return;
  if (g_data.restRaw >= s_restPersisted) {  // subindo: rep primeiro, mín depois
    prefs.putUShort(kKeyRest, g_data.restRaw);
    prefs.putUShort(kKeyMin, g_data.minRaw);
  } else {  // descendo: mín primeiro, rep depois
    prefs.putUShort(kKeyMin, g_data.minRaw);
    prefs.putUShort(kKeyRest, g_data.restRaw);
  }
  if (g_data.rest2Raw >= s_rest2Persisted) {
    prefs.putUShort(kKeyRest2, g_data.rest2Raw);
    prefs.putUShort(kKeyMin2, g_data.min2Raw);
  } else {
    prefs.putUShort(kKeyMin2, g_data.min2Raw);
    prefs.putUShort(kKeyRest2, g_data.rest2Raw);
  }
  prefs.end();
  s_restPersisted = g_data.restRaw;
  s_rest2Persisted = g_data.rest2Raw;
  s_restPersistMs = now;
}

}  // namespace calibration
