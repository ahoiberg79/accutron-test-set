#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_ADS1115 ads;

const float senseResistor = 1000.0;
const float protectionResistor = 470.0;

const float zeroDeadband_uA = 3.0;

// Detect voltage adjustment
const float voltageMoveThreshold = 0.006;
const int blankLoopsAfterMove = 3;

float previousVoltage = 0.0;
int blankCounter = 0;

void readAveragedPair(float &vA1, float &vA0) {
  long sumA1 = 0;
  long sumA0 = 0;

  const int samples = 50;

  for (int i = 0; i < samples; i++) {
    sumA1 += ads.readADC_SingleEnded(1);
    sumA0 += ads.readADC_SingleEnded(0);
    delay(1);
  }

  vA1 = (sumA1 / (float)samples) * 0.000125;
  vA0 = (sumA0 / (float)samples) * 0.000125;
}

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

  display.clearDisplay();
  display.display();
}

void loop() {
  float vA1, vA0;
  readAveragedPair(vA1, vA0);

  float current_uA = ((vA1 - vA0) / senseResistor) * 1000000.0;

  if (current_uA < zeroDeadband_uA && current_uA > -zeroDeadband_uA) {
    current_uA = 0.0;
  }

  if (current_uA < 0.0) {
    current_uA = 0.0;
  }

  float terminalVoltage =
      vA0 - ((current_uA / 1000000.0) * protectionResistor);

  if (terminalVoltage < 0.0) {
    terminalVoltage = 0.0;
  }

  float voltageChange = abs(terminalVoltage - previousVoltage);

  if (voltageChange > voltageMoveThreshold) {
    blankCounter = blankLoopsAfterMove;
    current_uA = 0.0;   // discard transient calculation
  } else if (blankCounter > 0) {
    blankCounter--;
    current_uA = 0.0;   // keep blanking while settling
  }

  previousVoltage = terminalVoltage;

  char voltageText[8];
  char currentText[8];

  dtostrf(terminalVoltage, 5, 2, voltageText);
  dtostrf(current_uA, 4, 1, currentText);

  display.clearDisplay();
  display.setTextSize(3);

  display.setCursor(3, 11);
  display.print(voltageText);

  display.setCursor(104, 11);
  display.print("V");

  display.setCursor(4, 39);

  if (blankCounter > 0) {
    display.print("--.-");
  } else {
    display.print(currentText);
  }

  display.setCursor(88, 39);
  display.print("uA");

  display.display();

  delay(100);
}
