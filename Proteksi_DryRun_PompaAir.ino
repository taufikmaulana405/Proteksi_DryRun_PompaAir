// Proteksi Dry Run Pompa Air

// Pin Sensor aliran FS300A (input interrupt)
// Pin Relay pompa (output digital)
const byte FLOW_PIN  = 2;
const byte RELAY_PIN = 8;

// Relay Ouput Signal
const byte RELAY_ON  = HIGH;
const byte RELAY_OFF = LOW;

// Asumsi awal FS300A: frekuensi (Hz) = 5.5 x debit (L/min).
const float FLOW_FACTOR = 5.5f;
const float MIN_FLOW_LPM = 7.0f;

// true: tampilkan debit setiap detik pada semua status.
// false: tampilkan debit hanya saat STARTING atau RUNNING.
const bool ALWAYS_SHOW_FLOW = true;

// Waktu dalam milidetik
const unsigned long START_TIMEOUT_MS = 30000UL; // Batas waktu menunggu aliran saat STARTING
const unsigned long LOW_FLOW_MS      = 3000UL; // Batas waktu aliran rendah saat RUNNING
const unsigned long MAX_RUN_MS       = 30UL * 60UL * 1000UL; // Batas waktu operasi pompa
const unsigned long REST_MS          = 15UL * 60UL * 1000UL; // Batas waktu istirahat (RESTING)
const unsigned long SAMPLE_MS        = 1000UL; // Interval sampling sensor aliran

// Status Pompa
enum State {
  RESTING,
  STARTING,
  RUNNING
};
State state = RESTING; // Definisi awal status pompa

// Variabel global untuk menghitung pulsa sensor aliran
volatile unsigned long pulseCount = 0;

// Variabel waktu untuk menghitung durasi dan interval
unsigned long restStartedAt = 0;
unsigned long pumpStartedAt = 0;
unsigned long lastSampleAt  = 0;
unsigned long lowFlowDuration = 0;

// Variabel untuk menghitung total volume air
float cycleVolumeLiters = 0.0f;

// Fungsi interrupt untuk menghitung pulsa sensor aliran
void countPulse() {
  pulseCount++;
}

// Fungsi untuk membaca dan menghapus jumlah pulsa sensor aliran
unsigned long readAndClearPulses() {
  noInterrupts();
  unsigned long count = pulseCount;
  pulseCount = 0;
  interrupts();

  // Mengembalikan jumlah pulsa yang dibaca sebelum dihapus.
  return count;
}

// Fungsi untuk mematikan pompa dan mengatur status ke RESTING
// Input: now = waktu saat ini (millis), reason = alasan mematikan pompa
void stopPump(unsigned long now, const __FlashStringHelper *reason) {
  digitalWrite(RELAY_PIN, RELAY_OFF);

  state = RESTING;
  restStartedAt = now;
  lowFlowDuration = 0;

  Serial.print(F("Pompa OFF: "));
  Serial.println(reason);
  Serial.println(F("Istirahat 30 menit."));
}

// Fungsi untuk memulai pompa dan mengatur status ke STARTING
// Input: now = waktu saat ini (millis)
void startPump(unsigned long now) {
  readAndClearPulses();

  // Reset total air setiap memulai siklus pompa.
  cycleVolumeLiters = 0.0f;

  pumpStartedAt = now;
  lastSampleAt = now;
  lowFlowDuration = 0;
  state = STARTING;

  digitalWrite(RELAY_PIN, RELAY_ON);
  Serial.println(F("Pompa ON: total air direset, menunggu aliran."));
}

// Fungsi untuk menampilkan countdown waktu dalam format mm:ss
// Input: remainingMs = waktu tersisa dalam milidetik
void printCountdown(unsigned long remainingMs) {
  // Bulatkan ke atas agar tidak menampilkan 00:00 terlalu awal.
  unsigned long seconds = remainingMs / 1000UL;
  if (remainingMs % 1000UL != 0) {
    seconds++;
  }

  unsigned long minutes = seconds / 60UL;
  seconds %= 60UL;

  if (minutes < 10) Serial.print('0');
  Serial.print(minutes);
  Serial.print(':');
  if (seconds < 10) Serial.print('0');
  Serial.print(seconds);
}

// Fungsi untuk menghitung waktu tersisa dari durasi dan waktu yang telah berlalu
// Input: duration = durasi total (ms), elapsed = waktu yang telah berlalu (ms)
// Output: waktu tersisa (ms)
unsigned long remainingTime(unsigned long duration, unsigned long elapsed) {
  if (elapsed >= duration) {
    return 0UL;
  } else {
    return duration - elapsed;
  }
}

