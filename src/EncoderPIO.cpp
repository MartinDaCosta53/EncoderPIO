// Copyright (C) Martin Da Costa 2026 (martindc.merg@gmail.com)
// Licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
// The full licence can be found at: http://creativecommons.org/licenses/by-nc-sa/4.0/

#include "hardware/irq.h"
#include "hardware/sync.h"
#include "EncoderPIO.h"
#include "EncoderPIO.pio.h"

//---------------------------------------------
// Static member definition (one encoder per PIO block)

EncoderPIO* EncoderPIO::_instances[3] = { nullptr, nullptr, nullptr };

//---------------------------------------------
// Helper: system IRQ number for a PIO block and IRQ line (0 or 1)

static uint pioSystemIrq(uint8_t pioNum, uint irqLine)
{
  switch (pioNum)
  {
  case 0:
    return irqLine ? PIO0_IRQ_1 : PIO0_IRQ_0;
  case 1:
    return irqLine ? PIO1_IRQ_1 : PIO1_IRQ_0;
#if PICO_RP2350
  case 2:
    return irqLine ? PIO2_IRQ_1 : PIO2_IRQ_0;
#endif
  default:
    return irqLine ? PIO0_IRQ_1 : PIO0_IRQ_0;  // not reached: pioNum validated earlier
  }
}

//---------------------------------------------
// Constructor

EncoderPIO::EncoderPIO(uint8_t pioNum, uint8_t enc1Pin)
{
  _pioNum = pioNum;
  _encPin_1A = enc1Pin;
  _encPin_1B = _encPin_1A + 1;
}

//---------------------------------------------
// Returns true on success, false if the encoder could not be started.
// Safe to call more than once: later calls return true without doing anything.

bool EncoderPIO::initialiseEncoder()
{
  if (_initialised)
  {
    return true;
  }

  switch (_pioNum)
  {
  case 0:
    pio = pio0;
    break;
  case 1:
    pio = pio1;
    break;
#if PICO_RP2350
  case 2:
    pio = pio2;
    break;
#endif
  default:
    return false;  // invalid PIO number
  }

  // Only one encoder per PIO block: the IRQ flags and _instances[] slot are per-PIO
  if (_instances[_pioNum] != nullptr)
  {
    return false;
  }

  // The program uses "mov exec" with raw jmp opcodes, so it MUST load at offset 0
  if (!pio_can_add_program_at_offset(pio, &EncoderPIO_program, 0))
  {
    return false;  // offset 0 already used by another program on this PIO
  }

  // Claim a free state machine rather than assuming SM0 is available
  int sm = pio_claim_unused_sm(pio, false);
  if (sm < 0)
  {
    return false;  // no free state machine on this PIO
  }
  _sm = (uint8_t)sm;

  _offset = pio_add_program_at_offset(pio, &EncoderPIO_program, 0);

  // Pins
  pio_gpio_init(pio, _encPin_1A);
  gpio_set_pulls(_encPin_1A, true, false);
  pio_gpio_init(pio, _encPin_1B);
  gpio_set_pulls(_encPin_1B, true, false);

  // State machine setup
  pio_sm_config c = EncoderPIO_program_get_default_config(_offset);
  sm_config_set_in_pin_base(&c, _encPin_1A);
  sm_config_set_in_shift(&c, false, false, 0);
  pio_sm_init(pio, _sm, _offset + 16, &c);  // program starts after the 16-entry jump table

  // Clear any stale flags, register the instance, then enable the IRQs
  pio_interrupt_clear(pio, 0);
  pio_interrupt_clear(pio, 1);
  _instances[_pioNum] = this;

  pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
  pio_set_irq1_source_enabled(pio, pis_interrupt1, true);

  uint irq0 = pioSystemIrq(_pioNum, 0);
  uint irq1 = pioSystemIrq(_pioNum, 1);

  irq_add_shared_handler(irq0, dec_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
  irq_set_enabled(irq0, true);
  irq_add_shared_handler(irq1, inc_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
  irq_set_enabled(irq1, true);

  pio_sm_set_enabled(pio, _sm, true);

  _initialised = true;
  return true;
}

//---------------------------------------------
// Interrupt handlers (kept in RAM so flash activity can't delay them)

__not_in_flash("EncoderPIO") void EncoderPIO::dec_irq_handler()  // pio_int_0 handler
{
  for (EncoderPIO* inst : _instances)
  {
    if (inst && pio_interrupt_get(inst->pio, 0))
    {
      pio_interrupt_clear(inst->pio, 0);
      inst->decPos();
    }
  }
}

__not_in_flash("EncoderPIO") void EncoderPIO::inc_irq_handler()  // pio_int_1 handler
{
  for (EncoderPIO* inst : _instances)
  {
    if (inst && pio_interrupt_get(inst->pio, 1))
    {
      pio_interrupt_clear(inst->pio, 1);
      inst->incPos();
    }
  }
}

//---------------------------------------------
// Each quadrature edge calls decPos()/incPos(). _subCount accumulates edges
// until a full detent (_countsPerDetent edges) has been made, then the
// position moves by one. Reversing direction part-way through a detent
// simply cancels the accumulated edges, so there is no jitter.

__not_in_flash("EncoderPIO") void EncoderPIO::decPos()
{
  _subCount = _subCount - 1;
  if (_subCount > -(int8_t)_countsPerDetent)
  {
    return;  // detent not complete yet
  }
  _subCount = 0;

  int pos = _position;
  if (pos > _minPos)
  {
    _position = pos - 1;
  }
  else if (_wrap)
  {
    _position = _maxPos;
  }
}

__not_in_flash("EncoderPIO") void EncoderPIO::incPos()
{
  _subCount = _subCount + 1;
  if (_subCount < (int8_t)_countsPerDetent)
  {
    return;  // detent not complete yet
  }
  _subCount = 0;

  int pos = _position;
  if (pos < _maxPos)
  {
    _position = pos + 1;
  }
  else if (_wrap)
  {
    _position = _minPos;
  }
}

//---------------------------------------------
// Public API

int EncoderPIO::getPosition()
{
  return _position;
}

void EncoderPIO::setPosition(int newPosition)
{
  if (newPosition < _minPos) newPosition = _minPos;
  if (newPosition > _maxPos) newPosition = _maxPos;

  uint32_t irqState = save_and_disable_interrupts();
  _position = newPosition;
  _subCount = 0;
  restore_interrupts(irqState);
}

// Returns false (and changes nothing) if maxPos <= minPos.
// The current position is clamped into the new range.
bool EncoderPIO::setLimits(int minPos, int maxPos)
{
  if (maxPos <= minPos)
  {
    return false;
  }

  uint32_t irqState = save_and_disable_interrupts();
  _minPos = minPos;
  _maxPos = maxPos;
  int pos = _position;
  if (pos < _minPos) pos = _minPos;
  if (pos > _maxPos) pos = _maxPos;
  _position = pos;
  _subCount = 0;
  restore_interrupts(irqState);
  return true;
}

void EncoderPIO::setWrap(bool wrap)
{
  _wrap = wrap;
}

// Number of quadrature edges per position step.
//   1 = every edge counts (4 per full cycle)
//   2 = for encoders with 2 edges per detent
//   4 = for the common type with 4 edges (one full cycle) per detent
// Values below 1 are treated as 1; values above 100 are limited to 100.
void EncoderPIO::setCountsPerDetent(uint8_t counts)
{
  if (counts < 1) counts = 1;
  if (counts > 100) counts = 100;

  uint32_t irqState = save_and_disable_interrupts();
  _countsPerDetent = counts;
  _subCount = 0;
  restore_interrupts(irqState);
}
