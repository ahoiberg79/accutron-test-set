#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_ADS1115 ads;

// PCB V1.6 ADS1115 channel mapping
const uint8_t CH_SENSE_LO = 0;  // A0: after 1k sense resistor
const uint8_t CH_SENSE_HI = 1;  // A1: before 1k sense resistor
const uint8_t CH_OUTPUT   = 2;  // A2: actual output terminal voltage

// Hardware values
const float senseResistor_ohms = 1000.0;

// ADS1115 GAIN_ONE = +/-4.096V range = 0.125mV per count
const float ADS_VOLTS_PER_COUNT = 0.000125;

// -----------------------------
// Current measurement settings
// -----------------------------

// Paired A1/A0 samples per current block.
// Higher = more stable but slower response.
const int currentBlockSamples = 32;

// Current rolling average buffer.
// Higher = more analog-meter-like damping.
const int currentBufferSize = 8;

float currentBuffer[currentBufferSize];
int currentBufferIndex = 0;
int currentBufferCount = 0;

// Final display damping after rolling average.
// 0.10 to 0.18 is a useful range.
// Lower = steadier/slower. Higher = faster/jumpier.
const float currentDisplaySmoothing = 0.12;

float displayedCurrent_uA = 0.0;
float filteredCurrent_uA = 0.0;

// Noise cleanup
const float zeroDeadband_uA = 0.35;

// If a single current block jumps this much, require persistence.
// This prevents one bad block from moving the display hard.
const float currentJumpThreshold_uA = 5.0;
const int requiredJumpPersistence = 3;
int jumpPersistenceCounter = 0;
float pendingJumpCurrent_uA = 0.0;

// High-current detection.
// This should eventually be movement-selectable.
// For 218/219, 10uA is a practical warning threshold.
const float highCurrentThreshold_uA = 10.0;
const int highCurrentPersistenceBlocks = 4;
int highCurrentCounter = 0;
bool highCurrentFlag = false;

// -----------------------------
// Voltage / adjustment blanking
// -----------------------------

// Voltage change required to treat pot as being adjusted.
// 0.006V = 6mV.
const float voltageMoveThreshold = 0.006;

// Time after last detected voltage movement before current is shown again.
const unsigned long currentReacquire_ms = 500;

float previousVoltage = 0.0;
unsigned long lastVoltageMove_ms = 0;
bool currentDisplayValid = false;

// -----------------------------
// Function declarations
// -----------------------------

float readOutputVoltage();
float readCurrentBlock_uA();
void resetCurrentFilter(float seedCurrent_uA);
float averageCurrentBuffer();
float processCurrentForDisplay(float rawCurrent_uA);
void updateHighCurrentFlag(float rawCurrent_uA);
void drawDisplay(float terminalVoltage, float current_uA, bool showCurrent);
void printDebug(float vOutput, float rawCurrent_uA, float filteredCurrent_uA, bool showCurrent);

// -----------------------------
// Setup
// -----------------------------

void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!ads.begin()) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("ADS1115 FAIL");
    display.display();
    while (true);
  }

  ads.setGain(GAIN_ONE);

  // Initialize timing so current does not appear instantly on boot.
  lastVoltageMove_ms = millis();

  display.clearDisplay();
  display.display();
}

// -----------------------------
// Main loop
// -----------------------------

