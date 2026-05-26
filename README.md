# Accutron Test Set

Modern digital bench supply and current meter for Bulova Accutron tuning fork movements.

Designed for:
- voltage phasing
- low-amplitude testing
- current diagnostics
- general tuning fork movement evaluation

---

# Features

Adjustable low-voltage output with real-time voltage and microamp display.

Built around:
- Arduino Nano
- ADS1115 16-bit ADC
- SSD1306 OLED
- LM4040 precision reference

USB powered for bench use.

---

# Functions

## Adjustable Output Voltage

```text
0.00V → 2.50V
```

Common Accutron phasing voltages:

```text
1.05V
1.35V
1.55V
1.75V
1.80V
```

---

## Microamp Measurement

Current is measured through a 1k precision sense resistor using differential ADC measurement.

Displayed in real-time on the OLED.

---

## Corrected Terminal Voltage

Firmware compensates for voltage drop across the 470Ω output protection resistor:

```text
Vdrop = I × R
```

This allows the displayed voltage to more closely match actual terminal voltage under load.

---

# Hardware

## Main Components

```text
Arduino Nano
ADS1115 ADC Module
SSD1306 OLED Display
LM4040AIZ-2.5
10-turn 10k potentiometer
```

## Passive Components

```text
1k precision sense resistor
470Ω output resistor
2.2k LM4040 bias resistor
10µF capacitors
0.1µF capacitors
```

---

# Notes

This is a bench-powered device intended for hobbyist restoration and diagnostic use.

The design is inspired by original Bulova test equipment but implemented using modern components and digital measurement.

---

# Future Development

Possible future improvements:

```text
Larger OLED
Digital voltage presets
PCB version
Kelvin terminal sensing
Rechargeable battery operation
Dedicated movement adapters
```

---

# Usage & Licensing

This project is shared for:
- personal use
- learning
- hobbyist restoration work

Please do not commercially reproduce or sell this design or derivative versions without permission from the project author.

Community improvements and technical collaboration are encouraged.

---

# Disclaimer

Use at your own risk when testing vintage electronic watch movements.

Always verify polarity and voltage before connecting a movement.
