// Copyright (C) Martin Da Costa 2026 (martindc.merg@gmail.com)
// Licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
// The full licence can be found at: http://creativecommons.org/licenses/by-nc-sa/4.0/

#pragma once

#include "Arduino.h"
#include "hardware/pio.h"
#include "hardware/irq.h"

#include "EncoderPIO.pio.h"

// One encoder per PIO block (2 on RP2040, 3 on RP2350).
// The PIO program must load at offset 0 of its PIO block.
class EncoderPIO
{
public:
  EncoderPIO(uint8_t pioNum, uint8_t enc1PinA);

  bool initialiseEncoder();                    // true on success; check the result
  int  getPosition();
  void setPosition(int newPosition);           // clamped to the limits
  bool setLimits(int minPos, int maxPos);      // false if maxPos <= minPos
  void setWrap(bool wrap);
  void setCountsPerDetent(uint8_t counts);     // 1 (default), 2 or 4 edges per step

private:
  // Static so they can be registered with irq_add_shared_handler()
  static void dec_irq_handler();
  static void inc_irq_handler();

  // One entry per PIO block (pio0, pio1, pio2 on RP2350)
  static EncoderPIO* _instances[3];

  void decPos();
  void incPos();

  PIO pio;            // must NOT be static
  uint8_t _pioNum;
  uint8_t _encPin_1A;
  uint8_t _encPin_1B;
  uint8_t _sm = 0;
  uint    _offset = 0;
  bool    _initialised = false;

  // Changed in the ISR, so volatile
  volatile int    _position = 0;
  volatile int8_t _subCount = 0;      // edges accumulated towards the next step

  int     _minPos = 0;
  int     _maxPos = 100;
  bool    _wrap   = false;
  volatile uint8_t _countsPerDetent = 1;
};
