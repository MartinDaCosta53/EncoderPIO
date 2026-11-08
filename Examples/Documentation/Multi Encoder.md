# Multi_Encoders: Multiple Rotary Encoders on the RP2350

An example Arduino sketch showing how to read up to four quadrature rotary encoders using the RP2040/RP2350 PIO (Programmable I/O) hardware, via the `EncoderPIO` library. Position tracking is done in hardware by the PIO state machines, so the main loop only has to poll the results.

The sketch reads three encoders, applies position limits, and prints the positions to the Serial Monitor whenever any of them change.

## Requirements

### Hardware

- A board based on the RP2040 or RP2350 (e.g. Raspberry Pi Pico, Pico 2)
- Three rotary encoders (quadrature, A/B outputs)
- Wiring for the encoder pins listed below

### Libraries

| Library | Purpose |
|---|---|
| [Streaming](http://arduiniana.org/libraries/streaming/) (v5) | C++ stream-style `Serial << ...` output |
| `EncoderPIO` | PIO-based quadrature encoder decoding |

## Wiring

Each encoder uses a **pair of adjacent GPIO pins**: the A pin, and the B pin on the next GPIO number. A GPIO cannot be shared between encoders.

| Encoder | A pin | B pin |
|---|---|---|
| Encoder 1 | GPIO 15 | GPIO 16 |
| Encoder 2 | GPIO 17 | GPIO 18 |
| Encoder 3 | GPIO 13 | GPIO 14 |

Only the A pin is passed to the library. The B pin is implied as A + 1, so the B pin must always be on the consecutive GPIO.

Connect each encoder's common pin to ground. Pull-up configuration depends on the library and your encoder module, so check the `EncoderPIO` documentation if you see erratic readings.

## How it works

### 1. Creating the encoder group

```cpp
byte encoderPIO = 0;
EncoderPIO encGrp(encoderPIO, PIN_ENC1A, PIN_ENC2A, PIN_ENC3A);
```

An `EncoderPIO` object manages a **group** of up to four encoders. The first argument selects which PIO block to use (`0` in this instance), followed by the A-pin of each encoder.

- All encoders in a single object share the same PIO block.
- To use more than four encoders, create an additional `EncoderPIO` object, which will use a different PIO block ( see `Multi_PIO` example).
- Encoders in a group are addressed by index (0, 1, 2) in the order the pins were given.

### 2. Setup

`setup()` does the following:

1. Starts the serial port at 115200 baud, waiting up to 2 seconds for a connection so the board doesn't hang if no monitor is attached.
2. Prints the banner via `printConfig()` (code version, compile date/time, copyright).
3. Calls `encGrp.initialiseEncoder()`. If this fails, an error is printed and the sketch halts in an infinite loop.
4. Sets `encReady = true`, then configures each encoder:

| Call | Effect |
|---|---|
| `setLimits(i, min, max)` | Constrains the position to the range min to max. Returns `false` if `min >= max`. |
| `setPosition(i, value)` | Sets the starting position. |
| `setWrap(i, false)` | Disables wrap-around, so the position stops at the limits instead of jumping to the opposite end. Set `true` to wrap. |
| `setCountsPerDetent(i, counts)` | Sets how many quadrature counts make one position step. |

In this example every encoder has limits of **-10 to +10**, a starting position of **0**, wrapping off, and **2 counts per detent**. Adjust `counts` to match your encoders: common values are 1, 2 or 4, depending on how many quadrature transitions occur per physical click.

### 3. The main loop

```cpp
for (byte i = 0; i < numEnc; i++)
{
  newPos[i] = encGrp.getPosition(i);
  if (newPos[i] != oldPos[i]) { change = true; oldPos[i] = newPos[i]; }
}
```

Each pass, the loop reads every encoder's position with `getPosition(i)` and compares it with the previously stored value. If any has changed, a single line is printed showing all three positions, for example:

```
> Encoder 1 = 3, Encoder 2 = 0, Encoder3 = -5
```

Because the PIO hardware tracks the encoders independently of the CPU, no rotation is missed even if the loop is busy, and the loop only needs to read the current position values.

## Customising

- **Change pins:** edit the `PIN_ENCxA` constants. Remember each B pin must be A + 1.
- **Change the number of encoders:** adjust `numEnc`, the arrays (`newPos`, `oldPos`, `minPos`, `maxPos`), and the constructor arguments (up to four per group).
- **Change ranges:** edit `minPos` and `maxPos` per encoder.
- **Enable wrap-around:** pass `true` to `setWrap()`, which suits dials such as a heading or a menu that should loop.
- **Adjust feel:** change `counts` to match your encoder's detents.
- **Use the position:** replace the `Serial` print in `loop()` with your own action, such as changing a setting or moving a menu selection.

## Using it

1. Install the `Streaming` and `EncoderPIO` libraries.
2. Select your RP2040 / RP2350 board in the Arduino IDE.
3. Wire up the encoders as in the table above.
4. Upload the sketch and open the Serial Monitor at **115200 baud**.
5. Turn the encoders and watch the positions update.

## Version

Code version 1.0.0 (set by `VER_MAJ`, `VER_MIN`, `VER_PATCH`).

## Licence

Copyright (C) Martin Da Costa 2026.

Licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](http://creativecommons.org/licenses/by-nc-sa/4.0/).

- **Share and adapt** the material freely.
- **Attribution:** give appropriate credit, link to the licence, and indicate changes.
- **NonCommercial:** no commercial use. For commercial licensing, contact the copyright holder.
- **ShareAlike:** derivative works must use the same licence.

This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
