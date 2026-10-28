// Copyright (C) Martin Da Costa 2026 (martindc.merg@gmail.com)
// Licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
// The full licence can be found at: http://creativecommons.org/licenses/by-nc-sa/4.0/

#pragma once

#include "Arduino.h"
#include "hardware/pio.h"

// One EncoderPIO object drives 1 to 4 encoders on a single PIO block
// (one state machine per encoder). Only one EncoderPIO object per PIO block
// (2 blocks on RP2040, 3 on RP2350). The PIO program must load at offset 0.
//
// Each encoder is given by the GPIO of its A signal; B must be on the next GPIO.
// The {A, A+1} pin pairs of the encoders must not overlap.
// The encoder index used by the methods below is the order of the pins in the
// constructor: 0 = first pin, 1 = second pin, and so on. It defaults to 0, so
// code written for a single encoder does not need to change.
//
// Call initialiseEncoder() and end() from the same core: the PIO interrupt
// handler is registered on the core that calls initialiseEncoder().
class EncoderPIO
{
public:
  static constexpr uint8_t MAX_ENCODERS = 4;

  EncoderPIO(uint8_t pioNum, uint8_t enc1PinA);
  EncoderPIO(uint8_t pioNum, uint8_t enc1PinA, uint8_t enc2PinA);
  EncoderPIO(uint8_t pioNum, uint8_t enc1PinA, uint8_t enc2PinA, uint8_t enc3PinA);
  EncoderPIO(uint8_t pioNum, uint8_t enc1PinA, uint8_t enc2PinA, uint8_t enc3PinA, uint8_t enc4PinA);
  ~EncoderPIO();  // calls end()

  // The object is registered with the interrupt handler by address, so it must not be copied
  EncoderPIO(const EncoderPIO&) = delete;
  EncoderPIO& operator=(const EncoderPIO&) = delete;

  bool initialiseEncoder();                                      // true on success; check the result
  void end();                                                    // stops the encoders and releases the PIO resources; safe to call more than once
  void setPullUps(bool enable);                                  // internal pull-ups on A and B (default true); may be called before or after initialiseEncoder()

  int  getPosition() { return getPosition(0); }
  int  getPosition(uint8_t encoder);                             // 0 if the index is invalid
  void setPosition(int newPosition) { setPosition(0, newPosition); }
  void setPosition(uint8_t encoder, int newPosition);            // clamped to the limits
  bool setLimits(int minPos, int maxPos) { return setLimits(0, minPos, maxPos); }
  bool setLimits(uint8_t encoder, int minPos, int maxPos);       // false if maxPos <= minPos or bad index
  void setWrap(bool wrap) { setWrap(0, wrap); }
  void setWrap(uint8_t encoder, bool wrap);
  void setCountsPerDetent(uint8_t counts) { setCountsPerDetent(0, counts); }
  void setCountsPerDetent(uint8_t encoder, uint8_t counts);      // edges per step: 1 (default) to 100; 2 or 4 for common detented encoders

private:
  // State for one encoder
  struct Channel
  {
    uint8_t pinA = 0;
    uint8_t sm = 0;

    // Changed in the ISR, so volatile
    volatile int    position = 0;
    volatile int8_t subCount = 0;     // edges accumulated towards the next step

    int     minPos = 0;
    int     maxPos = 100;
    bool    wrap   = false;
    volatile uint8_t countsPerDetent = 1;
  };

  // Static so they can be registered with irq_add_shared_handler().
  // One per PIO block, so two blocks never drain the same FIFOs.
  static void pio0_irq_handler();
  static void pio1_irq_handler();
  static void pio2_irq_handler();

  typedef void (*IrqHandler)();
  static IrqHandler handlerFor(uint8_t pioNum);

  // One entry per PIO block (pio0, pio1, pio2 on RP2350)
  static EncoderPIO* _instances[3];

  void configure(uint8_t pioNum, uint8_t count);
  void serviceFifos();
  void applyPulls();
  static void decPos(Channel& ch);
  static void incPos(Channel& ch);

  PIO     _pio = nullptr;     // must NOT be static
  uint8_t _pioNum = 0;
  uint8_t _numEncoders = 0;
  uint    _offset = 0;
  bool    _pullUps = true;
  bool    _initialised = false;

  Channel _ch[MAX_ENCODERS];
};