void loop() {
  // 1. Read actual output terminal voltage from A2.
  //    This is separate from A1/A0 current sampling.
  float terminalVoltage = readOutputVoltage();

  // 2. Detect pot movement / output voltage adjustment.
  float voltageChange = fabs(terminalVoltage - previousVoltage);

  if (voltageChange > voltageMoveThreshold) {
    lastVoltageMove_ms = millis();
    currentDisplayValid = false;
    currentBufferCount = 0;
    currentBufferIndex = 0;
    jumpPersistenceCounter = 0;
    highCurrentCounter = 0;
    highCurrentFlag = false;
  }

  previousVoltage = terminalVoltage;

  // 3. Always measure current as a fresh paired A1/A0 block.
  //    But do not always feed it into the display filter.
  float rawCurrent_uA = readCurrentBlock_uA();

  // 4. Decide whether current display is allowed.
  bool voltageRecentlyMoved =
    (millis() - lastVoltageMove_ms) < currentReacquire_ms;

  bool showCurrent = !voltageRecentlyMoved;

  if (!showCurrent) {
    // During adjustment / reacquire:
    // - do not update filter
    // - do not show stale current
    currentDisplayValid = false;
    currentBufferCount = 0;
    currentBufferIndex = 0;
    jumpPersistenceCounter = 0;
  } else {
    // First valid current after voltage has settled.
    if (!currentDisplayValid) {
      resetCurrentFilter(rawCurrent_uA);
      displayedCurrent_uA = rawCurrent_uA;
      filteredCurrent_uA = rawCurrent_uA;
      currentDisplayValid = true;
    } else {
      filteredCurrent_uA = processCurrentForDisplay(rawCurrent_uA);

      displayedCurrent_uA =
        (displayedCurrent_uA * (1.0 - currentDisplaySmoothing)) +
        (filteredCurrent_uA * currentDisplaySmoothing);
    }

    updateHighCurrentFlag(rawCurrent_uA);
  }

  drawDisplay(terminalVoltage, displayedCurrent_uA, showCurrent);
  printDebug(terminalVoltage, rawCurrent_uA, filteredCurrent_uA, showCurrent);

  delay(100);
}

// -----------------------------
// Read A2 output voltage
// -----------------------------

float readOutputVoltage() {
  // Dummy read after mux switch.
  // This helps avoid stale ADS1115 mux residue.
  ads.readADC_SingleEnded(CH_OUTPUT);
  delay(8);

  const int samples = 16;
  long sum = 0;

  for (int i = 0; i < samples; i++) {
    sum += ads.readADC_SingleEnded(CH_OUTPUT);
    delay(1);
  }

  float rawAverage = sum / (float)samples;
  float voltage = rawAverage * ADS_VOLTS_PER_COUNT;

  if (voltage < 0.0) {
    voltage = 0.0;
  }

  return voltage;
}

// -----------------------------
// Read true watch current
// -----------------------------

float readCurrentBlock_uA() {
  long sumHi = 0;
  long sumLo = 0;

  // Paired sampling:
  // A1, A0, A1, A0...
  //
  // This is important because current is the instantaneous
  // difference across the 1k sense resistor.
  for (int i = 0; i < currentBlockSamples; i++) {
    sumHi += ads.readADC_SingleEnded(CH_SENSE_HI);
    sumLo += ads.readADC_SingleEnded(CH_SENSE_LO);
    delay(1);
  }

  float vSenseHi = (sumHi / (float)currentBlockSamples) * ADS_VOLTS_PER_COUNT;
  float vSenseLo = (sumLo / (float)currentBlockSamples) * ADS_VOLTS_PER_COUNT;

  float current_uA =
    ((vSenseHi - vSenseLo) / senseResistor_ohms) * 1000000.0;

  // Clamp very small ADC noise to zero.
  if (current_uA > -zeroDeadband_uA && current_uA < zeroDeadband_uA) {
    current_uA = 0.0;
  }

  // Reverse current is not meaningful for this tester display.
  if (current_uA < 0.0) {
    current_uA = 0.0;
  }

  return current_uA;
}

// -----------------------------
// Current filter reset
// -----------------------------

void resetCurrentFilter(float seedCurrent_uA) {
  currentBufferIndex = 0;
  currentBufferCount = currentBufferSize;

  for (int i = 0; i < currentBufferSize; i++) {
    currentBuffer[i] = seedCurrent_uA;
  }

  filteredCurrent_uA = seedCurrent_uA;
  displayedCurrent_uA = seedCurrent_uA;

  jumpPersistenceCounter = 0;
  pendingJumpCurrent_uA = seedCurrent_uA;
}

// -----------------------------
// Rolling average
// -----------------------------

