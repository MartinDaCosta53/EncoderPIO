// An example sketch for a single rotary encoder using RP2350

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

const byte PIN_ENCA = 15;  // Number is GPIO reference
const byte PIN_ENCB = 16;  // Must be consecutive to PIN_ENC_A

byte encoderPIO = 0;

EncoderPIO encoder(encoderPIO, PIN_ENCA);

int newPos = 0;
int oldPos = 0;
int minPos = -10;
int maxPos = 10;
byte counts = 2;
bool encReady = false;

void setup() {
  uint32_t stimer = millis();
  Serial.begin(115200);
  while (!Serial && millis() - stimer < 2000);
  Serial << F("> PIO example for single encoder") << endl << endl;
  printConfig();  // show code version and copyright notice
 
    
  if (encoder.initialiseEncoder())
  {
    Serial << F("> Encoder Initialised") << endl;
  }
  else
  {
    Serial << F("> Encoder failed to Initialise") << endl;
    while (true) { }
  }
  
  encReady = true;  

  bool limitsValid = encoder.setLimits(minPos, maxPos); // returns false if min >= max
  encoder.setPosition(newPos);
  encoder.setWrap(false);
  encoder.setCountsPerDetent(counts);  

  Serial << F("> Encoder position = ") << newPos << endl;
}

void loop()
  {
    if (!encReady) return;
    newPos = encoder.getPosition();
    if (newPos != oldPos) {
    Serial << F("> Encoder position = ") << newPos << endl;
    oldPos = newPos;
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