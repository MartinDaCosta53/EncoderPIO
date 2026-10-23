// Copyright (C) Martin Da Costa 2026 (martindc.merg@gmail.com)

#include "hardware/irq.h"
#include "EncoderPIO.h"
#include "EncoderPIO.pio.h"

//---------------------------------------------
// Constructor

EncoderPIO::EncoderPIO(uint8_t pioNum, uint8_t enc1Pin)
{
  _pioNum = pioNum;
  _encPin_1A = enc1Pin;
  _encPin_1B = _encPin_1A + 1;
}

EncoderPIO* EncoderPIO::_instances[3] = { nullptr, nullptr, nullptr };

void EncoderPIO::initialiseEncoder()  
{
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
    return;  // invalid PIO number
  }

  _instances[_pioNum] = this;

  // setup state machine
  uint16_t offset = pio_add_program(pio, &EncoderPIO_program);

  uint8_t sm = 0;
  pio_gpio_init(pio, _encPin_1A);
  gpio_set_pulls(_encPin_1A, true, false);
  pio_gpio_init(pio, _encPin_1B);
  gpio_set_pulls(_encPin_1B, true, false);

  pio_sm_config c = EncoderPIO_program_get_default_config(offset);
  sm_config_set_in_pin_base(&c, _encPin_1A);
  sm_config_set_in_shift(&c, false, false, 0);
  pio_sm_init(pio, sm, offset + 16, &c);  // program starts after the 16-entry jump table

  // Enable irqs
  pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
  pio_set_irq1_source_enabled(pio, pis_interrupt1, true);

  uint irq0 = (_pioNum == 0) ? PIO0_IRQ_0 : (_pioNum == 1) ? PIO1_IRQ_0 :
#if PICO_RP2350
              PIO2_IRQ_0;
#else
              PIO1_IRQ_0;
#endif
  uint irq1 = (_pioNum == 0) ? PIO0_IRQ_1 : (_pioNum == 1) ? PIO1_IRQ_1 :
#if PICO_RP2350
              PIO2_IRQ_1;
#else
              PIO1_IRQ_1;
#endif

  irq_add_shared_handler(irq0, dec_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
  irq_set_enabled(irq0, true);
  irq_add_shared_handler(irq1, inc_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
  irq_set_enabled(irq1, true);

  pio_sm_set_enabled(pio, sm, true);
}

void EncoderPIO::dec_irq_handler()  //pio_int_0 handler
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

void EncoderPIO::decPos()
{
  _position -= 1;
  if (_position < _minPos)
  {
    if (_wrap)
    {
      setPosition(_maxPos);
    }
    else
    {
      setPosition(_minPos);
    }
  }
}   

void EncoderPIO::inc_irq_handler()  //pio_int_1 handler
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

void EncoderPIO::incPos()
{
  _position += 1;
  if (_position > _maxPos)
  {
    if (_wrap)
    {
      setPosition(_minPos);
    }
    else
    {
      setPosition(_maxPos);
    }
  }
}

int EncoderPIO::getPosition()
{
  return _position;
}

void EncoderPIO::setPosition(int newPosition)
{
  _position = newPosition;
}

void EncoderPIO::setLimits(int minPos, int maxPos)
{
  if (maxPos > minPos)
  {
    _minPos = minPos;
    _maxPos = maxPos;
  }
  setPosition(_minPos);
}

void EncoderPIO::setWrap(bool wrap)
{
  _wrap = wrap;
}
