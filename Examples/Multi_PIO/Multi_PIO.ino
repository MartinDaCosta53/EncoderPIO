// An example sketch for multiple rotary encoders using RP2350

/*
  Copyright (C) Martin Da Costa 2026

  This work is licensed under the:
      Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
   To view a copy of this license, visit:
      http://creativecommons.org/licenses/by-nc-sa/4.0/
   or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.

   License summary:
    You are free to:
      Share, copy and redistribute the material in any medium or format
      Adapt, remix, transform, and build upon the material

    The licensor cannot revoke these freedoms as long as you follow the license terms.

    Attribution : You must give appropriate credit, provide a link to the license,
                  and indicate if changes were made. You may do so in any reasonable manner,
                  but not in any way that suggests the licensor endorses you or your use.

    NonCommercial : You may not use the material for commercial purposes. **(see note below)

    ShareAlike : If you remix, transform, or build upon the material, you must distribute
                 your contributions under the same license as the original.

    No additional restrictions : You may not apply legal terms or technological measures that
                                 legally restrict others from doing anything the license permits.

   ** For commercial use, please contact the original copyright holder(s) to agree licensing terms

    This software is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE

*/

/*
      3rd party libraries needed for compilation: (not for binary-only distributions)

      Streaming   -- C++ stream style output, v5, (http://arduiniana.org/libraries/streaming/)
*/

#include <Streaming.h>
#include <EncoderPIO.h>

const byte VER_MAJ = 0;   // code major version
const byte VER_MIN = 0;   // code minor version
const byte VER_PATCH = 0; // code sub-version

const byte PIN_ENC1A = 15;  // Number is GPIO reference
const byte PIN_ENC1B = 16;  // Must be consecutive to PIN_ENC1A
const byte PIN_ENC2A = 18;  // Number is GPIO reference
const byte PIN_ENC2B = 19;  // Must be consecutive to PIN_ENC2A
const byte PIN_ENC3A = 13;  // Number is GPIO reference
const byte PIN_ENC3B = 14;  // Must be consecutive to PIN_ENC3A

byte PIO_0 = 0;
byte PIO_1 = 1;

// Up to 4 encoders can be assigned to a single object.  They will all be on the same PIO.
// Additional encoders can be added in a new object that must use a different PIO.
// Each encoder must have a pair of adjacent GPIO. GPIO cannot be reused.
EncoderPIO groupA(PIO_0, PIN_ENC1A, PIN_ENC2A);
EncoderPIO groupB(PIO_1, PIN_ENC3A);

static const uint8_t NUM_ENCODERS = 3;

int lastPos[NUM_ENCODERS];
int newPos[NUM_ENCODERS];

// Small helpers so the rest of the code can use a single index 0..5
static int encGet(uint8_t n)
{
  return (n < 2) ? groupA.getPosition(n) : groupB.getPosition(n - 2);
}

static void encSetup(uint8_t n, int minPos, int maxPos, bool wrap, uint8_t countsPerDetent)
{
  EncoderPIO& g = (n < 2) ? groupA : groupB;
  uint8_t idx = (n < 2) ? n : n - 2;

  g.setLimits(idx, minPos, maxPos);
  g.setWrap(idx, wrap);
  g.setCountsPerDetent(idx, countsPerDetent);
  g.setPosition(idx, minPos);
}

void printConfig(void)
{
  // code version
  Serial << F("> code version: ") << VER_MAJ << F(".") << VER_MIN << F(".") << VER_PATCH << endl;
  Serial << F("> compiled on ") << __DATE__ << F(" at ") << __TIME__ << F(", compiler ver = ") << __cplusplus << endl;

  // copyright
  Serial << F("> © Martin Da Costa (MERG M6223) 2026") << endl
         << endl;
}

byte edgesPerDetent = 2;
bool encReady = false;

void setup()
{
  uint32_t stimer = millis();
  Serial.begin(115200);
  while (!Serial && millis() - stimer < 2000);
  Serial << F("> PIO example for multiple encoders on multiple PIOs") << endl << endl;
  printConfig();  // show code version and copyright notice 
  
  // Configure before or after initialiseEncoder(); both work.
  // 4 edges per detent is the common type of mechanical encoder.
  encSetup(0, 0, 100, false, edgesPerDetent);   // 0..100, stops at the ends
  encSetup(1, 0, 255, false, edgesPerDetent);   // 0..255, e.g. a brightness
  encSetup(2, 0, 11,  true,  edgesPerDetent);   // 0..11, wraps round, e.g. a menu
   
  // Always check the result: it fails if a pin is invalid or overlaps,
  // the PIO block is already in use, or there are not enough free
  // state machines.
  if (!groupA.initialiseEncoder())
  {
    Serial.println("Encoder group A (PIO0) failed to start");
    while (true) { delay(1000); }
  }
  if (!groupB.initialiseEncoder())
  {
    Serial.println("Encoder group B (PIO1) failed to start");
    while (true) { delay(1000); }
  }

  for (uint8_t i = 0; i < NUM_ENCODERS; i++)
  {
    lastPos[i] = encGet(i);
  }
  
  encReady = true;
  Serial.println("3 encoders running");
}

void loop()
{
  if (!encReady) return;
  bool change = false;
  for (uint8_t i = 0; i < NUM_ENCODERS; i++)
  {
    int p = encGet(i);
    if (p != lastPos[i])
    {
      newPos[i] = p;
      lastPos[i] = p;
      change = true;
    }
  }
  if (change)
  {
    Serial << F("> Encoder 1 = ") << newPos[0] << F(", Encoder 2 = ") << newPos[1] << F(", Encoder 3 = ") << newPos[2] << endl;
  }
}
