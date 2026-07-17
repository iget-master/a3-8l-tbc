#include "webui.h"

#include <WebServer.h>
#include <WiFi.h>

#include "calibration.h"
#include "control.h"
#include "hbridge.h"
#include "pwm_input.h"
#include "settings.h"
#include "tps.h"
#include "version.h"
#include "webui_page.h"

namespace webui {
namespace {

WebServer s_server(80);

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
      "\"analogOut\":%.2f,\"rawTps\":%u,\"cmdDuty\":%.2f,\"cmdFreq\":%.1f,"
      "\"cmdPresent\":%s,\"pidP\":%.2f,\"pidI\":%.2f,\"pidD\":%.2f,"
      "\"manual\":%s,\"cal\":{\"state\":\"%s\",\"rest\":%u,\"min\":%u,"
      "\"max\":%u,\"valid\":%s,\"learnedMax\":%u}}",
      FW_VERSION, (unsigned long)millis(), control::modeName(),
      control::faultReason(), b(control::idleActive()),
      (double)control::setpointPct(), (double)control::positionPct(),
      (double)control::appliedDutyPct(), (double)control::analogOutPct(),
      (unsigned)tps::raw(), (double)pwm_input::dutyPct(),
      (double)pwm_input::freqHz(), b(pwm_input::signalPresent()),
      (double)control::pidP(), (double)control::pidI(),
      (double)control::pidD(), b(control::manualActive()),
      calibration::stateName(), (unsigned)cal.restRaw, (unsigned)cal.minRaw,
      (unsigned)cal.maxRaw, b(cal.valid),
      (unsigned)calibration::learnedMaxRaw());
  sendJsonOrOverflow(buf, n, sizeof(buf));
}

void handleParamsGet() {
  static char buf[1280];
  const Settings& s = settings::get();
  char ssid[6 * sizeof(s.apSsid)];  // pior caso: tudo \u00XX
  char pass[6 * sizeof(s.apPass)];
  jsonEscape(s.apSsid, ssid, sizeof(ssid));
  jsonEscape(s.apPass, pass, sizeof(pass));
  const int n = snprintf(
      buf, sizeof(buf),
      "{\"kp\":%.3f,\"ki\":%.3f,\"kd\":%.3f,\"deadbandPct\":%.2f,"
      "\"maxDutyPct\":%.2f,\"loopHz\":%u,\"cmdTimeoutMs\":%u,"
      "\"cmdStuckHighIs100\":%s,\"pwmFreqHz\":%lu,\"tpsFaultLowRaw\":%u,"
      "\"tpsFaultHighRaw\":%u,\"calSettleMs\":%u,\"calStabilityCounts\":%u,"
      "\"calTimeoutMs\":%lu,\"calMinRangeCounts\":%u,\"calDrivePct\":%.2f,"
      "\"outMinRaw\":%u,\"outMaxRaw\":%u,\"outAutoLearnMax\":%s,"
      "\"idleActiveLow\":%s,\"idleDebounceMs\":%u,"
      "\"apSsid\":\"%s\",\"apPass\":\"%s\"}",
      (double)s.kp, (double)s.ki, (double)s.kd, (double)s.deadbandPct,
      (double)s.maxDutyPct, (unsigned)s.loopHz, (unsigned)s.cmdTimeoutMs,
      b(s.cmdStuckHighIs100), (unsigned long)s.pwmFreqHz,
      (unsigned)s.tpsFaultLowRaw, (unsigned)s.tpsFaultHighRaw,
      (unsigned)s.calSettleMs, (unsigned)s.calStabilityCounts,
      (unsigned long)s.calTimeoutMs, (unsigned)s.calMinRangeCounts,
      (double)s.calDrivePct, (unsigned)s.outMinRaw, (unsigned)s.outMaxRaw,
      b(s.outAutoLearnMax), b(s.idleActiveLow), (unsigned)s.idleDebounceMs,
      ssid, pass);
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
  s.loopHz = (uint16_t)argLong("loopHz", s.loopHz, 20, 1000);
  s.cmdTimeoutMs = (uint16_t)argLong("cmdTimeoutMs", s.cmdTimeoutMs, 20, 10000);
  s.cmdStuckHighIs100 = argBool("cmdStuckHighIs100", s.cmdStuckHighIs100);
  s.pwmFreqHz = (uint32_t)argLong("pwmFreqHz", s.pwmFreqHz, 1000, 40000);
  s.tpsFaultLowRaw = (uint16_t)argLong("tpsFaultLowRaw", s.tpsFaultLowRaw, 0, 4095);
  s.tpsFaultHighRaw = (uint16_t)argLong("tpsFaultHighRaw", s.tpsFaultHighRaw, 0, 4095);
  s.calSettleMs = (uint16_t)argLong("calSettleMs", s.calSettleMs, 50, 10000);
  s.calStabilityCounts = (uint16_t)argLong("calStabilityCounts", s.calStabilityCounts, 1, 1000);
  s.calTimeoutMs = (uint32_t)argLong("calTimeoutMs", s.calTimeoutMs, 500, 60000);
  s.calMinRangeCounts = (uint16_t)argLong("calMinRangeCounts", s.calMinRangeCounts, 10, 4095);
  s.calDrivePct = argFloat("calDrivePct", s.calDrivePct, 10.0f, 100.0f);
  s.outMinRaw = (uint16_t)argLong("outMinRaw", s.outMinRaw, 0, 4095);
  s.outMaxRaw = (uint16_t)argLong("outMaxRaw", s.outMaxRaw, 0, 4095);
  s.outAutoLearnMax = argBool("outAutoLearnMax", s.outAutoLearnMax);
  s.idleActiveLow = argBool("idleActiveLow", s.idleActiveLow);
  s.idleDebounceMs = (uint16_t)argLong("idleDebounceMs", s.idleDebounceMs, 0, 500);

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

void handleCal() {
  control::requestCalibration();  // efetiva só se em idle
  sendOk();
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

}  // namespace

void begin() {
  const Settings& s = settings::get();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(s.apSsid, s.apPass);

  s_server.on("/", HTTP_GET, handleRoot);
  s_server.on("/api/status", HTTP_GET, handleStatus);
  s_server.on("/api/params", HTTP_GET, handleParamsGet);
  s_server.on("/api/params", HTTP_POST, handleParamsPost);
  s_server.on("/api/manual", HTTP_POST, handleManual);
  s_server.on("/api/cal", HTTP_POST, handleCal);
  s_server.on("/api/defaults", HTTP_POST, handleDefaults);
  s_server.onNotFound([]() { s_server.send(404, "text/plain", "not found"); });
  s_server.begin();
}

void loop() { s_server.handleClient(); }

}  // namespace webui
