// Copyright (C) Martin Da Costa 2026 (martindc.merg@gmail.com)
// Licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
// The full licence can be found at: http://creativecommons.org/licenses/by-nc-sa/4.0/

#include "hardware/irq.h"
#include "hardware/sync.h"
#include "EncoderPIO.h"
#include "EncoderPIO.pio.h"

//---------------------------------------------
// Static member definition (one EncoderPIO object per PIO block)

EncoderPIO* EncoderPIO::_instances[3] = { nullptr, nullptr, nullptr };

//---------------------------------------------
// The PIO program pushes the 4-bit value B'A'BA for every valid step
// (A = bit 0 and B = bit 1 of each 2-bit sample; ' = previous sample).
// These masks have bit n set if value n is a step in that direction.
// A leading B (A rising first) is an increment:
//   increment: 0001, 0111, 1000, 1110
//   decrement: 0010, 0100, 1011, 1101

static const uint32_t DEC_MASK = (1u << 0x2) | (1u << 0x4) | (1u << 0xB) | (1u << 0xD);
static const uint32_t INC_MASK = (1u << 0x1) | (1u << 0x7) | (1u << 0x8) | (1u << 0xE);

// The 16-entry jump table comes first in the program; the code starts after it
static const uint PC_START_OFFSET = 16;

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

// Helper: the interrupt handler for a PIO block (pioNum validated earlier)
EncoderPIO::IrqHandler EncoderPIO::handlerFor(uint8_t pioNum)
{
  return (pioNum == 0) ? pio0_irq_handler :
         (pioNum == 1) ? pio1_irq_handler : pio2_irq_handler;
}

//---------------------------------------------
// Constructors: the number of pins given sets the number of encoders

void EncoderPIO::configure(uint8_t pioNum, uint8_t count)
{
  _pioNum = pioNum;
  _numEncoders = count;
}

EncoderPIO::EncoderPIO(uint8_t pioNum, uint8_t enc1PinA)
{
  configure(pioNum, 1);
  _ch[0].pinA = enc1PinA;
}

EncoderPIO::EncoderPIO(uint8_t pioNum, uint8_t enc1PinA, uint8_t enc2PinA)
{
  configure(pioNum, 2);
  _ch[0].pinA = enc1PinA;
  _ch[1].pinA = enc2PinA;
}

EncoderPIO::EncoderPIO(uint8_t pioNum, uint8_t enc1PinA, uint8_t enc2PinA, uint8_t enc3PinA)
{
  configure(pioNum, 3);
  _ch[0].pinA = enc1PinA;
  _ch[1].pinA = enc2PinA;
  _ch[2].pinA = enc3PinA;
}

EncoderPIO::EncoderPIO(uint8_t pioNum, uint8_t enc1PinA, uint8_t enc2PinA, uint8_t enc3PinA, uint8_t enc4PinA)
{
  configure(pioNum, 4);
  _ch[0].pinA = enc1PinA;
  _ch[1].pinA = enc2PinA;
  _ch[2].pinA = enc3PinA;
  _ch[3].pinA = enc4PinA;
}

EncoderPIO::~EncoderPIO()
{
  end();
}

//---------------------------------------------
// Returns true on success, false if the encoders could not be started.
// Safe to call more than once: later calls return true without doing anything.
// After end(), it can be called again to restart the encoders.

