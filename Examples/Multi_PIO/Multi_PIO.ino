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

const byte VER_MAJ = 1;   // code major version
const byte VER_MIN = 0;   // code minor version
const byte VER_PATCH = 0; // code sub-version

//---------------------------------------------
// Encoder pins
//
// List the GPIO of the A signal of each encoder; B must be on the next GPIO,
// so each encoder uses a pair {A, A+1} and the pairs must not overlap.
// The position in this list is the encoder number used in the output below.
// The encoder pins do not have to be contiguous or increasing as long as they 
// conform with the requirements in the first sentence of this comment block.
//
// This list has 6 entries but can be shrunk or expanded up to the maximum capacity
// Capacity: 4 encoders per PIO block (one state machine each)
//   RP2040: 2 PIO blocks = 8 encoders
//   RP2350: 3 PIO blocks = 12 encoders

const byte ENCODER_PINS[] = { 9, 11, 13, 15, 17, 19 };
const byte NUM_ENCODERS = sizeof(ENCODER_PINS) / sizeof(ENCODER_PINS[0]);

int oldPos[NUM_ENCODERS];
int newPos[NUM_ENCODERS];
int minPos[NUM_ENCODERS] = {-10, 0, 0, 0, -180, 0};
int maxPos[NUM_ENCODERS] = {10, 100, 255, 360, 180, 100};
bool wrap[NUM_ENCODERS] = {false, true, false, true, true, false};
byte edgesPerDetent = 2;

//---------------------------------------------
// PIO allocation: encoders fill one PIO block completely (4 state machines)
// before the next block is used. Encoder i lives on PIO block i / 4 as
// encoder number i % 4 within that block's EncoderPIO object.

const byte ENC_PER_PIO = EncoderPIO::MAX_ENCODERS;                      // 4
const byte PIOS_USED = (NUM_ENCODERS + ENC_PER_PIO - 1) / ENC_PER_PIO;  // PIO blocks actually needed

EncoderPIO* blocks[PIOS_USED];   // one EncoderPIO object per PIO block used

// Creates the EncoderPIO object for one PIO block, handling 1 to 4 encoders
EncoderPIO* makeBlock(byte pioNum, byte first, byte count)
{
  const byte* p = &ENCODER_PINS[first];
  switch (count)
  {
  case 1: return new EncoderPIO(pioNum, p[0]);
  case 2: return new EncoderPIO(pioNum, p[0], p[1]);
  case 3: return new EncoderPIO(pioNum, p[0], p[1], p[2]);
  case 4: return new EncoderPIO(pioNum, p[0], p[1], p[2], p[3]);
  default: return nullptr;
  }
}

// helpers so the rest of the code can use a single index i
EncoderPIO& blockFor(byte i) { return *blocks[i / ENC_PER_PIO]; } // Finds the PIO block for encoder i
byte        indexIn(byte i)  { return i % ENC_PER_PIO; }  // Finds the encoder within the PIO

bool encReady = false;

void setup()
{
  uint32_t stimer = millis();
  Serial.begin(115200);
  while (!Serial && millis() - stimer < 2000);
  Serial << F("> PIO example for multiple encoders on multiple PIOs") << endl << endl;
  printConfig();  // show code version and copyright notice 
  
  // Build and start one EncoderPIO object per PIO block, filling each block before using the next
  for (byte b = 0; b < PIOS_USED; b++)
  {
    byte first = b * ENC_PER_PIO;
    byte count = min((int)ENC_PER_PIO, (int)NUM_ENCODERS - first);

    blocks[b] = makeBlock(b, first, count);

    if (blocks[b] && blocks[b]->initialiseEncoder())
    {
      Serial << F("> PIO") << b << F(": ") << count << F(" encoder(s) initialised (encoders ")
             << first << F(" to ") << (first + count - 1) << F(")") << endl;
    }
    else
    {
      Serial << F("> PIO") << b << F(": failed to initialise") << endl;
      while (true) { }
    }
  }
  encReady = true;
  
  // Configure every encoder
  for (byte i = 0; i < NUM_ENCODERS; i++)
  {
    EncoderPIO& blk = blockFor(i);
    byte idx = indexIn(i);

    newPos[i] = 0;
    oldPos[i] = 0;

    bool limitsValid = blk.setLimits(idx, minPos[i], maxPos[i]); // returns false if min >= max
    if (!limitsValid)
    {
      Serial << F("> Encoder ") << i << F(": invalid limits") << endl;
    }
    blk.setPosition(idx, newPos[i]);
    blk.setWrap(idx, wrap[i]);
    blk.setCountsPerDetent(idx, edgesPerDetent);
  }

  Serial << endl << F("> All encoders at initialisation position.") << endl;
}

void loop()
{
  if (!encReady) return;

  for (byte i = 0; i < NUM_ENCODERS; i++)
  {
    newPos[i] = blockFor(i).getPosition(indexIn(i));
    if (newPos[i] != oldPos[i]) {
      Serial << F("> Encoder ") << i << F(" position = ") << newPos[i] << endl;
      oldPos[i] = newPos[i];
    }
  }
}

void printConfig(void) {
  // code version
  Serial << F("> code version: ") << VER_MAJ << F(".") << VER_MIN << F(".") << VER_PATCH << endl;
  Serial << F("> compiled on ") << __DATE__ << F(" at ") << __TIME__ << F(", compiler ver = ") << __cplusplus << endl;

  // copyright
  Serial << F("> © Martin Da Costa (MERG M6223) 2026") << endl
         << endl;
}
