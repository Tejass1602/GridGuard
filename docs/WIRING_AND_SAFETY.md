# Low-voltage wiring and safety

GridGuard's waveform is a mathematical simulation. The physical output stage handles only an isolated, current-limited 3.3–12 V DC supply. **Never connect household 230 V AC or any mains conductor to the board, dividers, relay, or breadboard.**

## Pins

| Pico 2 W | GridGuard role |
| --- | --- |
| GP14 | Relay/MOSFET driver, boot-safe LOW; use 220 Ω series and 10 kΩ gate-to-ground |
| GP15 | External status LED and 330 Ω resistor to GND |
| GP16 | Optional auxiliary contact, active-low to GND with internal pull-up |
| GP26 / ADC0 | Source divider, never above 3.3 V |
| GP27 / ADC1 | Load divider, never above 3.3 V |

Do not use the Pico 2 W onboard LED alias. RP2350 input pull-downs are not enabled; use the GP16 pull-up or external pull-downs no larger than 8.2 kΩ.

For 5–12 V DC, the recommended divider is 33 kΩ top and 10 kΩ bottom (factor 4.30), designed for 13.2 V maximum. Add 10 nF from each ADC pin to ground. A 1 kΩ load bleeder helps the load-side voltage fall after opening. For an N-MOSFET low-side LED load, select low-side feedback mode so firmware evaluates `Vsrc - Vdrain`.

The device reports a physical disconnect only after source presence and five consecutive low-load samples satisfy the configured feedback checks. Keep the load current limited and fused. Verify polarity and divider voltage with a meter before connecting GP26/GP27.
