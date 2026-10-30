# EncoderPIO

A quadrature rotary encoder library for the Raspberry Pi **RP2040** and **RP2350**, using the PIO (Programmable I/O) hardware to decode encoders with no CPU polling and no missed edges.

One `EncoderPIO` object drives **1 to 4 encoders** on a single PIO block, using one state machine per encoder. Position, limits, wrapping and detent handling are done for you, so reading an encoder is a single call to `getPosition()`.

Any of the PIO may be used allowing up to 8 encoders to be decoded with the **RP2040** and up to 12 on the **RP2350**.

## Features

- Hardware decoding in PIO: the CPU is only interrupted when a valid step happens
- 1 to 4 encoders per object, one state machine each
- All PIo can be used as individual objects for decoding
- Per-encoder position, minimum/maximum limits and optional wrap-around
- Configurable counts per detent (1, 2 or 4 edges per step for common encoders)
- Invalid transitions (contact bounce, glitches) are ignored by the PIO program
- Reversing part-way through a detent cancels the partial movement, so there is no jitter
- Adjustable sampling rate (default 1 MHz) that can be changed while running
- Internal pull-ups on by default
- Clean shutdown with `end()`, which releases all PIO resources so the encoders can be restarted later
- Interrupt handlers run from RAM, so flash activity cannot delay them

## Requirements

- A board based on the RP2040 or RP2350 (for example Raspberry Pi Pico, Pico 2)
- An Arduino core for these chips that provides the Pico SDK headers, such as Arduino-Pico (`hardware/pio.h`, `hardware/irq.h`, and so on)
- Mechanical or optical quadrature encoders with A and B outputs

## Wiring

Each encoder is identified by the GPIO of its **A** signal. **B must be connected to the next GPIO** (A + 1).

| Encoder | A pin (given) | B pin (implied) |
|---------|---------------|-----------------|
| 0       | GPIO 2        | GPIO 3          |
| 1       | GPIO 4        | GPIO 5          |

Rules:

- The `{A, A+1}` pairs of different encoders must not overlap. Using GPIO 2 and GPIO 3 as two separate A pins is invalid.
- To reverse the counting direction, swap A and B by swapping the physical wires .
- Common (common-ground) encoders should have their common pin connected to GND. Leave the internal pull-ups enabled.

## API reference

### Constructors

```cpp
EncoderPIO(uint8_t pioNum, uint8_t enc1PinA);
EncoderPIO(uint8_t pioNum, uint8_t enc1PinA, uint8_t enc2PinA);
EncoderPIO(uint8_t pioNum, uint8_t enc1PinA, uint8_t enc2PinA, uint8_t enc3PinA);
EncoderPIO(uint8_t pioNum, uint8_t enc1PinA, uint8_t enc2PinA, uint8_t enc3PinA, uint8_t enc4PinA);
```

`pioNum` is 0 or 1 (or 2 on RP2350). The number of pin arguments sets the number of encoders. Objects cannot be copied.

### Starting and stopping

| Method | Description |
|--------|-------------|
| `bool initialiseEncoder()` | Claims the PIO resources and starts the encoders. Returns `true` on success. Safe to call more than once. **Always check the result.** |
| `void end()` | Stops the encoders and releases state machines, program space, the interrupt handler and the pins. Positions and settings are kept. Also called by the destructor. |

`initialiseEncoder()` returns `false` if the PIO number is invalid, another `EncoderPIO` object already uses that PIO block, a pin pair does not exist or overlaps another, offset 0 of the PIO program memory is already in use, or there are not enough free state machines.

Call `initialiseEncoder()` and `end()` from the same core.

### Reading and setting the position

| Method | Description |
|--------|-------------|
| `int getPosition(uint8_t encoder = 0)` | Current position. Returns 0 for an invalid index. |
| `void setPosition(uint8_t encoder, int pos)` | Sets the position, clamped to the limits. Clears any partial detent. |

The `encoder` argument can be left out for encoder 0, so single-encoder code stays simple.

### Configuration

| Method | Description |
|--------|-------------|
| `bool setLimits(uint8_t encoder, int minPos, int maxPos)` | Sets the range (default 0 to 100). Returns `false` if `maxPos <= minPos` or the index is invalid. The current position is clamped into the new range. |
| `void setWrap(uint8_t encoder, bool wrap)` | `true`: going past a limit wraps to the other end. `false` (default): stops at the limit. |
| `void setCountsPerDetent(uint8_t encoder, uint8_t counts)` | Quadrature edges per position step, 1 to 100 (default 1). Use 4 for the common type with one full cycle per detent, 2 for encoders with two edges per detent. |
| `void setPullUps(bool enable)` | Internal pull-ups on A and B of every encoder (default `true`). Can be called before or after `initialiseEncoder()`. |
| `bool setSampleRate(uint32_t hz)` | Pin sampling rate of the PIO program (default 1 MHz). Can be called before or after `initialiseEncoder()`. Returns `false` if `hz` is 0. |

For each per-encoder method, the encoder-less form (for example `setLimits(0, 100)`) applies to encoder 0.

### Sample rate notes

The PIO program takes 5 or 6 PIO cycles per sample depending on the path, so the actual idle sampling rate can be up to 20% higher than requested. The achievable range is limited by the clock divider: the maximum is about `clk_sys / 6` and the minimum a few hundred Hz. The sample rate applies to all encoders on one object. If you change `clk_sys` after setting it, call `setSampleRate()` again to recompute the divider.

## Limitations

- One `EncoderPIO` object per PIO block (2 on RP2040, 3 on RP2350), each with up to 4 encoders
- The PIO program must load at offset 0 of the PIO instruction memory, so it cannot share a PIO block with another program that already occupies offset 0
- B must be on GPIO A + 1
- Position is a signed `int`; sub-detent counting uses an `int8_t`, which is why counts per detent are limited to 100
- If the RX FIFO fills up (the CPU cannot keep up), further steps are dropped

## Further reading

See [EXPLANATION.md](EXPLANATION.md) for a detailed description of how the library works.

## Licence

Copyright (C) Martin Da Costa 2026 (martindc.merg@gmail.com)

Licensed under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](http://creativecommons.org/licenses/by-nc-sa/4.0/).
