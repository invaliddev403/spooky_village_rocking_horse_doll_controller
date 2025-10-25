// ESP32-C3 SoftAP Web Controller + /img endpoint
// Board: ESP32C3 Dev Module

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "doll_img.h"

#define RELAY_PIN 2                 // GPIO number (AITRIP "D2")
#define DEFAULT_SSID   "ESP32-Button"
#define DEFAULT_PASS   "press1234"  // 8+ chars

#define DEFAULT_MIN_S     5
#define DEFAULT_MAX_S     15
#define DEFAULT_PRESS_MS  200

Preferences prefs;
WebServer server(80);

struct Settings {
  bool enabled;
  uint32_t min_s;
  uint32_t max_s;
  uint32_t press_ms;
} cfg;

enum PressState { IDLE, PRESSING };
PressState pstate = IDLE;

uint32_t nextTriggerMs = 0;
uint32_t pressEndMs = 0;

void saveSettings() {
  prefs.putBool("enabled",   cfg.enabled);
  prefs.putUInt("min_s",     cfg.min_s);
  prefs.putUInt("max_s",     cfg.max_s);
  prefs.putUInt("press_ms",  cfg.press_ms);
}

void loadSettings() {
  cfg.enabled   = prefs.getBool("enabled", false);
  cfg.min_s     = prefs.getUInt("min_s", DEFAULT_MIN_S);
  cfg.max_s     = prefs.getUInt("max_s", DEFAULT_MAX_S);
  cfg.press_ms  = prefs.getUInt("press_ms", DEFAULT_PRESS_MS);
  if (cfg.min_s < 1) cfg.min_s = 1;
  if (cfg.max_s < cfg.min_s) cfg.max_s = cfg.min_s;
}

// min and max now represent MINUTES, not seconds
uint32_t randMs(uint32_t min_m, uint32_t max_m) {
  uint32_t span = (max_m - min_m + 1);
  uint32_t m = min_m + (esp_random() % span);
  return m * 60UL * 1000UL;  // minutes → milliseconds
}

