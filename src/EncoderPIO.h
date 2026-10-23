// Copyright (C) Martin Da Costa 2026 (martindc.merg@gmail.com)
// Licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
// The full licence can be found at: http://creativecommons.org/licenses/by-nc-sa/4.0/

#pragma once

#ifndef EncoderPIO_h
#define EncoderPIO_h

#include "Arduino.h"
#include "hardware/pio.h"
#include "hardware/irq.h"

#include "EncoderPIO.pio.h"

class EncoderPIO
{
public:
  EncoderPIO(uint8_t pioNum, uint8_t enc1Pin);

  void initialiseEncoder();
  int  getPosition();
  void setPosition(int newPosition);
  void setLimits(int minPos, int maxPos);
  void setWrap(bool wrap);

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

  volatile int _position = 0;   // changed in the ISR, so volatile
  int  _minPos = 0;
  int  _maxPos = 100;
  bool _wrap   = false;
};

#endif