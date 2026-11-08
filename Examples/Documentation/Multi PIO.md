# Multi_PIO: Multiple Rotary Encoders on the RP2040 / RP2350

An example Arduino sketch showing how to read several quadrature rotary encoders using the RP2040/RP2350 PIO (Programmable I/O) hardware, via the `EncoderPIO` library. Position tracking is done in hardware by the PIO state machines, so the main loop only has to poll the results.

The sketch reads six encoders, each with its own position limits and wrap behaviour, and prints a line to the Serial Monitor whenever one changes.

## Requirements

**Hardware**
- A board based on the RP2040 or RP2350 (e.g. Raspberry Pi Pico, Pico 2)
- One or more quadrature rotary encoders

**Libraries**
- `EncoderPIO` (the encoder library used by this sketch)
- [`Streaming`](http://arduiniana.org/libraries/streaming/) v5, for C++ stream-style output (`Serial << ...`)

## Capacity

Each PIO block has four state machines, and each encoder uses one.

| Chip | PIO blocks | Max encoders |
|------|-----------|--------------|
| RP2040 | 2 | 8 |
| RP2350 | 3 | 12 |

The example uses six encoders, which means two PIO blocks: the first holds encoders 0–3 and the second holds encoders 4–5.

## Wiring

Each encoder needs two consecutive GPIO pins: the **A** signal on pin `N` and the **B** signal on pin `N+1`.

The sketch lists the A pin of each encoder:

```cpp
const byte ENCODER_PINS[] = { 9, 11, 13, 15, 17, 19 };
```

| Encoder | A pin | B pin |
|---------|-------|-------|
| 0 | GPIO 9  | GPIO 10 |
| 1 | GPIO 11 | GPIO 12 |
| 2 | GPIO 13 | GPIO 14 |
| 3 | GPIO 15 | GPIO 16 |
| 4 | GPIO 17 | GPIO 18 |
| 5 | GPIO 19 | GPIO 20 |

Rules:
- B must be on the GPIO immediately after A.
- Pin pairs must not overlap.
- The pairs do not need to be contiguous or in increasing order.
- The position in the list is the encoder number shown in the serial output.

Connect each encoder's common pin to ground (assuming the library's usual pull-up arrangement; check the `EncoderPIO` documentation for your encoder type).

## How it works

### 1. Configuration arrays

Everything is derived from `ENCODER_PINS`. `NUM_ENCODERS` is computed automatically with `sizeof`, so adding or removing an entry is all that is needed to change the encoder count (within the capacity above).

Per-encoder settings are held in parallel arrays, indexed by encoder number:

| Array | Purpose |
|-------|---------|
| `minPos[]` / `maxPos[]` | Lower and upper position limits |
| `wrap[]` | `true` = wrap around at the limits; `false` = clamp at the limits |
| `oldPos[]` / `newPos[]` | Previous and current position, used to detect change |

A single `edgesPerDetent` value (set to 2) says how many quadrature edges make up one physical click of the encoder. Set this to match your encoder (commonly 1, 2 or 4).

Keep these arrays the same length as `ENCODER_PINS`.

### 2. PIO allocation

Encoders fill one PIO block completely (four at a time) before the next block is used:

```cpp
const byte ENC_PER_PIO = EncoderPIO::MAX_ENCODERS;                      // 4
const byte PIOS_USED = (NUM_ENCODERS + ENC_PER_PIO - 1) / ENC_PER_PIO;  // round up
EncoderPIO* blocks[PIOS_USED];
```

Global encoder `i` lives on PIO block `i / 4`, as encoder `i % 4` within that block's `EncoderPIO` object. Two small helpers hide this arithmetic so the rest of the code can just use a single index:

```cpp
EncoderPIO& blockFor(byte i);  // the EncoderPIO object that owns encoder i
byte        indexIn(byte i);   // encoder i's number within that object
```

### 3. Creating the blocks

`makeBlock(pioNum, first, count)` creates one `EncoderPIO` object for a PIO block. Because the library constructor takes the pins as individual arguments, a `switch` handles blocks containing 1 to 4 encoders. The last block may be only partly filled (here, 2 encoders).

### 4. `setup()`

1. Starts Serial at 115200 baud and waits up to 2 seconds for a connection.
2. Prints the code version and copyright notice.
3. For each PIO block needed, creates the `EncoderPIO` object and calls `initialiseEncoder()`. If initialisation fails, an error is printed and the sketch halts.
4. Sets `encReady = true`.
5. Configures every encoder:
   - `setLimits(idx, min, max)`: returns `false` if `min >= max`
   - `setPosition(idx, 0)`: initial position
   - `setWrap(idx, wrap)`: wrap or clamp at limits
   - `setCountsPerDetent(idx, edgesPerDetent)`: edges per click

### 5. `loop()`

For each encoder, the sketch reads the current position with `getPosition()` and compares it to the last known value. If it changed, it prints a message and updates the stored value. The PIO hardware does the counting, so the loop only needs to read and report.

## Example configuration

| Encoder | Min | Max | Wrap | Typical use |
|---------|-----|-----|------|-------------|
| 0 | -10 | 10 | No | Small bipolar adjustment |
| 1 | 0 | 100 | Yes | Wrapping percentage |
| 2 | 0 | 255 | No | 8-bit value (e.g. brightness) |
| 3 | 0 | 360 | Yes | Angle in degrees |
| 4 | -180 | 180 | Yes | Signed angle |
| 5 | 0 | 100 | No | Clamped percentage |

## Expected output

Open the Serial Monitor at **115200 baud**. On start-up you should see something like:

```
> PIO example for multiple encoders on multiple PIOs

> code version: 0.0.0
> compiled on ... at ..., compiler ver = ...
> © Martin Da Costa (MERG M6223) 2026

> PIO0: 4 encoder(s) initialised (encoders 0 to 3)
> PIO1: 2 encoder(s) initialised (encoders 4 to 5)

> All encoders at initialisation position.
```

Turning an encoder then prints:

```
> Encoder 2 position = 1
> Encoder 2 position = 2
> Encoder 0 position = -1
```

## Adapting the example

1. Edit `ENCODER_PINS` with the A pin of each encoder (B = A+1).
2. Resize `minPos`, `maxPos` and `wrap` to match, and set the values you need.
3. Set `edgesPerDetent` to suit your encoders.
4. Make sure the count does not exceed the capacity of your chip.

## Troubleshooting

- **"failed to initialise" and the sketch halts:** check that the pin pairs do not overlap, B pins are free, and you have not exceeded the PIO capacity.
- **"invalid limits" message:** a `minPos` value is not lower than its `maxPos`.
- **Position changes by two (or half) per click:** adjust `edgesPerDetent`.
- **Direction is reversed:** swap the A and B wires for that encoder.
- **No serial output:** open the Serial Monitor at 115200 baud; the sketch waits only 2 seconds for a connection.

## Version

Code version 1.0.0 (set by `VER_MAJ`, `VER_MIN`, `VER_PATCH`).

## License

Copyright (C) Martin Da Costa 2026.

Licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](http://creativecommons.org/licenses/by-nc-sa/4.0/).

- **Share and adapt** the material in any medium or format.
- **Attribution**: give appropriate credit, link to the license, and indicate changes.
- **NonCommercial**: no commercial use. For commercial licensing, contact the copyright holder.
- **ShareAlike**: derivatives must use the same license.

This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.