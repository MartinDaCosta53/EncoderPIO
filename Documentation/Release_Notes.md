# EncoderPIO Release Notes

## Summary

This release adds a configurable step size, so each detent can move the position by more than 1. It also adds control over how the position wraps at the limits. Existing sketches that never call the new functions behave exactly as before.

## New API

### `setStep(step)` / `setStep(encoder, step)`

- Sets how much the position changes per detent, from 1 to 65535 (`uint16_t`). The default is 1, and 0 is treated as 1.
- It is set per encoder. The version without an index acts on encoder 0, and an invalid index is ignored.
- It combines with `setCountsPerDetent()`, which still sets how many quadrature edges make one detent. For example, 4 counts per detent with a step of 5 moves the position by 5 per full detent.
- It can be changed while the encoders are running. The write is done with interrupts disabled, and it does not reset a partly accumulated detent.

### `setWrapEndsDistinct(distinct)` / `setWrapEndsDistinct(encoder, distinct)`

- Chooses how wrap treats the two limits. It has no effect unless wrap is enabled.
- `true` (default): both end values are separate positions, so the circle has `maxPos - minPos + 1` positions and a single step from one limit lands on the other. This is the behaviour of earlier releases.
- `false`: `minPos` and `maxPos` are the same point on a circle of `maxPos - minPos` positions, like compass degrees.
- It is set per encoder, with the same indexing and invalid-index handling as `setStep()`.

## Limit and wrap behaviour with a step above 1

- **Without wrap:** a move that would pass a limit stops on it, so `minPos` and `maxPos` are always reachable. With a step of 10 on a 0–100 range, 95 goes to 100.
- **With wrap:** the excess carries over past the opposite limit instead of stopping or snapping to it. A step larger than the whole range keeps going round correctly.

Example with a range of -180 to +180 and a step of 5:

| Move | Distinct ends (default) | Shared ends |
|---|---|---|
| -178 down | +178 | +177 |
| -180 down by 1 | +180 | +179 |
| +180 up by 1 | -180 | -179 |

## Compatibility

Existing code behaves as before. With wrap enabled and a step of 1, decrementing from `minPos` still goes to `maxPos`, and incrementing from `maxPos` still goes to `minPos`. Without wrap, limits still clamp. To get compass-style wrapping, where `minPos` and `maxPos` are the same point, call `setWrapEndsDistinct(false)`.

The one exception is the limits `INT_MIN` to `INT_MAX`, which always use shared ends (see below).

## Implementation notes

- Position arithmetic is in a new helper, `movePosition()`, used by both `incPos()` and `decPos()`. It uses 32-bit arithmetic only, with unsigned distances to the limits, so ranges wider than `INT_MAX` cannot overflow.
- The helper is kept in RAM (`__not_in_flash`) like the interrupt handler.
- The normal path, including a single wrap, contains no division. A modulo is only used when a step is at least one whole range, which needs a very small range with a large step.
- The limits `INT_MIN` to `INT_MAX` always use shared ends, because the distinct-ends lap length would overflow.
- `setPosition()`, `setLimits()` and the other setters are unchanged and still clamp positions into the limits, which the new arithmetic relies on.

## Files changed

| File | Change |
|---|---|
| `EncoderPIO.h` | New `step` and `wrapEndsDistinct` fields in `Channel` (the latter defaulting to `true`), plus the `setStep()` and `setWrapEndsDistinct()` declarations. |
| `EncoderPIO.cpp` | New `movePosition()`, updated `incPos()` and `decPos()`, and the two new setter functions. |
| `EncoderPIO.pio`, `EncoderPIO.pio.h` | Unchanged. |
