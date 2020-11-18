// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// In order to compile this in the Arduino IDE you will need to install the
// ATTiny core from here: https://github.com/damellis/attiny - use the
// following settings:
//
// - Board: ATTiny25/45/85 (ATTiny85 recommended if you're going to add wavetables)
// - Processor: ATTiny85
// - Clock: Internal 16MHz
// 
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "wavetables.h"

// Wavetable names - the names have no particular significance outside of describing what
// the wavetable actually is.
#define WAVETABLE_SINE        0
#define WAVETABLE_TRIANGLE    1
#define WAVETABLE_SAW         2
#define WAVETABLE_SQUARE      3
#define WAVETABLE_PULSE       4
#define WAVETABLE_NOISE       5

#define BUFF_LENGTH           4
#define BUFF_SHIFT            2

// Input pins
#define CV_INPUT              A2  // ATTiny pin 3
#define WAVE_INPUT            A3  // ATTiny pin 2
#define OP_INPUT              A1  // ATTiny pin 7

// Any variables which are used inside the ISR must be declared as volatile - not doing so could
// result in unpredictable behaviour or, more likely, no behaviour at all.
//
// DO NOT use compiler optimisation flags (-O0 etc.)
volatile uint16_t syncPhaseAcc, syncPhaseInc, baseFreq, syncOffset, finetune;
volatile uint8_t current_wavetable = 0;
volatile uint8_t pitch_offset = 0, phase_offset = 0;
volatile uint8_t range_high = 0;

// Wavetable definitions - the two NULL values are placeholders for the square and sawtooth
// waveforms which can be generated based on the current step. To replace these with a wavetable,
// declare the wavetable in wavetables.h and replace the appropriate NULL with a pointer to the
// beginning of the desired table.
//
// Doing this will also require the code within the ISR to be modified as well.
const uint8_t * const wavetables[] PROGMEM = { &sine[0], &triangle[0], NULL, NULL, &pulse[0], &noise[0] };
uint16_t acc, a1_level, buffered_vals[BUFF_LENGTH], buff_step;

void audioOn() {
  // Enable the on-board PLL as a source for Timer1 (64MHz)
  // Then disable timer interrupts
  PLLCSR = 1 << PCKE | 1 << PLLE;
  TIMSK = 0;

  // PWM1A - Enable pulse-width modulator A
  // COM1A0 - Clear the OC1A output line
  // CS11 - /2 clock prescaler
  TCCR1 = 1 << PWM1A | 2<< COM1A0 | 1 << CS11;
  OCR1A = 128; // 50% duty cycle

  pinMode(0, INPUT_PULLUP);
  pinMode(1, OUTPUT);
  pinMode(CV_INPUT, INPUT);
  pinMode(WAVE_INPUT, INPUT);
  pinMode(OP_INPUT, INPUT);

  TCCR0A = (1 << WGM01);               // CTC mode
  TCCR0B = (1 << WGM02) | (2 << CS00); // Waveform mode, /8 prescaler
  TIMSK = 1 << OCIE0A;                 // Interrupt on compare match
  OCR0A = 29;
  
  syncPhaseAcc = 0;
  buff_step = 0;
  memset(&buffered_vals[0], 0, BUFF_LENGTH);
}

void setup() {
  audioOn();
}

void loop(){
  // Desired frequency - resolution is 5V / 1024 steps = 4.8mV
  // The lookup table for mapFreq yields a response pretty close to 1V/oct.
  // An external ADC could improve matters, at the expense of more code.
  baseFreq = mapFreq(analogRead(CV_INPUT));

  // Since we're potentially getting the output waveform type via a pot, buffer
  // the last few values and set the current wavetable value to be the average
  // of the buffer contents. This goes some way to eliminating jitter.
  //
  // Note the right-shift to do the division.
  //
  // Using a larger buffer could yield more stable results but also increases 
  // the risks of audible glitching in the output. A 4-byte buffer seems to 
  // do the job.Buffer length should be a power of two in order to take
  // advantage of bit-shift division
  //
  // Something similar _could_ be done to the CV inputs
  buffered_vals[buff_step++] = mapOsc(analogRead(WAVE_INPUT));
  if (buff_step == BUFF_LENGTH) {
    acc = 0;
    for (int i=0; i < BUFF_LENGTH; i++) { acc += buffered_vals[i]; }
    current_wavetable = (acc >> BUFF_SHIFT) & 0xff;
    acc = 0;
    buff_step = 0;
  }

  // Handle the fine-tuning
  finetune = -64 + (analogRead(OP_INPUT) >> 3);

  // Set the phase increment value.
  syncPhaseInc = baseFreq + finetune;

  // Check the sense of the range switch - this is acted on within the ISR.
  range_high = (digitalRead(0) == HIGH) ? 1 : 0;
}

// The interrupt service routine - this is where it all happens.
// Triggered when TIMER0 overflows.
//
// - Increment the phase accumulator
// - Convert the accumulator value to an 8-bit number corresponding to
//   the step in the wavetable
// - Read the appropriate wavetable value
// - Write it to the PWM pin
//
// In order to prevent overruns and glitching, code within the ISR should do
// as little as it can get away with. In this case, we're reading a value from
// wavetable and writing it out to a PWM pin - square and sawtooth waves are
// generated on the fly based on the current step; obviously this is much quicker.
ISR(TIMER0_COMPA_vect) {
   uint8_t val, step, phased_step;

   syncPhaseAcc += (range_high == 0) ? syncPhaseInc >> 1 : syncPhaseInc >> 4 ;
   step = syncPhaseAcc >> 8;
   
   phased_step = step + phase_offset;

   switch (current_wavetable) {
     case WAVETABLE_SINE:
         val = pgm_read_byte_near((uint8_t *)wavetables[WAVETABLE_SINE] + step);
         break;
     case WAVETABLE_TRIANGLE:
         val = pgm_read_byte_near((uint8_t *)wavetables[WAVETABLE_TRIANGLE] + step);
         break;
     case WAVETABLE_SAW:
         val = step;
         break;
     case WAVETABLE_SQUARE:
         val = (step < 128) ? 0x00 : 0xff;
         break;
     case WAVETABLE_PULSE:
         val = pgm_read_byte_near((uint8_t *)wavetables[WAVETABLE_PULSE] + step);
         break;
     default: val = pgm_read_byte_near((uint8_t *)wavetables[WAVETABLE_NOISE] + step);
         val = (pgm_read_byte_near((uint8_t *)wavetables[WAVETABLE_NOISE] + phased_step)) << 1;
         break;
   }

   // Writing the current wavetable value directly to OCR1A is much quicker than using
   // digitalWrite() or its equivalent..
   OCR1A = val;
}
