// 🕯️ Spooky Village Rocking Horse Doll Controller
// Board: ESP32-C3 Dev Module
// Version: 2025-10-25 (C3 deep sleep wake via GPIO)

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "esp_sleep.h"          // deep sleep APIs (GPIO wake in this build)
#include "doll_img.h"           // contains DOLL_JPG[] + DOLL_JPG_LEN
#include <Adafruit_NeoPixel.h>

// -------- Pin + Config --------
#define RELAY_PIN        2        // drives AQV212 via 470R
#define WAKE_PIN         4        // RTC IO on ESP32-C3; button to GND to escape sleep
#define DEFAULT_SSID     "ESP32-Button"
#define DEFAULT_PASS     "press1234"

#define DEFAULT_MIN_M     5
#define DEFAULT_MAX_M     15
#define DEFAULT_PRESS_MS  200

#define RGB_LED_PIN 23   // <- set to the board’s RGB DIN pin if present
#define RGB_COUNT   1

Preferences prefs;
WebServer server(80);

struct Settings {
  bool enabled;
  uint32_t min_m;
  uint32_t max_m;
  uint32_t press_ms;
} cfg;

enum PressState { IDLE, PRESSING };
PressState pstate = IDLE;

uint32_t nextTriggerMs = 0;
uint32_t pressEndMs = 0;

Adafruit_NeoPixel rgb(RGB_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------- Helpers ----------

void initRgbOff() {
  rgb.begin();
  rgb.clear();   // all zeros
  rgb.show();    // latch OFF
}

void saveSettings() {
  prefs.putBool("enabled", cfg.enabled);
  prefs.putUInt("min_m",   cfg.min_m);
  prefs.putUInt("max_m",   cfg.max_m);
  prefs.putUInt("press_ms",cfg.press_ms);
}

void loadSettings() {
  cfg.enabled   = prefs.getBool("enabled", false);
  cfg.min_m     = prefs.getUInt("min_m", DEFAULT_MIN_M);
  cfg.max_m     = prefs.getUInt("max_m", DEFAULT_MAX_M);
  cfg.press_ms  = prefs.getUInt("press_ms", DEFAULT_PRESS_MS);
  if (cfg.min_m < 1) cfg.min_m = 1;
  if (cfg.max_m < cfg.min_m) cfg.max_m = cfg.min_m;
}

// minutes → random ms in [min,max]
uint32_t randMs(uint32_t min_m, uint32_t max_m) {
  uint32_t span = (max_m - min_m + 1);
  uint32_t m = min_m + (esp_random() % span);
  return m * 60UL * 1000UL;
}

inline void relayOn()  { digitalWrite(RELAY_PIN, HIGH); }
inline void relayOff() { digitalWrite(RELAY_PIN, LOW);  }
void scheduleNext()    { nextTriggerMs = millis() + randMs(cfg.min_m, cfg.max_m); }
void startPress(uint32_t ms) { relayOn(); pressEndMs = millis() + ms; pstate = PRESSING; }

// ---------- HTML ----------
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>Spooky Doll Controller</title>
<style>
:root{--ink:#1b1b1d;--muted:#666;--edge:#ddd}
body{font-family:system-ui;margin:18px;max-width:720px;color:var(--ink)}
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
  <img class="doll" src="/img" alt="Spooky Doll">
  <div>
    <h1>Spooky Rocking Horse Doll <span id="ip" class="pill"></span></h1>
    <div class="muted">Manual press or randomized intervals — right from this page.</div>
  </div>
</div>
<div class="card">
  <h3>Manual</h3>
  <div class="row">
    <div class="grow">
      <label>Press duration (ms)<input id="press" value="200" type="number" min="50" max="2000" step="10"></label>
    </div>
    <div><button id="doPress" class="on">Press</button></div>
  </div>
</div>
<div class="card">
  <h3>Random schedule</h3>
  <div class="row">
    <div class="grow"><label>Min gap (min)<input id="min" value="5" type="number" min="1" max="1440"></label></div>
    <div class="grow"><label>Max gap (min)<input id="max" value="15" type="number" min="1" max="1440"></label></div>
    <div class="grow"><label>Press (ms)<input id="dur" value="200" type="number" min="50" max="2000"></label></div>
  </div>
  <div class="row">
    <button id="save" class="ghost">Save</button>
    <button id="toggle" class="on">Start</button>
    <div class="muted" id="next"></div>
  </div>
</div>
<div class="card"><h3>Status</h3><div id="status" class="muted">Loading…</div></div>
<script>
async function fetchStatus(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw 0;
    const j=await r.json();
    const t=document.getElementById('toggle');
    t.textContent=j.enabled?'Stop':'Start';
    t.className=j.enabled?'off':'on';
    const mins=j.next_in_ms>=0?Math.round(j.next_in_ms/60000):'—';
    document.getElementById('status').textContent=
      `Enabled:${j.enabled} · Next in:${mins} min · Pressing:${j.pressing}`;
    document.getElementById('next').textContent=
      j.enabled?`Next press in ~${Math.max(0,mins)} min`:'';
    document.getElementById('ip').textContent=j.ap_ip||'';
  }catch(e){document.getElementById('status').textContent='Lost connection…';}
}
setInterval(fetchStatus,1000);
document.getElementById('doPress').onclick=async()=>{
  const ms=+document.getElementById('press').value||200;
  await fetch(`/api/press?ms=${ms}`);fetchStatus();};
document.getElementById('save').onclick=async()=>{
  const min=+document.getElementById('min').value||5;
  const max=+document.getElementById('max').value||15;
  const dur=+document.getElementById('dur').value||200;
  await fetch(`/api/config?min=${min}&max=${max}&press=${dur}`);fetchStatus();};
document.getElementById('toggle').onclick=async()=>{
  await fetch('/api/toggle');fetchStatus();};
fetchStatus();
</script></body></html>
)HTML";

