# Accutron Test Set

Modern digital bench supply and current meter for Bulova Accutron tuning fork movements.

Designed for:

- voltage phasing
- low-amplitude testing
- current diagnostics
- general tuning fork movement evaluation

---

## Features

Adjustable low-voltage output with real-time voltage and microamp display.

Built around:

- Arduino Nano
- ADS1115 16-bit ADC
- SSD1306 OLED
- LM4040 precision reference

USB powered for bench use.

---

## Functions

### Adjustable Output Voltage

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

### Microamp Measurement

Current is measured through a 1k precision sense resistor using ADS1115 measurement across the sense resistor.

```text
A1 = sense high
A0 = sense low

Current = (A1 - A0) / 1000Ω
```

Displayed in real time on the OLED in microamps.

The measured current represents the current flowing through the connected watch movement, not the total current drawn by the tester circuitry.

---

### Actual Terminal Voltage Measurement

PCB V1.6 measures actual output terminal voltage directly at OUTPUT+ using ADS1115 channel A2.

```text
OUTPUT+ → ADS1115 A2
```

The displayed voltage is therefore the actual watch terminal voltage.

Firmware no longer applies software correction for the 470Ω output protection resistor.

Previous method:

```text
terminal voltage = set voltage - calculated 470Ω drop
```

Current PCB V1.6 method:

```text
terminal voltage = A2 voltage reading
```

---

## Hardware

### Main Components

```text
Arduino Nano
ADS1115 ADC Module
SSD1306 OLED Display
LM4040AIZ-2.5 precision reference
10-turn 10k potentiometer
SPST RUN/STOP switch
```

### Passive Components

```text
1k precision sense resistor
470Ω output protection resistor
100k switched-node pull-down resistor
2.2k LM4040 bias resistor
10µF capacitors
0.1µF capacitors
```

---

## Analog Measurement Path

```text
LM4040 2.5V reference
↓
10-turn 10k potentiometer
↓
RUN/STOP switch
↓
A1 / sense high
↓
1k current sense resistor
↓
A0 / sense low
↓
470Ω output protection resistor
↓
OUTPUT+
↓
watch movement
↓
GND
```

Voltage sense:

```text
OUTPUT+ → ADS1115 A2
```

---

## Switched-Node Pull-Down Resistor

PCB V1.6 includes a required 100kΩ pull-down resistor from the RUN/STOP switch output node to ground.

Connection:

```text
SW_OUT / ADS1115 A1 / high side of 1k current-sense resistor
↓
100kΩ
↓
GND
```

When the RUN/STOP switch is open, the switched output chain is otherwise floating:

```text
A1 → 1k sense resistor → A0 → 470Ω protection resistor → OUTPUT+ / ADS1115 A2
```

With no DC reference, this high-impedance node can retain charge or pick up leakage and capacitive coupling from the USB supply, wiring, ADS1115 inputs, display wiring, or nearby circuitry. This can cause false OFF-state voltage readings at OUTPUT+ / A2.

The 100kΩ pull-down provides a weak ground reference and discharge path for the switched node, forcing the output chain near 0V in STOP mode.

The resistor is intentionally placed at the switch output / A1 node, before the 1k current-sense resistor. Its current returns directly to ground and does not pass through the 1k sense resistor, so it is not included in the displayed watch-current measurement.

Do not place this resistor from OUTPUT+ to ground. A resistor at OUTPUT+ would draw current through the 1k sense resistor and would appear as false watch current.

Example:

```text
1.5V / 100kΩ = 15µA
```

If placed at OUTPUT+, that 15µA would be incorrectly displayed as watch current.

---

## Display Behavior

The OLED displays:

```text
terminal voltage
watch current in µA
```

During voltage adjustment, current display is temporarily blanked:

```text
--.- uA
```

After the voltage settles, the firmware discards transient readings and resumes displaying measured current.

This prevents stale or adjustment-related current readings from being shown as valid watch current.

---

## Notes

This is a bench-powered device intended for hobbyist restoration and diagnostic use.

The design is inspired by original Bulova test equipment but implemented using modern components and digital measurement.

The current display is intended to mimic the useful behavior of an analog microamp meter while preserving a true measured current value from the 1k sense resistor.

---

## Future Development

Possible future improvements:

```text
Larger OLED
Digital voltage presets
PCB version
Selectable movement profiles
High-current caution indicator
Kelvin terminal sensing
Rechargeable battery operation
Dedicated movement adapters
```

---

## Usage & Licensing

This project is shared for:

- personal use
- learning
- hobbyist restoration work

Please do not commercially reproduce or sell this design or derivative versions without permission from the project author.

Community improvements and technical collaboration are encouraged.

---

## Disclaimer

Use at your own risk when testing vintage electronic watch movements.

Always verify polarity and voltage before connecting a movement.
