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
#define DWELL_TIME_MS 200

// ===================== DANH SACH TAN SO SUB-GHZ =====================
// Lay theo phong cach ESP-C3-Pocket-Puter: quet vong toan bo dai tan
const float jam_frequency_list[] = {
  300.000f, 302.757f, 303.875f, 303.900f, 304.250f, 307.000f, 307.500f,
  307.800f, 309.000f, 310.000f, 312.000f, 312.100f, 312.200f, 313.000f,
  313.850f, 314.000f, 314.350f, 314.980f, 315.000f, 318.000f, 330.000f,
  345.000f, 348.000f, 350.000f,
  387.000f, 390.000f, 418.000f, 430.000f, 430.500f, 431.000f, 431.500f,
  433.075f, 433.220f, 433.420f, 433.657f, 433.889f, 433.920f, 434.075f,
  434.177f, 434.190f, 434.390f, 434.420f, 434.620f, 434.775f, 438.900f,
  440.175f, 464.000f, 467.750f,
  779.000f, 868.350f, 868.400f, 868.800f, 868.950f, 906.400f, 915.000f,
  925.000f, 928.000f
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

  Serial.print(F("Jamming ")); Serial.print(jam_frequency_count); Serial.println(F(" frequencies..."));
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