// Fungsi untuk memperbarui status LED
// Input: now = waktu saat ini (millis)
void updateStatusLed(unsigned long now) {
  static State previousState = RESTING;
  static unsigned long lastBlinkAt = 0;
  static bool ledOn = false;

  // Mulai pola baru ketika status berubah.
  if (state != previousState) {
    previousState = state;
    lastBlinkAt = now;
    ledOn = true;

    digitalWrite(LED_BUILTIN, HIGH);
  }

  // RUNNING: LED menyala terus.
  if (state == RUNNING) {
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }

  unsigned long blinkInterval;

  if (state == STARTING) {
    blinkInterval = 250UL;
  } else {
    // RESTING
    blinkInterval = 1000UL;
  }

  if (now - lastBlinkAt >= blinkInterval) {
    lastBlinkAt = now;

    if (ledOn) {
      ledOn = false;
      digitalWrite(LED_BUILTIN, LOW);
    } else {
      ledOn = true;
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
}

void setup() {
  // Inisialisasi pin relay
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT);

  // Inisialisasi pin LED bawaan
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Inisialisasi pin sensor aliran
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(
    digitalPinToInterrupt(FLOW_PIN),
    countPulse,
    RISING
  );

  // Inisialisasi Serial Monitor
  Serial.begin(9600);

  // Langsung STARTING dan aktifkan relay saat Arduino menyala.
  startPump(millis());
}

void loop() {
  // Ambil waktu saat ini dalam milidetik
  unsigned long now = millis();

  // Perbarui status LED sesuai status pompa.
  updateStatusLed(now);

  // Sampling dan laporan Serial setiap detik pada semua status.
  if (now - lastSampleAt >= SAMPLE_MS) {
    unsigned long elapsed = now - lastSampleAt;
    unsigned long pulses = readAndClearPulses();
    lastSampleAt = now;

    // Hitung debit air dalam L/min
    float frequencyHz = pulses * 1000.0f / elapsed;
    float flowLpm = frequencyHz / FLOW_FACTOR;

    // Hitung volume air hanya selama siklus pompa aktif.
    if (state == STARTING || state == RUNNING) {
      float volumeThisSample = pulses / (FLOW_FACTOR * 60.0f);
      cycleVolumeLiters += volumeThisSample;
    }

    // Evaluasi apakah aliran air memenuhi batas minimum.
    bool flowOK = (flowLpm >= MIN_FLOW_LPM);

    // Evaluasi aliran hanya ketika relay diperintahkan ON.
    if (state == STARTING) {
      if (flowOK) {
        state = RUNNING;
        lowFlowDuration = 0;
        Serial.println(F("Aliran terdeteksi."));
      }
    } else if (state == RUNNING) {
      if (flowOK) {
        lowFlowDuration = 0;
      } else {
        lowFlowDuration += elapsed;

        if (lowFlowDuration >= LOW_FLOW_MS) {
          stopPump(now, F("Aliran rendah / tidak ada"));
        }
      }
    }

    Serial.print(F("Status: "));

    switch (state) {
      case STARTING:
        Serial.print(F("STARTING | Batas deteksi aliran: "));
        printCountdown(remainingTime(START_TIMEOUT_MS, now - pumpStartedAt));
        break;

      case RUNNING:
        Serial.print(F("RUNNING | Sisa operasi: "));
        printCountdown(remainingTime(MAX_RUN_MS, now - pumpStartedAt));

        if (lowFlowDuration > 0) {
          Serial.print(F(" | Debit rendah, OFF dalam: "));
          printCountdown(remainingTime(LOW_FLOW_MS, lowFlowDuration));
        }
        break;

      case RESTING:
        Serial.print(F("RESTING | Coba ON dalam: "));
        printCountdown(remainingTime(REST_MS, now - restStartedAt));
        break;
    }

    if (ALWAYS_SHOW_FLOW || state != RESTING) {
      Serial.print(F(" | Debit: "));
      Serial.print(flowLpm, 2);
      Serial.print(F(" L/menit"));
    }

    // Total air selalu ditampilkan, termasuk selama RESTING.
    Serial.print(F(" | Total siklus: "));
    Serial.print(cycleVolumeLiters, 2);
    Serial.print(F(" L"));

    Serial.println();
  }

  if (state == RESTING) {
    if (now - restStartedAt >= REST_MS) {
      startPump(now);
    }
    return;
  }

  if (now - pumpStartedAt >= MAX_RUN_MS) {
    stopPump(now, F("Batas operasi 30 menit tercapai"));
    return;
  }

  if (state == STARTING && now - pumpStartedAt >= START_TIMEOUT_MS) {
    stopPump(now, F("Aliran awal tidak terdeteksi"));
  }
}