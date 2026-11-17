// =============================================================================
//  webui.h - Wi-Fi access point and web page
//
//  Join the unit's Wi-Fi network (WIFI_AP_SSID) from a phone or laptop and open
//  http://192.168.4.1 to see status, start/stop logging and download files.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

#if ENABLE_WIFI
#include <WiFi.h>
#include <WebServer.h>
#include "gnss.h"
#include "storage.h"

extern volatile bool reqStart, reqStop;
String statusJson();

static WebServer web(80);

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Smart GCP</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;background:#f3f5f8;color:#14233b}
header{background:#0d3b7a;color:#fff;padding:14px 16px;font-size:20px;font-weight:600}
main{max-width:640px;margin:auto;padding:12px}
.card{background:#fff;border-radius:10px;padding:14px;margin-bottom:12px;box-shadow:0 1px 3px #0002}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px 14px}
.k{font-size:12px;color:#5b6b82}.v{font-size:17px;font-weight:600}
button{font-size:16px;padding:10px 18px;border:0;border-radius:8px;color:#fff;margin-right:8px}
#go{background:#1c8a3c}#halt{background:#b3261e}button:disabled{opacity:.35}
table{width:100%;border-collapse:collapse;font-size:14px}td,th{padding:6px 4px;border-bottom:1px solid #e4e8ee;text-align:left}
a{color:#0d3b7a}.bad{color:#b3261e}.ok{color:#1c8a3c}small{color:#5b6b82}
</style></head><body><header id="hd">Smart GCP</header><main>
<div class="card"><div class="grid">
<div><div class="k">Logging</div><div class="v" id="log">-</div></div>
<div><div class="k">Session file</div><div class="v" id="file">-</div></div>
<div><div class="k">GNSS fix</div><div class="v" id="fix">-</div></div>
<div><div class="k">Satellites / HDOP</div><div class="v" id="sat">-</div></div>
<div><div class="k">Raw data rate</div><div class="v" id="rate">-</div></div>
<div><div class="k">Data errors</div><div class="v" id="err">-</div></div>
<div><div class="k">Approx. position</div><div class="v" id="pos" style="font-size:14px">-</div></div>
<div><div class="k">UTC</div><div class="v" id="utc" style="font-size:14px">-</div></div>
<div><div class="k">SD card</div><div class="v" id="sd">-</div></div>
<div><div class="k">Uptime</div><div class="v" id="up">-</div></div>
</div></div>
<div class="card"><button id="go" onclick="cmd('start')">Start logging</button>
<button id="halt" onclick="cmd('stop')">Stop logging</button>
<p><small>Stop logging before downloading the current session or removing power.</small></p></div>
<div class="card"><b>Files</b><table id="files"><tr><td>loading...</td></tr></table></div>
<small id="fw"></small></main><script>
const $=i=>document.getElementById(i);let cur='';
function kb(b){return b>1048576?(b/1048576).toFixed(2)+' MB':(b/1024).toFixed(1)+' kB'}
async function st(){try{const s=await(await fetch('/api/status')).json();cur=s.logging?s.file:'';
$('hd').textContent='Smart GCP  '+s.id;
$('log').innerHTML=s.logging?'<span class=ok>ON</span> '+kb(s.bytes):'<span class=bad>OFF</span>';
$('file').textContent=s.logging?s.file:'-';
$('fix').innerHTML=s.gnss?(s.fix>0?'<span class=ok>'+s.fixName+'</span>':'<span class=bad>no fix</span>'):'<span class=bad>no data</span>';
$('sat').textContent=s.sats+' / '+s.hdop.toFixed(1);
$('rate').textContent=s.bps+' B/s';
$('err').innerHTML=(s.crcErr+s.sdErr)?'<span class=bad>'+s.crcErr+' CRC, '+s.sdErr+' SD</span>':'<span class=ok>none</span>';
$('pos').textContent=s.fix>0?s.lat.toFixed(6)+', '+s.lon.toFixed(6)+', '+s.alt.toFixed(1)+' m':'-';
$('utc').textContent=s.utc||'-';
$('sd').innerHTML=s.sd?s.sdFreeMB+' MB free':'<span class=bad>not found</span>';
$('up').textContent=Math.floor(s.uptime/60)+' min';
$('go').disabled=s.logging||!s.sd;$('halt').disabled=!s.logging;
$('fw').textContent='Firmware '+s.fw+' | '+s.rx;}catch(e){}}
async function ls(){try{const f=await(await fetch('/api/files')).json();
f.sort((a,b)=>a.n<b.n?1:-1);let h='<tr><th>Name</th><th>Size</th><th></th></tr>';
for(const x of f){const live=cur.endsWith(x.n);
h+='<tr><td>'+(live?x.n+' <small>(recording)</small>':'<a href="/download?f='+x.n+'">'+x.n+'</a>')+'</td><td>'+kb(x.s)+
'</td><td>'+(live?'':'<a href="#" onclick="del(\''+x.n+'\');return false">delete</a>')+'</td></tr>';}
$('files').innerHTML=f.length?h:'<tr><td>No files yet</td></tr>';}catch(e){}}
async function cmd(c){await fetch('/api/'+c,{method:'POST'});setTimeout(()=>{st();ls()},700)}
async function del(n){if(confirm('Delete '+n+'?')){await fetch('/api/delete?f='+n,{method:'POST'});ls()}}
st();ls();setInterval(st,2000);setInterval(ls,10000);
</script></body></html>)HTML";

// Accept only plain file names inside /logs
static bool webSafeName(const String& n) {
  if (n.length() == 0 || n.length() > 40) return false;
  for (size_t i = 0; i < n.length(); i++) {
    char c = n[i];
    if (!(isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.')) return false;
  }
  return n.indexOf("..") < 0;
}

static bool webIsActiveFile(const String& path) {
  return session.active && (path == session.path || path == session.metaPath);
}

static void webDownload() {
  String name = web.arg("f");
  if (!webSafeName(name)) { web.send(400, "text/plain", "bad file name"); return; }
  String path = String(LOG_DIR) + "/" + name;
  if (webIsActiveFile(path)) { web.send(409, "text/plain", "Stop logging before downloading the current session."); return; }

  sdLock();
  File f = SD.open(path, FILE_READ);
  size_t size = f ? f.size() : 0;
  sdUnlock();
  if (!f) { web.send(404, "text/plain", "not found"); return; }

  web.setContentLength(size);
  web.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
  web.send(200, "application/octet-stream", "");
  // Read in small chunks and release the card between chunks so the logger
  // (another session may be recording) never waits long.
  static uint8_t buf[2048];
  size_t sent = 0;
  while (sent < size && web.client().connected()) {
    sdLock();
    int n = f.read(buf, sizeof(buf));
    sdUnlock();
    if (n <= 0) break;
    web.sendContent((const char*)buf, (size_t)n);
    sent += n;
  }
  sdLock(); f.close(); sdUnlock();
}

static void webDelete() {
  String name = web.arg("f");
  if (!webSafeName(name)) { web.send(400, "text/plain", "bad file name"); return; }
  String path = String(LOG_DIR) + "/" + name;
  if (webIsActiveFile(path)) { web.send(409, "text/plain", "file is being recorded"); return; }
  sdLock(); bool ok = SD.remove(path); sdUnlock();
  web.send(ok ? 200 : 404, "text/plain", ok ? "deleted" : "not found");
}

static void webBegin() {
  WiFi.mode(WIFI_AP);
  if (strlen(WIFI_AP_PASS) >= 8) WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
  else                           WiFi.softAP(WIFI_AP_SSID);

  web.on("/", HTTP_GET, []() { web.send_P(200, "text/html", INDEX_HTML); });
  web.on("/api/status", HTTP_GET, []() { web.send(200, "application/json", statusJson()); });
  web.on("/api/files", HTTP_GET, []() { web.send(200, "application/json", storageListJson()); });
  web.on("/api/start", HTTP_POST, []() { reqStart = true; web.send(200, "text/plain", "ok"); });
  web.on("/api/stop", HTTP_POST, []() { reqStop = true; web.send(200, "text/plain", "ok"); });
  web.on("/api/delete", HTTP_POST, webDelete);
  web.on("/download", HTTP_GET, webDownload);
  web.onNotFound([]() { web.send(404, "text/plain", "not found"); });
  web.begin();
}

static inline void webLoop() { web.handleClient(); }

#else
static inline void webBegin() {}
static inline void webLoop() {}
#endif
