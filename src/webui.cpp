#include "webui.h"

#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "calibration.h"
#include "control.h"
#include "hbridge.h"
#include "isense.h"
#include "pwm_input.h"
#include "settings.h"
#include "tps.h"
#include "version.h"
#include "webui_page.h"

namespace webui {
namespace {

WebServer s_server(80);

// Rede: no boot tenta entrar na rede local (STA) se houver SSID configurado.
// Se não conectar dentro do timeout, sobe o próprio AP como fallback.
enum class NetState { Connecting, Sta, Ap };
NetState s_net = NetState::Ap;
uint32_t s_staStartMs = 0;
uint32_t s_lastLogMs = 0;
constexpr uint32_t kStaConnectTimeoutMs = 15000;

// Reboot adiado após OTA pela web: a resposta HTTP precisa escoar antes.
bool s_rebootPending = false;
uint32_t s_rebootAtMs = 0;

template <typename T>
T clampv(T v, T lo, T hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

const char* b(bool v) { return v ? "true" : "false"; }

// Escapa `"`, `\` e caracteres de controle (JSON exige U+0000–U+001F como
// \u00XX) para embutir strings do usuário (SSID/senha) no JSON.
void jsonEscape(const char* in, char* out, size_t outSize) {
  static const char kHex[] = "0123456789abcdef";
  size_t o = 0;
  for (const char* p = in; *p != '\0'; ++p) {
    const unsigned char c = (unsigned char)*p;
    if (c < 0x20) {
      if (o + 7 >= outSize) break;
      out[o++] = '\\';
      out[o++] = 'u';
      out[o++] = '0';
      out[o++] = '0';
      out[o++] = kHex[c >> 4];
      out[o++] = kHex[c & 0x0F];
    } else {
      if (o + 2 >= outSize) break;
      if (c == '"' || c == '\\') out[o++] = '\\';
      out[o++] = (char)c;
    }
  }
  out[o] = '\0';
}

bool hasControlChars(const String& v) {
  for (const char* p = v.c_str(); *p != '\0'; ++p) {
    if ((unsigned char)*p < 0x20) return true;
  }
  return false;
}

// Campo presente e não-vazio no form → converte, saneia e devolve; ausente ou
// vazio → mantém atual (input limpo não pode virar 0 silenciosamente).
float argFloat(const char* name, float cur, float lo, float hi) {
  if (!s_server.hasArg(name)) return cur;
  const String raw = s_server.arg(name);
  if (raw.length() == 0) return cur;
  const float v = raw.toFloat();
  return isnan(v) ? cur : clampv(v, lo, hi);
}

long argLong(const char* name, long cur, long lo, long hi) {
  if (!s_server.hasArg(name)) return cur;
  const String raw = s_server.arg(name);
  if (raw.length() == 0) return cur;
  return clampv(raw.toInt(), lo, hi);
}

bool argBool(const char* name, bool cur) {
  if (!s_server.hasArg(name)) return cur;
  const String raw = s_server.arg(name);
  if (raw.length() == 0) return cur;
  return raw.toInt() != 0;  // aceita "0"/"1"
}

void sendOk() { s_server.send(200, "text/plain", "OK"); }

void sendJsonOrOverflow(const char* buf, int n, size_t bufSize) {
  if (n < 0 || n >= (int)bufSize) {
    s_server.send(500, "text/plain", "json overflow");
    return;
  }
  s_server.send(200, "application/json", buf);
}

void handleRoot() { s_server.send_P(200, "text/html", WEBUI_PAGE); }

void handleStatus() {
  static char buf[1024];
  const calibration::Data& cal = calibration::data();
  const int n = snprintf(
      buf, sizeof(buf),
      "{\"ver\":\"%s\",\"uptimeMs\":%lu,\"mode\":\"%s\",\"fault\":\"%s\","
      "\"idle\":%s,\"setpoint\":%.2f,\"pos\":%.2f,\"duty\":%.2f,"
      "\"analogOut\":%.2f,\"rawTps\":%u,\"rawTps2\":%u,\"pos2\":%.2f,"
      "\"cmdDuty\":%.2f,\"cmdFreq\":%.1f,"
      "\"cmdPresent\":%s,\"pidP\":%.2f,\"pidI\":%.2f,\"pidD\":%.2f,"
      "\"manual\":%s,\"spOvr\":%s,\"mCur\":%u,\"mCurEst\":%.0f,\"stall\":%s,"
      "\"mLatch\":%s,\"cal\":{\"state\":\"%s\",\"fail\":\"%s\",\"rest\":%u,"
      "\"min\":%u,\"max\":%u,\"valid\":%s,\"learnedMax\":%u}}",
      FW_VERSION, (unsigned long)millis(), control::modeName(),
      control::faultReason(), b(control::idleActive()),
      (double)control::setpointPct(), (double)control::positionPct(),
      (double)control::appliedDutyPct(), (double)control::analogOutPct(),
      (unsigned)tps::raw(), (unsigned)tps::raw2(), (double)control::pos2Pct(),
      (double)pwm_input::dutyPct(),
      (double)pwm_input::freqHz(), b(pwm_input::signalPresent()),
      (double)control::pidP(), (double)control::pidI(),
      (double)control::pidD(), b(control::manualActive()),
      b(control::setpointOverrideActive()), (unsigned)isense::raw(),
      (double)isense::estRaw100(), b(isense::stalled()),
      b(control::motorFaultLatched()),
      calibration::stateName(), calibration::failReason(),
      (unsigned)cal.restRaw, (unsigned)cal.minRaw,
      (unsigned)cal.maxRaw, b(cal.valid),
      (unsigned)calibration::learnedMaxRaw());
  sendJsonOrOverflow(buf, n, sizeof(buf));
}

void handleParamsGet() {
  static char buf[2048];
  const Settings& s = settings::get();
  char ssid[6 * sizeof(s.apSsid)];  // pior caso: tudo \u00XX
  char pass[6 * sizeof(s.apPass)];
  char staSsid[6 * sizeof(s.staSsid)];
  char staPass[6 * sizeof(s.staPass)];
  jsonEscape(s.apSsid, ssid, sizeof(ssid));
  jsonEscape(s.apPass, pass, sizeof(pass));
  jsonEscape(s.staSsid, staSsid, sizeof(staSsid));
  jsonEscape(s.staPass, staPass, sizeof(staPass));
  const int n = snprintf(
      buf, sizeof(buf),
      "{\"kp\":%.3f,\"ki\":%.3f,\"kd\":%.3f,\"deadbandPct\":%.2f,"
      "\"maxDutyPct\":%.2f,\"spSlewPctPerS\":%.1f,\"loopHz\":%u,\"cmdTimeoutMs\":%u,"
      "\"cmdStuckHighIs100\":%s,\"pwmFreqHz\":%lu,\"tpsFaultLowRaw\":%u,"
      "\"tpsFaultHighRaw\":%u,\"tpsEmaAlpha\":%.3f,\"tpsMedianSamples\":%u,"
      "\"tpsInvert\":%s,\"tps2Enabled\":%s,\"tps2Invert\":%s,"
      "\"tps2DivergePct\":%.1f,"
      "\"calSettleMs\":%u,\"calStabilityCounts\":%u,"
      "\"calTimeoutMs\":%lu,\"calMinRangeCounts\":%u,\"calDrivePct\":%.2f,"
      "\"calMeasureClose\":%s,\"calEveryBoots\":%u,\"restTrackEnabled\":%s,"
      "\"outMinRaw\":%u,\"outMaxRaw\":%u,\"outAutoLearnMax\":%s,"
      "\"outBaseOnRelease\":%s,"
      "\"idleActiveLow\":%s,\"idleDebounceMs\":%u,"
      "\"isenseEnabled\":%s,\"isMinDutyPct\":%.1f,\"isShortRaw\":%u,"
      "\"isOpenRaw\":%u,\"isStallRaw\":%u,\"isShortMs\":%u,\"isOpenMs\":%u,"
      "\"isStallMs\":%u,"
      "\"apSsid\":\"%s\",\"apPass\":\"%s\","
      "\"staSsid\":\"%s\",\"staPass\":\"%s\"}",
      (double)s.kp, (double)s.ki, (double)s.kd, (double)s.deadbandPct,
      (double)s.maxDutyPct, (double)s.spSlewPctPerS, (unsigned)s.loopHz,
      (unsigned)s.cmdTimeoutMs,
      b(s.cmdStuckHighIs100), (unsigned long)s.pwmFreqHz,
      (unsigned)s.tpsFaultLowRaw, (unsigned)s.tpsFaultHighRaw,
      (double)s.tpsEmaAlpha, (unsigned)s.tpsMedianSamples,
      b(s.tpsInvert != 0), b(s.tps2Enabled != 0), b(s.tps2Invert != 0),
      (double)s.tps2DivergePct,
      (unsigned)s.calSettleMs, (unsigned)s.calStabilityCounts,
      (unsigned long)s.calTimeoutMs, (unsigned)s.calMinRangeCounts,
      (double)s.calDrivePct, b(s.calMeasureClose != 0),
      (unsigned)s.calEveryBoots, b(s.restTrackEnabled != 0),
      (unsigned)s.outMinRaw, (unsigned)s.outMaxRaw,
      b(s.outAutoLearnMax), b(s.outBaseOnRelease != 0),
      b(s.idleActiveLow), (unsigned)s.idleDebounceMs,
      b(s.isenseEnabled), (double)s.isMinDutyPct, (unsigned)s.isShortRaw,
      (unsigned)s.isOpenRaw, (unsigned)s.isStallRaw, (unsigned)s.isShortMs,
      (unsigned)s.isOpenMs, (unsigned)s.isStallMs,
      ssid, pass, staSsid, staPass);
  sendJsonOrOverflow(buf, n, sizeof(buf));
}

// Limites espelham o sanitize() do settings: o estado em RAM nunca fica pior
// do que ficaria após um reboot.
void handleParamsPost() {
  Settings& s = settings::get();
  const uint32_t oldFreq = s.pwmFreqHz;

  s.kp = argFloat("kp", s.kp, 0.0f, 1000.0f);
  s.ki = argFloat("ki", s.ki, 0.0f, 1000.0f);
  s.kd = argFloat("kd", s.kd, 0.0f, 100.0f);
  s.deadbandPct = argFloat("deadbandPct", s.deadbandPct, 0.0f, 20.0f);
  s.maxDutyPct = argFloat("maxDutyPct", s.maxDutyPct, 0.0f, 100.0f);
  s.spSlewPctPerS = argFloat("spSlewPctPerS", s.spSlewPctPerS, 0.0f, 20000.0f);
  s.loopHz = (uint16_t)argLong("loopHz", s.loopHz, 20, 1000);
  s.cmdTimeoutMs = (uint16_t)argLong("cmdTimeoutMs", s.cmdTimeoutMs, 20, 10000);
  s.cmdStuckHighIs100 = argBool("cmdStuckHighIs100", s.cmdStuckHighIs100);
  s.pwmFreqHz = (uint32_t)argLong("pwmFreqHz", s.pwmFreqHz, 1000, 40000);
  s.tpsFaultLowRaw = (uint16_t)argLong("tpsFaultLowRaw", s.tpsFaultLowRaw, 0, 4095);
  s.tpsFaultHighRaw = (uint16_t)argLong("tpsFaultHighRaw", s.tpsFaultHighRaw, 0, 4095);
  s.tpsEmaAlpha = argFloat("tpsEmaAlpha", s.tpsEmaAlpha, 0.01f, 1.0f);
  s.tpsMedianSamples =
      (uint8_t)argLong("tpsMedianSamples", s.tpsMedianSamples, 1, TPS_MEDIAN_MAX);
  s.tpsInvert = argBool("tpsInvert", s.tpsInvert != 0) ? 1u : 0u;
  s.tps2Enabled = argBool("tps2Enabled", s.tps2Enabled != 0) ? 1u : 0u;
  s.tps2Invert = argBool("tps2Invert", s.tps2Invert != 0) ? 1u : 0u;
  s.tps2DivergePct = argFloat("tps2DivergePct", s.tps2DivergePct, 2.0f, 50.0f);
  s.calSettleMs = (uint16_t)argLong("calSettleMs", s.calSettleMs, 50, 10000);
  s.calStabilityCounts = (uint16_t)argLong("calStabilityCounts", s.calStabilityCounts, 1, 1000);
  s.calTimeoutMs = (uint32_t)argLong("calTimeoutMs", s.calTimeoutMs, 500, 60000);
  s.calMinRangeCounts = (uint16_t)argLong("calMinRangeCounts", s.calMinRangeCounts, 10, 4095);
  s.calDrivePct = argFloat("calDrivePct", s.calDrivePct, 10.0f, 100.0f);
  s.calMeasureClose = argBool("calMeasureClose", s.calMeasureClose != 0) ? 1u : 0u;
  s.calEveryBoots = (uint32_t)argLong("calEveryBoots", (long)s.calEveryBoots, 0, 1000);
  s.restTrackEnabled = argBool("restTrackEnabled", s.restTrackEnabled != 0) ? 1u : 0u;
  s.outMinRaw = (uint16_t)argLong("outMinRaw", s.outMinRaw, 0, 4095);
  s.outMaxRaw = (uint16_t)argLong("outMaxRaw", s.outMaxRaw, 0, 4095);
  s.outAutoLearnMax = argBool("outAutoLearnMax", s.outAutoLearnMax);
  s.outBaseOnRelease =
      argBool("outBaseOnRelease", s.outBaseOnRelease != 0) ? 1u : 0u;
  s.idleActiveLow = argBool("idleActiveLow", s.idleActiveLow);
  s.idleDebounceMs = (uint16_t)argLong("idleDebounceMs", s.idleDebounceMs, 0, 500);
  s.isenseEnabled = argBool("isenseEnabled", s.isenseEnabled);
  s.isMinDutyPct = argFloat("isMinDutyPct", s.isMinDutyPct, 5.0f, 100.0f);
  s.isShortRaw = (uint16_t)argLong("isShortRaw", s.isShortRaw, 0, 4095);
  s.isOpenRaw = (uint16_t)argLong("isOpenRaw", s.isOpenRaw, 0, 4095);
  s.isStallRaw = (uint16_t)argLong("isStallRaw", s.isStallRaw, 0, 4095);
  s.isShortMs = (uint16_t)argLong("isShortMs", s.isShortMs, 5, 1000);
  s.isOpenMs = (uint16_t)argLong("isOpenMs", s.isOpenMs, 20, 5000);
  s.isStallMs = (uint16_t)argLong("isStallMs", s.isStallMs, 10, 2000);

  if (s_server.hasArg("apSsid")) {
    const String v = s_server.arg("apSsid");
    if (v.length() > 0 && v.length() < sizeof(s.apSsid) &&
        !hasControlChars(v)) {
      strlcpy(s.apSsid, v.c_str(), sizeof(s.apSsid));
    }
  }
  if (s_server.hasArg("apPass")) {
    const String v = s_server.arg("apPass");
    // WPA2 exige >= 8 caracteres; fora disso mantém a senha atual.
    if (v.length() >= 8 && v.length() < sizeof(s.apPass) &&
        !hasControlChars(v)) {
      strlcpy(s.apPass, v.c_str(), sizeof(s.apPass));
    }
  }

  // Rede local (STA): SSID vazio = desativa; senha vazia = rede aberta.
  if (s_server.hasArg("staSsid")) {
    const String v = s_server.arg("staSsid");
    if (v.length() < sizeof(s.staSsid) && !hasControlChars(v)) {
      strlcpy(s.staSsid, v.c_str(), sizeof(s.staSsid));
    }
  }
  if (s_server.hasArg("staPass")) {
    const String v = s_server.arg("staPass");
    if (v.length() < sizeof(s.staPass) && !hasControlChars(v)) {
      strlcpy(s.staPass, v.c_str(), sizeof(s.staPass));
    }
  }

  settings::save();
  if (s.pwmFreqHz != oldFreq) hbridge::setFrequency(s.pwmFreqHz);
  sendOk();
}

void handleManual() {
  const bool on = s_server.arg("on").toInt() != 0;
  const float duty = on
      ? clampv(s_server.arg("duty").toFloat(), -100.0f, 100.0f)
      : 0.0f;
  control::setManual(on, duty);
  sendOk();
}

void handleSetpoint() {
  const bool on = s_server.arg("on").toInt() != 0;
  const float sp =
      on ? clampv(s_server.arg("sp").toFloat(), -100.0f, 100.0f) : 0.0f;
  control::setSetpointOverride(on, sp);
  sendOk();
}

void handleCal() {
  control::requestCalibration();  // efetiva só se em idle
  sendOk();
}

void handleFaultClear() {
  control::clearMotorFault();
  sendOk();
}

// OTA pela página (POST /update, multipart). O handleClient síncrono bloqueia o
// loop() durante toda a transferência: ponte desabilitada no início e task WDT
// alimentado a cada bloco.
void handleUpdateUpload() {
  HTTPUpload& up = s_server.upload();
  if (up.status == UPLOAD_FILE_START) {
    hbridge::disable();
    Serial.printf("[ota] web: recebendo \"%s\"\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    feedLoopWDT();
    if (Update.isRunning() &&
        Update.write(up.buf, up.currentSize) != up.currentSize) {
      Update.printError(Serial);
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[ota] web: %u bytes gravados\n", (unsigned)up.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    Serial.println("[ota] web: upload abortado");
  }
}

void handleUpdateDone() {
  if (Update.hasError()) {
    s_server.send(500, "text/plain", "falha na gravação — ver log serial");
    return;
  }
  s_server.send(200, "text/plain", "OK");
  s_rebootPending = true;
  s_rebootAtMs = millis() + 600;
}

void handleDefaults() {
  const uint32_t oldFreq = settings::get().pwmFreqHz;
  settings::resetDefaults();
  settings::save();
  if (settings::get().pwmFreqHz != oldFreq) {
    hbridge::setFrequency(settings::get().pwmFreqHz);
  }
  sendOk();
}

// Sobe o AP próprio (rede de configuração e de fallback) e loga o IP.
void startAP() {
  const Settings& s = settings::get();
  WiFi.mode(WIFI_AP);
  const bool ok = WiFi.softAP(s.apSsid, s.apPass);
  Serial.printf("[webui] AP %s: SSID=\"%s\" IP=%s\n", ok ? "OK" : "FALHOU",
                s.apSsid, WiFi.softAPIP().toString().c_str());
}

}  // namespace

void begin() {
  const Settings& s = settings::get();

  if (s.staSsid[0] != '\0') {
    // Rede local configurada → tenta entrar como estação (não bloqueia; o
    // resultado é acompanhado em loop(), com fallback para o AP).
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(s.staSsid, s.staPass);
    s_staStartMs = millis();
    s_lastLogMs = s_staStartMs;
    s_net = NetState::Connecting;
    Serial.printf("[webui] STA: tentando conectar em \"%s\"...\n", s.staSsid);
  } else {
    startAP();
    s_net = NetState::Ap;
  }

  s_server.on("/", HTTP_GET, handleRoot);
  s_server.on("/api/status", HTTP_GET, handleStatus);
  s_server.on("/api/params", HTTP_GET, handleParamsGet);
  s_server.on("/api/params", HTTP_POST, handleParamsPost);
  s_server.on("/api/manual", HTTP_POST, handleManual);
  s_server.on("/api/setpoint", HTTP_POST, handleSetpoint);
  s_server.on("/api/cal", HTTP_POST, handleCal);
  s_server.on("/api/faultclear", HTTP_POST, handleFaultClear);
  s_server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  s_server.on("/api/defaults", HTTP_POST, handleDefaults);
  s_server.onNotFound([]() { s_server.send(404, "text/plain", "not found"); });
  s_server.begin();
}

void loop() {
  s_server.handleClient();

  if (s_rebootPending && (int32_t)(millis() - s_rebootAtMs) >= 0) {
    Serial.println("[ota] web: reiniciando");
    ESP.restart();
  }

  if (s_net != NetState::Connecting) return;

  const wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    s_net = NetState::Sta;
    const String ip = WiFi.localIP().toString();
    Serial.printf("[webui] STA conectado: IP=%s  ->  http://%s/\n", ip.c_str(),
                  ip.c_str());
    return;
  }

  const uint32_t now = millis();
  const bool failed = (st == WL_NO_SSID_AVAIL || st == WL_CONNECT_FAILED);
  if (failed || now - s_staStartMs > kStaConnectTimeoutMs) {
    Serial.printf("[webui] STA sem conexão (status=%d); subindo AP de fallback\n",
                  (int)st);
    startAP();
    s_net = NetState::Ap;
    return;
  }

  if (now - s_lastLogMs > 2000) {
    s_lastLogMs = now;
    Serial.printf("[webui] STA conectando... (status=%d)\n", (int)st);
  }
}

}  // namespace webui