bool EncoderPIO::initialiseEncoder()
{
  if (_initialised)
  {
    return true;
  }

  switch (_pioNum)
  {
  case 0:
    _pio = pio0;
    break;
  case 1:
    _pio = pio1;
    break;
#if PICO_RP2350
  case 2:
    _pio = pio2;
    break;
#endif
  default:
    return false;  // invalid PIO number
  }

  // Only one EncoderPIO object per PIO block: the FIFO IRQ handler and _instances[] slot are per-PIO
  if (_instances[_pioNum] != nullptr)
  {
    return false;
  }

  // B is the GPIO after A, so both must exist
  for (uint8_t i = 0; i < _numEncoders; i++)
  {
    if (_ch[i].pinA + 1 >= NUM_BANK0_GPIOS)
    {
      return false;
    }
  }

  // The {A, A+1} pin pairs must not overlap (this also rejects the same pin used twice)
  for (uint8_t i = 0; i < _numEncoders; i++)
  {
    for (uint8_t j = i + 1; j < _numEncoders; j++)
    {
      int diff = (int)_ch[i].pinA - (int)_ch[j].pinA;
      if (diff > -2 && diff < 2)
      {
        return false;
      }
    }
  }

  // The program jumps into a table at absolute addresses 0 to 15 ("mov pc, isr"),
  // so it MUST load at offset 0
  if (!pio_can_add_program_at_offset(_pio, &EncoderPIO_program, 0))
  {
    return false;  // offset 0 already used by another program on this PIO
  }

  // Claim one free state machine per encoder, or give them all back and fail
  for (uint8_t i = 0; i < _numEncoders; i++)
  {
    int sm = pio_claim_unused_sm(_pio, false);
    if (sm < 0)
    {
      for (uint8_t j = 0; j < i; j++)
      {
        pio_sm_unclaim(_pio, _ch[j].sm);
      }
      return false;  // not enough free state machines on this PIO
    }
    _ch[i].sm = (uint8_t)sm;
  }

  // One copy of the program is shared by all the state machines
  _offset = pio_add_program_at_offset(_pio, &EncoderPIO_program, 0);

  for (uint8_t i = 0; i < _numEncoders; i++)
  {
    Channel& ch = _ch[i];

    // Pins: handed to the PIO, explicitly inputs, pulls set below
    pio_gpio_init(_pio, ch.pinA);
    pio_gpio_init(_pio, ch.pinA + 1);
    pio_sm_set_consecutive_pindirs(_pio, ch.sm, ch.pinA, 2, false);

    // State machine setup
    pio_sm_config c = EncoderPIO_program_get_default_config(_offset);
    sm_config_set_in_pin_base(&c, ch.pinA);
    sm_config_set_in_shift(&c, false, false, 0);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);  // 8-deep RX FIFO, nothing is sent to the TX FIFO
    pio_sm_init(_pio, ch.sm, _offset + PC_START_OFFSET, &c);
  }

  applyPulls();

  // Register the object, then enable the FIFO "not empty" interrupt of each state machine
  _instances[_pioNum] = this;

  for (uint8_t i = 0; i < _numEncoders; i++)
  {
    pio_set_irq0_source_enabled(_pio,
        static_cast<enum pio_interrupt_source>(pis_sm0_rx_fifo_not_empty + _ch[i].sm), true);
  }

  uint irq0 = pioSystemIrq(_pioNum, 0);
  irq_add_shared_handler(irq0, handlerFor(_pioNum), PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
  irq_set_enabled(irq0, true);

  for (uint8_t i = 0; i < _numEncoders; i++)
  {
    pio_sm_set_enabled(_pio, _ch[i].sm, true);
  }

  _initialised = true;
  return true;
}

//---------------------------------------------
// Stops the encoders and gives back everything initialiseEncoder() took:
// state machines, program space, interrupt handler and pins (left as plain
// inputs with the pulls off). Positions and settings are kept. Safe to call
// more than once, and called by the destructor. Call it from the core that
// called initialiseEncoder().

void EncoderPIO::end()
{
  if (!_initialised)
  {
    return;
  }

  // Stop the state machines and their interrupt sources first, so nothing new can arrive
  for (uint8_t i = 0; i < _numEncoders; i++)
  {
    pio_sm_set_enabled(_pio, _ch[i].sm, false);
    pio_set_irq0_source_enabled(_pio,
        static_cast<enum pio_interrupt_source>(pis_sm0_rx_fifo_not_empty + _ch[i].sm), false);
  }

  // Remove our handler; only switch the IRQ off if nobody else is sharing it
  uint irq0 = pioSystemIrq(_pioNum, 0);
  irq_remove_handler(irq0, handlerFor(_pioNum));
  if (!irq_has_shared_handler(irq0))
  {
    irq_set_enabled(irq0, false);
  }
  _instances[_pioNum] = nullptr;

  // Hand back the pins as plain inputs with the pulls off
  for (uint8_t i = 0; i < _numEncoders; i++)
  {
    gpio_init(_ch[i].pinA);
    gpio_set_pulls(_ch[i].pinA, false, false);
    gpio_init(_ch[i].pinA + 1);
    gpio_set_pulls(_ch[i].pinA + 1, false, false);
  }

  // Give back the state machines and the program
  for (uint8_t i = 0; i < _numEncoders; i++)
  {
    pio_sm_clear_fifos(_pio, _ch[i].sm);
    pio_sm_unclaim(_pio, _ch[i].sm);
    _ch[i].subCount = 0;
  }
  pio_remove_program(_pio, &EncoderPIO_program, _offset);

  _initialised = false;
}

//---------------------------------------------
// Pull-ups on A and B of every encoder (pull-downs are never used).
// Takes effect immediately if the encoders are already running, otherwise
// when initialiseEncoder() runs.

void EncoderPIO::setPullUps(bool enable)
{
  _pullUps = enable;
  if (_initialised)
  {
    applyPulls();
  }
}

void EncoderPIO::applyPulls()
{
  for (uint8_t i = 0; i < _numEncoders; i++)
  {
    gpio_set_pulls(_ch[i].pinA, _pullUps, false);
    gpio_set_pulls(_ch[i].pinA + 1, _pullUps, false);
  }
}

//---------------------------------------------
// Interrupt handlers (kept in RAM so flash activity can't delay them)