inline void relayOn()  { digitalWrite(RELAY_PIN, HIGH); }
inline void relayOff() { digitalWrite(RELAY_PIN, LOW);  }
void scheduleNext()    { nextTriggerMs = millis() + randMs(cfg.min_s, cfg.max_s); }
void startPress(uint32_t ms){ relayOn(); pressEndMs = millis() + ms; pstate = PRESSING; }

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>Spooky Village Rocking Horse Doll Controller</title>
<style>
  :root{--ink:#1b1b1d;--muted:#666;--edge:#ddd}
  body{font-family:system-ui,Segoe UI,Arial;margin:18px;max-width:720px;color:var(--ink)}
  .card{border:1px solid var(--edge);border-radius:12px;padding:16px;margin:12px 0;box-shadow:0 1px 4px rgba(0,0,0,.06)}
  h1{font-size:22px;margin:10px 0 14px}
  .pill{display:inline-block;background:#f2ecff;color:#3b2a73;padding:4px 10px;border-radius:999px;margin-left:8px}
  label{display:block;margin:.4rem 0}
  input[type=number]{width:100%;padding:8px;margin:.2rem 0}
  button{padding:10px 14px;border:0;border-radius:10px;cursor:pointer}
  .row{display:flex;gap:10px;flex-wrap:wrap}
  .grow{flex:1;min-width:140px}
  .on{background:#2e7d32;color:#fff}
  .off{background:#b71c1c;color:#fff}
  .ghost{background:#eee}
  .muted{color:var(--muted)}
  .center{display:flex;align-items:center;gap:14px}
  img.doll{width:220px;height:auto;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,.15)}
</style></head>
<body>
<div class="center">
  <img class="doll" alt="Spooky Doll" src="/img">
  <div>
    <h1>Spooky Village Rocking Horse Doll Controller <span id="ip" class="pill"></span></h1>
    <div class="muted">Manual press or randomized intervals—right from this page.</div>
  </div>
</div>

<div class=card>
  <h3>Manual</h3>
  <div class=row>
    <div class=grow>
      <label>Press duration (ms)
        <input id=press value=200 type=number min=50 max=2000 step=10>
      </label>
    </div>
    <div>
      <button id="doPress" class=on>Press</button>
    </div>
  </div>
</div>

<div class=card>
  <h3>Random schedule</h3>
  <div class=row>
    <div class=grow>
      <label>Min gap (min) <input id=min type=number min=1 max=1440 value=5></label>
    </div>
    <div class=grow>
      <label>Max gap (min) <input id=max type=number min=1 max=1440 value=15></label>
    </div>
    <div class=grow>
      <label>Press (ms) <input id=dur type=number min=50 max=2000 step=10 value=200></label>
    </div>
  </div>
  <div class=row>
    <button id="save" class=ghost>Save</button>
    <button id="toggle" class=on>Start</button>
    <div class="muted" id="next"></div>
  </div>
</div>

<div class=card>
  <h3>Status</h3>
  <div id="status" class=muted>Loading…</div>
</div>

<script>
const dirty = { press:false, min:false, max:false, dur:false };

function markDirty(id){ dirty[id] = true; }
function clearDirty(){ for (const k in dirty) dirty[k]=false; }

function setIfIdle(id, val){
  const el = document.getElementById(id);
  if (!el) return;
  if (document.activeElement === el) return;  // user is editing
  if (dirty[id]) return;                      // user changed but not saved yet
  if (String(el.value) !== String(val)) el.value = val;
}

async function fetchStatus(){
  try {
    const r = await fetch('/api/status', {cache:'no-store'});
    if (!r.ok) throw new Error('HTTP '+r.status);
    const j = await r.json();

    // update inputs ONLY if not focused/dirty
    setIfIdle('press', j.press_ms);
    setIfIdle('min',   j.min_s);
    setIfIdle('max',   j.max_s);
    setIfIdle('dur',   j.press_ms);

    // buttons/status always safe to refresh
    const togg = document.getElementById('toggle');
    togg.textContent = j.enabled ? 'Stop' : 'Start';
    togg.className = j.enabled ? 'off' : 'on';

    const minsRemaining = j.next_in_ms >= 0 ? Math.round(j.next_in_ms / 60000) : '—';
    document.getElementById('status').textContent =
      `Enabled: ${j.enabled} · Next in: ${minsRemaining} min · Pressing: ${j.pressing}`;
    document.getElementById('next').textContent =
      j.enabled ? `Next press in ~ ${Math.max(0, minsRemaining)} min` : '';
    document.getElementById('ip').textContent = j.ap_ip || '';
  } catch(e) {
    console.warn('Status fetch failed:', e);
    document.getElementById('status').textContent = "Lost connection… retrying";
  }
}

// retry every second regardless of error
setInterval(fetchStatus, 1000);

// mark inputs dirty when user types; cleared on Save
['press','min','max','dur'].forEach(id=>{
  const el = document.getElementById(id);
  el.addEventListener('input', ()=>markDirty(id));
  el.addEventListener('change', ()=>markDirty(id));
  el.addEventListener('keydown', (e)=>{ if(e.key==='Enter') { e.preventDefault(); e.stopPropagation(); }});
});

function preventReload(ev){ ev.preventDefault(); ev.stopPropagation(); }

document.querySelectorAll('button').forEach(btn=>{
  btn.addEventListener('click', preventReload);
});

document.getElementById('doPress').onclick = async (e)=>{
  preventReload(e);
  const ms = +document.getElementById('press').value || 200;
  await fetch(`/api/press?ms=${ms}`, {cache:'no-store'});
  // don't clear dirty here — manual press doesn't change stored config
  fetchStatus();
};

document.getElementById('save').onclick = async (e)=>{
  preventReload(e);
  const min = +document.getElementById('min').value || 5;
  const max = +document.getElementById('max').value || 15;
  const dur = +document.getElementById('dur').value || 200;
  await fetch(`/api/config?min=${min}&max=${max}&press=${dur}`, {cache:'no-store'});
  clearDirty();          // server now owns truth; allow UI to refresh inputs
  fetchStatus();
};

document.getElementById('toggle').onclick = async (e)=>{
  preventReload(e);
  await fetch('/api/toggle', {cache:'no-store'});
  fetchStatus();
};

fetchStatus();
</script>
</body></html>
)HTML";

// ---- forward declarations for image data (in a separate .h we include below)
extern const uint8_t DOLL_JPG[] PROGMEM;
extern const size_t  DOLL_JPG_LEN;

void handleIndex() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}
void handleImg() {
  server.send_P(200, "image/jpeg", (PGM_P)DOLL_JPG, DOLL_JPG_LEN);
}
void handleStatus() {
  int32_t nextIn = -1;
  if (cfg.enabled && pstate == IDLE) nextIn = (int32_t)(nextTriggerMs - millis());
  else if (pstate == PRESSING) nextIn = 0;

  IPAddress ip = WiFi.softAPIP();
  char buf[384];
  snprintf(buf, sizeof(buf),
    "{\"enabled\":%s,\"min_s\":%u,\"max_s\":%u,\"press_ms\":%u,"
    "\"pressing\":%s,\"next_in_ms\":%d,\"ap_ip\":\"%u.%u.%u.%u\"}",
    cfg.enabled?"true":"false", cfg.min_s, cfg.max_s, cfg.press_ms,
    (pstate==PRESSING)?"true":"false", nextIn,
    ip[0],ip[1],ip[2],ip[3]);
  server.send(200, "application/json", buf);
}
void handlePress() {
  uint32_t ms = cfg.press_ms;
  if (server.hasArg("ms")) ms = constrain(server.arg("ms").toInt(), 50, 5000);
  if (pstate == IDLE) { startPress(ms); scheduleNext(); }
  server.send(200, "text/plain", "OK");
}
void handleConfig() {
  if (server.hasArg("min"))
    cfg.min_s = std::max((uint32_t)1, (uint32_t)server.arg("min").toInt());
  if (server.hasArg("max"))
    cfg.max_s = std::max(cfg.min_s, (uint32_t)server.arg("max").toInt());
  if (server.hasArg("press"))
    cfg.press_ms = constrain(server.arg("press").toInt(), 50, 5000);
  saveSettings(); scheduleNext();
  server.send(200, "text/plain", "OK");
}
void handleToggle() {
  cfg.enabled = !cfg.enabled; saveSettings();
  if (cfg.enabled) scheduleNext();
  server.send(200, "text/plain", cfg.enabled ? "ENABLED" : "DISABLED");
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  relayOff();

  prefs.begin("btn", false);
  loadSettings();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(DEFAULT_SSID, DEFAULT_PASS);
  delay(100);

  server.on("/", HTTP_GET, handleIndex);
  server.on("/img", HTTP_GET, handleImg);           // <-- your image
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/press",  HTTP_GET, handlePress);
  server.on("/api/config", HTTP_GET, handleConfig);
  server.on("/api/toggle", HTTP_GET, handleToggle);
  server.onNotFound([](){ server.send(404, "text/plain", "Not found"); });
  server.begin();

  uint32_t seed = analogRead(A0); seed ^= micros();
  esp_fill_random(&seed, sizeof(seed)); srand(seed);

  if (cfg.enabled) scheduleNext();
}

void loop() {
  server.handleClient();

  const uint32_t now = millis();
  if (pstate == PRESSING && (int32_t)(now - pressEndMs) >= 0) {
    relayOff(); pstate = IDLE;
  }
  if (cfg.enabled && pstate == IDLE && (int32_t)(now - nextTriggerMs) >= 0) {
    startPress(cfg.press_ms); scheduleNext();
  }
}
