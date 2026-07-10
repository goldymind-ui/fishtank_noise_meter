# Fishtank Noise Meter

A portable, battery-powered OLED device that reacts to sound with a pixel-art fish tank animation. Built with an ESP32 and a digital microphone — the fish swim peacefully when it's quiet, panic and dash when they hear a noise, then huddle in the corner until calm returns.

Built as a classroom tool by a teacher learning electronics from scratch.

---

## Demo

The fish have three animation states:

| State | Frames | Behavior |
|---|---|---|
| Idle | 1–10 | Fish swim slowly, bubbles rise, seaweed sways |
| Scared | 11–20 | Fish scatter and dash frantically across the screen |
| Hiding | 21–25 | Fish huddle in the corner until the coast is clear |

Sound is detected continuously via an I2S microphone. When amplitude crosses a configurable threshold, the fish immediately enter the scared state, dash for a set duration, then hide before slowly returning to idle.

---

## Hardware

| Component | Details |
|---|---|
| ESP32 DevKit | Microcontroller |
| SH1106 OLED | 1.3", 128×64px, I2C, address 0x3C |
| INMP441 microphone | Digital I2S mic |
| MT3608 boost converter | Steps 3.7V battery up to stable 5V |
| TP4056 charging module | Li-ion charger via USB |
| 720mAh Li-ion battery | Powers the device overnight |
| Rocker switch | On/off |
| Cardboard enclosure | Handmade, with bolted OLED window |

---

## Wiring

**Power chain**
```
Battery+ ──> TP4056 BAT+
Battery+ ──> Switch ──> MT3608 IN+
Battery- ──> TP4056 BAT- ──> MT3608 IN-
MT3608 OUT+ ──> ESP32 VIN
MT3608 OUT- ──> ESP32 GND
```

**OLED (I2C)**
```
VCC ──> 3.3V
GND ──> GND
SDA ──> GPIO21
SCL ──> GPIO22
```

**Microphone (I2S)**
```
VDD ──> 3.3V
GND ──> GND
SCK ──> GPIO14
WS  ──> GPIO15
SD  ──> GPIO32
L/R ──> GND
```

---

## Software

Built with **PlatformIO** and the **Arduino framework**.

**Libraries:**
- Adafruit SH110X
- Adafruit GFX
- ESP32 I2S driver (built-in)

**Key parameters to tune in `main.cpp`:**

```cpp
#define SOUND_THRESHOLD  2500  // raise if fish react to background noise
#define IDLE_FRAME_MS    180   // idle animation speed (ms per frame)
#define DASH_FRAME_MS     45   // scared dash speed
#define HIDE_FRAME_MS    220   // hiding animation speed
#define DASH_DURATION_MS 2000  // how long fish dash before hiding
#define HIDE_DURATION_MS 3000  // how long fish hide before returning to idle
```

---

## Animation

25 frames drawn by hand in **Piskel** (piskelapp.com), exported via **image2cpp**, stored as PROGMEM byte arrays. Each frame is 128×64 pixels, 1-bit monochrome, 1024 bytes.

Total PROGMEM usage: ~25KB out of ~4MB available flash — the ESP32 handles this trivially.

---

## Files

```
src/
  main.cpp          — sketch: state machine, I2S mic reading, animation loop
  fish_bitmaps.h    — 25 PROGMEM bitmap arrays + frame pointer table
platformio.ini      — PlatformIO project config
```

---

## Notes

- The TP4056 charging port and ESP32 USB port are both exposed through the cardboard enclosure, so the battery can be charged and new code uploaded without opening the box.
- The MT3608 boost converter must be set to **5V output** before connecting the ESP32. Factory default on some modules is 12V — always verify with a multimeter first.
- `SOUND_THRESHOLD` will need tuning per environment. The Serial Monitor prints amplitude on every trigger to help calibrate.

---

## What's next

Adding a push button and a speech recognition pipeline (ESP32 → WiFi → Vosk → OLED) so students can speak into the device and see their words appear on screen.
