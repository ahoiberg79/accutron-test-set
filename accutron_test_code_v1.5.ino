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

float readAvgVoltage(uint8_t channel) {
  long sum = 0;
  const int samples = 100;

  for (int i = 0; i < samples; i++) {
    sum += ads.readADC_SingleEnded(channel);
    delay(2);
  }

  float avgRaw = sum / (float)samples;
  return avgRaw * 0.000125; // GAIN_ONE = 0.125mV/bit
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
  float vA1 = readAvgVoltage(1);  // before 1k sense resistor
  float vA0 = readAvgVoltage(0);  // after 1k sense resistor, before 470R

  float current_uA = ((vA1 - vA0) / senseResistor) * 1000000.0;

  if (current_uA < zeroDeadband_uA && current_uA > -zeroDeadband_uA) {
    current_uA = 0.0;
  }

  if (current_uA < 0.0) {
    current_uA = 0.0;
  }

  // Correct displayed voltage for drop across 470R protection resistor
  float terminalVoltage = vA0 - ((current_uA / 1000000.0) * protectionResistor);

  if (terminalVoltage < 0.0) {
    terminalVoltage = 0.0;
  }

  display.clearDisplay();

  // Voltage line
  display.setTextSize(3);
  display.setCursor(15, 11);
  display.print(terminalVoltage, 2);
  display.print("V");

  // Current number
  display.setTextSize(3);
  display.setCursor(2, 39);
  display.print(current_uA, 1);

  // Locked unit position
  display.setCursor(88, 39);
  display.print("uA");

  display.display();

  delay(250);
}