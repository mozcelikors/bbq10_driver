# Blackberry Q10 Keyboard STM32 and Linux Kernel Drivers

A fun weekend project to turn an **STM32F411CEU6 ("Black Pill")** into an **I²C slave device** that drives a **Blackberry Q10 keyboard** (BBQ10).  
The firmware detects key presses, generates an interrupt pulse, and sends the pressed character over I²C when the master performs a read.

> Developed by **@mozcelikors** in **November 2025**.
> Special thanks to **@arturo182** and many people prior for reversing this keyboard. Details: [arturo182/BBQ10KBD](https://github.com/arturo182/BBQ10KBD)

<p align="center">
  <img src="https://raw.githubusercontent.com/mozcelikors/bbq10_driver/main/pictures/1.jpg" alt="Demo" width="800" height="600">
  <img src="https://raw.githubusercontent.com/mozcelikors/bbq10_driver/main/pictures/2.png" alt="Communication" width="800" height="480">
  <img src="https://raw.githubusercontent.com/mozcelikors/bbq10_driver/main/pictures/3.png" alt="Linux Driver Output" width="800" height="160">
</p>

---

## Overview

- Two drivers are presented:
  - **A STM32 driver:** Scans the keyboard matrix, produces interrupt pulse and responds to master I2C requests
  - **A Linux kernel driver:** The driver talks to Linux input subsystem in order to emulate key presses and key releases so that our blackberry keyboard is acting like an actual keyboard.
- The STM32 acts as an **I²C slave**.
- When a key is pressed:
  - The firmware generates a **2 ms rising-edge pulse** on the `IRQ_KEYCHANGED` pin.
  - The I²C master receives the interrupt and reads from the slave.
  - The driver sends the **pressed character** over I²C in response.
- Keys like **Alt**, **RShift**, and **LShift** act as **mode keys** — they must be pressed *before* the actual key.
- Because the keyboard has **no diodes**, **ghosting is common**. Multi-key input was tested but disabled, similar to Blackberry’s original behavior.
- A folder with name **linux_driver** contains the Linux driver to communicate with this STM32 driver. 
- Key files are as follows:
  - Core
    - Inc
      - i2c_slave.h
      - keyboard.h
      - main.h
    - Src
      - i2c_slave.c
      - keyboard.c
      - main.c
  - linux_driver
    - bbq10_driver.c
---

## Hardware Connections

### BBQ10 Keyboard → STM32F411CEU6 (Black Pill)

| Keyboard Pin | STM32 Pin | Function |
|---------------|------------|-----------|
| Col1 | A0 | Column 1 |
| Col2 | A1 | Column 2 |
| Col3 | A2 | Column 3 |
| Col4 | A3 | Column 4 |
| Col5 | A4 | Column 5 |
| Row1 | B0 | Row 1 |
| Row2 | B1 | Row 2 |
| Row3 | A12 | Row 3 |
| Row4 | B3 | Row 4 |
| Row5 | C15 | Row 5 |
| Row6 | B5 | Row 6 |
| Row7 | B15 | Row 7 |
| VDD  | 3V3 | Power |

### Other STM32 Connections

| Pin | Function |
|------|-----------|
| B6 | I²C1_SCL |
| B7 | I²C1_SDA |
| B13 | IRQ_KEYCHANGED (output to master) |

---

## Keyboard Matrix

### Normal Layout

| Row / Col | 1 | 2 | 3 | 4 | 5 |
|------------|---|---|---|---|---|
| **Row 1** | Q | E | R | U | O |
| **Row 2** | W | S | G | H | L |
| **Row 3** | sym | D | T | Y | I |
| **Row 4** | A | P | R⇧ | ↵ | ⌫ |
| **Row 5** | alt | X | V | B | $ |
| **Row 6** | space | Z | C | N | M |
| **Row 7** | 🎤 | L⇧ | F | J | K |

### Alternate Layout (when **Alt** is active)

| Row / Col | 1 | 2 | 3 | 4 | 5 |
|------------|---|---|---|---|---|
| **Row 1** | # | 2 | 3 | _ | + |
| **Row 2** | 1 | 4 | / | : | " |
| **Row 3** |   | 5 | ( | ) | - |
| **Row 4** | * | @ |   |   |   |
| **Row 5** |   | 8 | ? | ! | 🔊 |
| **Row 6** |   | 7 | 9 | , | . |
| **Row 7** | 0 |   | 6 | ; | ' |

---

## Testing with Linux

For testing, I integrated everything to Beagley-AI board that has TI J722S (Jacinto 7) SoC.

### Pin Connections

| BeagleyAI GPIO | Pin | Connection |
|----------------|------|-------------|
| GPIO 4 | Pin 7 | IRQ_KEYCHANGED |
| GPIO 2 | Pin 3 | I²C1_SDA |
| GPIO 3 | Pin 5 | I²C1_SCL |

### Device tree Modifications

```
  // Pin mux
  &main_pmx0
  {
    bbq10_irq_pins_default: bbq10-irq-pins-default {
      pinctrl-single,pins = <
        J722S_IOPAD(0x00b8, PIN_INPUT, 7) // (W26) GPIO0_38
      >;
    };
  };

  // I2C1 in expansion header
  &mcu_i2c0 {
    bbq10_driver: bbq10_driver {
      compatible = "mozcelikors,bbq10_driver";
      reg = <0x52>;
      irq-gpios = <&main_gpio0 38 GPIO_ACTIVE_HIGH>; //GPIO4, GPIO0_38, W26
      pinctrl-names = "default";
      pinctrl-0 = <&bbq10_irq_pins_default>;
      status = "okay";
    };
  };
  
```