// ---------- API ----------
void handleIndex(){ server.send_P(200,"text/html; charset=utf-8",INDEX_HTML); }
void handleImg(){ server.send_P(200,"image/jpeg",(PGM_P)DOLL_JPG,DOLL_JPG_LEN); }
void handleStatus(){
  int32_t nextIn=-1;
  if(cfg.enabled && pstate==IDLE) nextIn=(int32_t)(nextTriggerMs-millis());
  else if(pstate==PRESSING) nextIn=0;
  IPAddress ip=WiFi.softAPIP();
  char buf[384];
  snprintf(buf,sizeof(buf),
    "{\"enabled\":%s,\"min_s\":%u,\"max_s\":%u,\"press_ms\":%u,"
    "\"pressing\":%s,\"next_in_ms\":%d,\"ap_ip\":\"%u.%u.%u.%u\"}",
    cfg.enabled?"true":"false",cfg.min_m,cfg.max_m,cfg.press_ms,
    (pstate==PRESSING)?"true":"false",nextIn,ip[0],ip[1],ip[2],ip[3]);
  server.send(200,"application/json",buf);
}
void handlePress(){
  uint32_t ms=cfg.press_ms;
  if(server.hasArg("ms")) ms=constrain(server.arg("ms").toInt(),50,5000);
  if(pstate==IDLE){ startPress(ms); scheduleNext(); }
  server.send(200,"text/plain","OK");
}
void handleConfig(){
  if(server.hasArg("min"))   cfg.min_m   = std::max((uint32_t)1,(uint32_t)server.arg("min").toInt());
  if(server.hasArg("max"))   cfg.max_m   = std::max(cfg.min_m,(uint32_t)server.arg("max").toInt());
  if(server.hasArg("press")) cfg.press_ms= constrain(server.arg("press").toInt(),50,5000);
  saveSettings(); scheduleNext();
  server.send(200,"text/plain","OK");
}
void handleToggle(){
  cfg.enabled=!cfg.enabled; saveSettings();
  if(cfg.enabled) scheduleNext();
  server.send(200,"text/plain",cfg.enabled?"ENABLED":"DISABLED");
}

// ---------- Setup ----------
void setup(){
  pinMode(RELAY_PIN,OUTPUT);
  relayOff();
  pinMode(WAKE_PIN,INPUT_PULLUP);

  // If using RGB, keep it dark
  #if (RGB_COUNT > 0)
    initRgbOff();
  #endif

  prefs.begin("btn",false);
  loadSettings();

  // Check wake pin to skip deep sleep (Option B)
  bool wakeHeld = (digitalRead(WAKE_PIN)==LOW);

  if(!wakeHeld && cfg.enabled){
    // Deep sleep random mode: sleep until next random press
    uint64_t sleepMs = randMs(cfg.min_m,cfg.max_m);

    // Timer wake
    esp_sleep_enable_timer_wakeup((uint64_t)sleepMs*1000ULL);

    // GPIO deep-sleep wake on WAKE_PIN LOW (requires RTC-IO pin; GPIO4 is OK on C3)
    if (esp_sleep_is_valid_wakeup_gpio((gpio_num_t)WAKE_PIN)) {
      esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKE_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    }

    // Radios off then sleep
    WiFi.mode(WIFI_OFF);
    #if defined(CONFIG_BT_ENABLED) || defined(BLUEDROID_ENABLED) || defined(CONFIG_BT_BLUEDROID_ENABLED)
      btStop();
    #endif
    esp_deep_sleep_start();
  }

  // Otherwise: normal web control mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(DEFAULT_SSID,DEFAULT_PASS);
  delay(100);

  server.on("/",HTTP_GET,handleIndex);
  server.on("/img",HTTP_GET,handleImg);
  server.on("/api/status",HTTP_GET,handleStatus);
  server.on("/api/press",HTTP_GET,handlePress);
  server.on("/api/config",HTTP_GET,handleConfig);
  server.on("/api/toggle",HTTP_GET,handleToggle);
  server.begin();

  // RNG seed
  // uint32_t seed = analogRead(A0); seed ^= micros();
  // esp_fill_random(&seed,sizeof(seed)); srand(seed);

  if(cfg.enabled) scheduleNext();
}

// ---------- Loop ----------
void loop(){
  server.handleClient();
  uint32_t now=millis();

  if(pstate==PRESSING && (int32_t)(now-pressEndMs)>=0){
    relayOff(); pstate=IDLE;
  }
  if(cfg.enabled && pstate==IDLE && (int32_t)(now-nextTriggerMs)>=0){
    startPress(cfg.press_ms); scheduleNext();
  }
}
