#include "settings.h"

#include <Preferences.h>

namespace {

constexpr const char* kNamespace = "cfg";
constexpr const char* kKeyVer = "ver";
constexpr const char* kKeyBlob = "blob";

Settings g_settings;

template <typename T>
T clampv(T v, T lo, T hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// NaN não passa em comparação: cai no default em vez de propagar.
float clampf(float v, float lo, float hi, float def) {
  return isnan(v) ? def : clampv(v, lo, hi);
}

// Saneia valores absurdos vindos de blob corrompido, layout antigo ou POST da
// web (save() também passa por aqui — o que persiste é sempre são).
void sanitize(Settings& s) {
  const Settings def;

  if (isnan(s.kp) || s.kp < 0.0f) s.kp = def.kp;
  if (isnan(s.ki) || s.ki < 0.0f) s.ki = def.ki;
  if (isnan(s.kd) || s.kd < 0.0f) s.kd = def.kd;
  s.deadbandPct = clampf(s.deadbandPct, 0.0f, 20.0f, def.deadbandPct);
  s.maxDutyPct = clampf(s.maxDutyPct, 0.0f, 100.0f, def.maxDutyPct);
  s.loopHz = clampv<uint16_t>(s.loopHz, 20, 1000);
  s.cmdTimeoutMs = clampv<uint16_t>(s.cmdTimeoutMs, 20, 10000);
  s.pwmFreqHz = clampv<uint32_t>(s.pwmFreqHz, 1000, 40000);
  s.calSettleMs = clampv<uint16_t>(s.calSettleMs, 50, 10000);
  s.calStabilityCounts = clampv<uint16_t>(s.calStabilityCounts, 1, 1000);
  s.calTimeoutMs = clampv<uint32_t>(s.calTimeoutMs, 500, 60000);
  s.calMinRangeCounts = clampv<uint16_t>(s.calMinRangeCounts, 10, 4095);
  s.calDrivePct = clampf(s.calDrivePct, 10.0f, 100.0f, def.calDrivePct);
  s.tpsFaultLowRaw = clampv<uint16_t>(s.tpsFaultLowRaw, 0, 4095);
  s.tpsFaultHighRaw = clampv<uint16_t>(s.tpsFaultHighRaw, 0, 4095);
  s.outMinRaw = clampv<uint16_t>(s.outMinRaw, 0, 4095);
  s.outMaxRaw = clampv<uint16_t>(s.outMaxRaw, 0, 4095);
  s.idleDebounceMs = clampv<uint16_t>(s.idleDebounceMs, 0, 500);

  // Relações entre campos: cada um pode ser válido isolado e o conjunto, não.
  // Timeout precisa comportar o settle + margem para o movimento mecânico,
  // senão toda fase da calibração estoura por timeout antes de estabilizar.
  if (s.calTimeoutMs < (uint32_t)s.calSettleMs + 500) {
    s.calTimeoutMs = (uint32_t)s.calSettleMs + 500;
  }
  // Faixa de plausibilidade invertida deixaria o TPS "sempre em falha".
  if (s.tpsFaultLowRaw >= s.tpsFaultHighRaw) {
    s.tpsFaultLowRaw = def.tpsFaultLowRaw;
    s.tpsFaultHighRaw = def.tpsFaultHighRaw;
  }

  // Strings sempre com terminador; WPA2 exige senha >= 8 caracteres.
  s.apSsid[sizeof(s.apSsid) - 1] = '\0';
  s.apPass[sizeof(s.apPass) - 1] = '\0';
  if (s.apSsid[0] == '\0') strlcpy(s.apSsid, def.apSsid, sizeof(s.apSsid));
  if (strlen(s.apPass) < 8) strlcpy(s.apPass, def.apPass, sizeof(s.apPass));
}

}  // namespace

namespace settings {

void begin() {
  Preferences prefs;
  bool loaded = false;
  // readOnly: falha se o namespace ainda não existe (primeiro boot) → defaults.
  if (prefs.begin(kNamespace, /*readOnly=*/true)) {
    if (prefs.getUChar(kKeyVer, 0) == SETTINGS_VERSION &&
        prefs.getBytesLength(kKeyBlob) == sizeof(Settings)) {
      Settings tmp;
      if (prefs.getBytes(kKeyBlob, &tmp, sizeof(tmp)) == sizeof(tmp)) {
        g_settings = tmp;
        loaded = true;
      }
    }
    prefs.end();
  }
  if (!loaded) g_settings = Settings{};
  sanitize(g_settings);
}

Settings& get() { return g_settings; }

void save() {
  sanitize(g_settings);  // nunca persiste (nem deixa ativo) valor fora de faixa
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) return;
  // Blob antes da versão: gravação interrompida deixa versão/tamanho sem
  // casar e a próxima carga volta aos defaults em vez de ler lixo.
  prefs.putBytes(kKeyBlob, &g_settings, sizeof(g_settings));
  prefs.putUChar(kKeyVer, SETTINGS_VERSION);
  prefs.end();
}

void resetDefaults() { g_settings = Settings{}; }

}  // namespace settings