float averageCurrentBuffer() {
  if (currentBufferCount <= 0) {
    return 0.0;
  }

  float sum = 0.0;

  for (int i = 0; i < currentBufferCount; i++) {
    sum += currentBuffer[i];
  }

  return sum / (float)currentBufferCount;
}

// -----------------------------
// Current processing
// -----------------------------

float processCurrentForDisplay(float rawCurrent_uA) {
  float currentAverageBefore = averageCurrentBuffer();
  float delta = fabs(rawCurrent_uA - currentAverageBefore);

  bool largeJump = delta > currentJumpThreshold_uA;

  if (largeJump) {
    // If current suddenly changes by a large amount,
    // do not immediately trust one block.
    //
    // A real high-current condition will persist.
    if (jumpPersistenceCounter == 0) {
      pendingJumpCurrent_uA = rawCurrent_uA;
      jumpPersistenceCounter = 1;
      return currentAverageBefore;
    }

    if (fabs(rawCurrent_uA - pendingJumpCurrent_uA) <= currentJumpThreshold_uA) {
      jumpPersistenceCounter++;
    } else {
      pendingJumpCurrent_uA = rawCurrent_uA;
      jumpPersistenceCounter = 1;
      return currentAverageBefore;
    }

    if (jumpPersistenceCounter < requiredJumpPersistence) {
      return currentAverageBefore;
    }

    // Persistent jump accepted.
    jumpPersistenceCounter = 0;
  } else {
    jumpPersistenceCounter = 0;
  }

  currentBuffer[currentBufferIndex] = rawCurrent_uA;
  currentBufferIndex++;

  if (currentBufferIndex >= currentBufferSize) {
    currentBufferIndex = 0;
  }

  if (currentBufferCount < currentBufferSize) {
    currentBufferCount++;
  }

  return averageCurrentBuffer();
}

// -----------------------------
// High-current detection
// -----------------------------

void updateHighCurrentFlag(float rawCurrent_uA) {
  if (rawCurrent_uA > highCurrentThreshold_uA) {
    highCurrentCounter++;

    if (highCurrentCounter >= highCurrentPersistenceBlocks) {
      highCurrentFlag = true;
    }
  } else {
    highCurrentCounter = 0;
    highCurrentFlag = false;
  }
}

// -----------------------------
// OLED display
// -----------------------------

void drawDisplay(float terminalVoltage, float current_uA, bool showCurrent) {
  char voltageText[8];
  char currentText[8];

  dtostrf(terminalVoltage, 5, 2, voltageText);
  dtostrf(current_uA, 4, 1, currentText);

  display.clearDisplay();
  display.setTextSize(3);

  // Voltage
  display.setCursor(3, 11);
  display.print(voltageText);

  display.setCursor(104, 11);
  display.print("V");

  // Current
  display.setCursor(4, 39);

  if (!showCurrent) {
    display.print("--.-");
  } else {
    display.print(currentText);
  }

  display.setCursor(88, 39);
  display.print("uA");

  // Layout preserved.
  // Optional future use:
  // You could add a small warning mark using text size 1,
  // but this is intentionally disabled to keep the current layout clean.
  //
  // if (highCurrentFlag) {
  //   display.setTextSize(1);
  //   display.setCursor(104, 56);
  //   display.print("HIGH");
  // }

  display.display();
}

// -----------------------------
// Serial debug
// -----------------------------

void printDebug(float vOutput, float rawCurrent_uA, float filteredCurrent_uA, bool showCurrent) {
  Serial.print("A2=");
  Serial.print(vOutput, 4);

  Serial.print(" V  RAW=");
  Serial.print(rawCurrent_uA, 2);

  Serial.print(" uA  FILT=");
  Serial.print(filteredCurrent_uA, 2);

  Serial.print(" uA  DISP=");
  Serial.print(displayedCurrent_uA, 2);

  Serial.print(" uA  SHOW=");
  Serial.print(showCurrent ? "YES" : "NO");

  Serial.print("  HIGH=");
  Serial.println(highCurrentFlag ? "YES" : "NO");
}
