# Single_Encoder: One Rotary Encoder on the RP2040 / RP2350

An example Arduino sketch showing how to read a single quadrature rotary encoders using the RP2040/RP2350 PIO (Programmable I/O) hardware, via the `EncoderPIO` library. Position tracking is done in hardware by the PIO state machines, so the main loop only has to poll the results.

The sketch limits the position to a range, and prints the position to the Serial Monitor whenever it changes.

If you need more than one encoder, see the `Multi_Encoders` or `Multi_PIO` example.

## Requirements

### Hardware

- A board based on the RP2040 or RP2350 (e.g. Raspberry Pi Pico, Pico 2)
- One rotary encoder (quadrature, A/B outputs)

### Libraries

| Library | Purpose |
|---|---|
| [Streaming](http://arduiniana.org/libraries/streaming/) (v5) | C++ stream-style `Serial << ...` output |
| `EncoderPIO` | PIO-based quadrature encoder decoding |

## Wiring

The encoder uses a **pair of adjacent GPIO pins**: the A pin, and the B pin on the next GPIO number.

| Encoder pin | GPIO |
|---|---|
| A | GPIO 15 |
| B | GPIO 16 |

Only the A pin is passed to the library. The B pin is implied as A + 1, so it must always be on the consecutive GPIO. The `PIN_ENCB` constant in the sketch is there for documentation only and isn't used in the code.

Connect the encoder's common pin to ground. Pull-up configuration depends on the library and your encoder module, so check the `EncoderPIO` documentation if you see erratic readings.

## How it works

### 1. Creating the encoder

```cpp
byte encoderPIO = 0;
EncoderPIO encoder(encoderPIO, PIN_ENCA);
```

The first argument selects which PIO block to use (`0` in this instance). The second is the GPIO number of the encoder's A pin.

### 2. Setup

`setup()` does the following:

1. Starts the serial port at 115200 baud, waiting up to 2 seconds for a connection so the board doesn't hang if no monitor is attached.
2. Prints the banner via `printConfig()` (code version, compile date/time, copyright).
3. Calls `encoder.initialiseEncoder()`. If this fails, an error is printed and the sketch halts in an infinite loop.
4. Sets `encReady = true`, then configures the encoder:

| Call | Effect |
|---|---|
| `setLimits(min, max)` | Constrains the position to the range min to max. Returns `false` if `min >= max`. |
| `setPosition(value)` | Sets the starting position. If this is outside the `min / max` boundaries, the library will move it inside.|
| `setWrap(false)` | Disables wrap-around, so the position stops at the limits instead of jumping to the opposite end. Set `true` to wrap. |
| `setCountsPerDetent(counts)` | Sets how many quadrature counts make one position step. |

Here the limits are **-10 to +10**, the starting position is **0**, wrapping is off, and there are **2 counts per detent**. Adjust `counts` to match your encoder: common values are 1, 2 or 4, depending on how many quadrature transitions occur per physical click.

The return value of `setLimits()` is stored in `limitsValid` but this sketch doesn't check it. If you change `minPos` or `maxPos`, you may want to add a check so that invalid limits are reported.

Finally, the starting position is printed.

### 3. The main loop

```cpp
newPos = encoder.getPosition();
if (newPos != oldPos) {
  Serial << F("> Encoder position = ") << newPos << endl;
  oldPos = newPos;
}
```

Each pass, the loop reads the position with `getPosition()` and compares it with the previously stored value. If it has changed, the new position is printed, for example:

```
> Encoder position = 4
```

Because the PIO hardware tracks the encoder independently of the CPU, no rotation is missed even if the loop is busy, and the loop only needs to read the current position value.

## Customising

- **Change pins:** edit `PIN_ENCA`. Remember the B pin must be A + 1.
- **Change the range:** edit `minPos` and `maxPos`.
- **Enable wrap-around:** pass `true` to `setWrap()`, which suits dials such as a heading or a menu that should loop.
- **Adjust feel:** change `counts` to match your encoder's detents.
- **Use the position:** replace the `Serial` print in `loop()` with your own action, such as changing a setting or moving a menu selection.

## Using it

1. Install the `Streaming` and `EncoderPIO` libraries.
2. Select your RP2040 / RP2350 board in the Arduino IDE.
3. Wire up the encoder as in the table above.
4. Upload the sketch and open the Serial Monitor at **115200 baud**.
5. Turn the encoder and watch the position update.

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