__not_in_flash("EncoderPIO") void EncoderPIO::pio0_irq_handler()
{
  if (_instances[0])
  {
    _instances[0]->serviceFifos();
  }
}

__not_in_flash("EncoderPIO") void EncoderPIO::pio1_irq_handler()
{
  if (_instances[1])
  {
    _instances[1]->serviceFifos();
  }
}

__not_in_flash("EncoderPIO") void EncoderPIO::pio2_irq_handler()
{
  if (_instances[2])
  {
    _instances[2]->serviceFifos();
  }
}

// Empties the RX FIFO of every encoder's state machine. Each word is the
// B'A'BA value of one step; the interrupt stays asserted until the FIFO is empty.
__not_in_flash("EncoderPIO") void EncoderPIO::serviceFifos()
{
  for (uint8_t i = 0; i < _numEncoders; i++)
  {
    Channel& ch = _ch[i];
    while (!pio_sm_is_rx_fifo_empty(_pio, ch.sm))
    {
      uint32_t v = pio_sm_get(_pio, ch.sm) & 0xF;
      if ((DEC_MASK >> v) & 1u)
      {
        decPos(ch);
      }
      else if ((INC_MASK >> v) & 1u)
      {
        incPos(ch);
      }
    }
  }
}

//---------------------------------------------
// Each quadrature edge calls decPos()/incPos(). subCount accumulates edges
// until a full detent (countsPerDetent edges) has been made, then the
// position moves by one. Reversing direction part-way through a detent
// simply cancels the accumulated edges, so there is no jitter.

__not_in_flash("EncoderPIO") void EncoderPIO::decPos(Channel& ch)
{
  ch.subCount = ch.subCount - 1;
  if (ch.subCount > -(int8_t)ch.countsPerDetent)
  {
    return;  // detent not complete yet
  }
  ch.subCount = 0;

  int pos = ch.position;
  if (pos > ch.minPos)
  {
    ch.position = pos - 1;
  }
  else if (ch.wrap)
  {
    ch.position = ch.maxPos;
  }
}

__not_in_flash("EncoderPIO") void EncoderPIO::incPos(Channel& ch)
{
  ch.subCount = ch.subCount + 1;
  if (ch.subCount < (int8_t)ch.countsPerDetent)
  {
    return;  // detent not complete yet
  }
  ch.subCount = 0;

  int pos = ch.position;
  if (pos < ch.maxPos)
  {
    ch.position = pos + 1;
  }
  else if (ch.wrap)
  {
    ch.position = ch.minPos;
  }
}

//---------------------------------------------
// Public API. "encoder" is the index in constructor order (0 to 3).

int EncoderPIO::getPosition(uint8_t encoder)
{
  if (encoder >= _numEncoders)
  {
    return 0;
  }
  return _ch[encoder].position;
}

void EncoderPIO::setPosition(uint8_t encoder, int newPosition)
{
  if (encoder >= _numEncoders)
  {
    return;
  }
  Channel& ch = _ch[encoder];

  if (newPosition < ch.minPos) newPosition = ch.minPos;
  if (newPosition > ch.maxPos) newPosition = ch.maxPos;

  uint32_t irqState = save_and_disable_interrupts();
  ch.position = newPosition;
  ch.subCount = 0;
  restore_interrupts(irqState);
}

// Returns false (and changes nothing) if maxPos <= minPos or the index is invalid.
// The current position is clamped into the new range.
bool EncoderPIO::setLimits(uint8_t encoder, int minPos, int maxPos)
{
  if (encoder >= _numEncoders || maxPos <= minPos)
  {
    return false;
  }
  Channel& ch = _ch[encoder];

  uint32_t irqState = save_and_disable_interrupts();
  ch.minPos = minPos;
  ch.maxPos = maxPos;
  int pos = ch.position;
  if (pos < ch.minPos) pos = ch.minPos;
  if (pos > ch.maxPos) pos = ch.maxPos;
  ch.position = pos;
  ch.subCount = 0;
  restore_interrupts(irqState);
  return true;
}

void EncoderPIO::setWrap(uint8_t encoder, bool wrap)
{
  if (encoder >= _numEncoders)
  {
    return;
  }
  _ch[encoder].wrap = wrap;
}

// Number of quadrature edges per position step.
//   1 = every edge counts (4 per full cycle)
//   2 = for encoders with 2 edges per detent
//   4 = for the common type with 4 edges (one full cycle) per detent
// Values below 1 are treated as 1; values above 100 are limited to 100.
void EncoderPIO::setCountsPerDetent(uint8_t encoder, uint8_t counts)
{
  if (encoder >= _numEncoders)
  {
    return;
  }
  Channel& ch = _ch[encoder];

  if (counts < 1) counts = 1;
  if (counts > 100) counts = 100;

  uint32_t irqState = save_and_disable_interrupts();
  ch.countsPerDetent = counts;
  ch.subCount = 0;
  restore_interrupts(irqState);
}
