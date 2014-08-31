/*
 * Sketch to turn the Microview into an Audio Frequency analyzer.
 * It requires a Microphone that registers between 0..5V output, to be sampled by the Arduino.
 * This requires a pre-amplifier, because a typical microphone delivers much less.
 * In my project I used one from Adafruit: http://www.adafruit.com/products/1063
 * 
 * Useful hints about Arduino audio sampling were found here: http://www.microsmart.co.za/technical/2014/03/01/advanced-arduino-adc/
 * The FFT library is from this site: http://wiki.openmusiclabs.com/wiki/ArduinoFFT
 * 
 * Wiring is easy: Connect the Mic's audio out to A0.
 * The mic needs its own power supply, because the Vout from the Microview is way too noisy.
 * I used a 1S LiPo (3.7 - 4.2 Volt) to power the mic.
*/

// Constants for FFT
#define LOG_OUT 1 // use the log output function. For a graphical display, this gives the best result.
// Note: 128 and 256 work. Lower values require code changes (in the display routine)
#define FFT_N 128 // Use 128 bins (actually 64, the upper 64 are redundant)
// Define ADC prescaler values
const unsigned char PS_16 = (1 << ADPS2); // 96'800 Hz sampling frequency
const unsigned char PS_32 = (1 << ADPS2) | (1 << ADPS0); // 38'400 Hz sampling frequency
const unsigned char PS_64 = (1 << ADPS2) | (1 << ADPS1); // 19'200 Hz sampling frequency
const unsigned char PS_128 = (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); //9600 Hz sampling Frequency, Arduino default and the only recommended value

#include <FFT.h> // include the library
#include <MicroView.h>	// include MicroView library

// Note: The sampling part is largely taken from the FFT sample sketch, including the comments
void setup() {
  TIMSK0 = 0; // turn off timer0 for lower jitter
  ADCSRA = 0xe5; // set the adc to free running mode
  // set up the ADC
  ADCSRA &= ~PS_128;  // clear default bits set by Arduino library
  // Note: I didn't see any difference between default value and 38kHz, which is surprising.
  // The default 9.6kHz sampling rate should show folding artifacts above 5000 Hz, because
  // no filter is used
  ADCSRA |= PS_32; // 32 prescaler, equals ca. 38kHz sampling frequency
  ADMUX = 0x40; // use adc0
  DIDR0 = 0x01; // turn off the digital input for adc0
  // Set up Microview
  uView.begin(); // start MicroView
  uView.clear(PAGE); // clear page
}

void loop() {
  while (1) { // reduces jitter (loop() is only called once)
    cli();  // UDRE interrupt slows this way down on arduino1.0
    for (int i = 0 ; i < FFT_N ; i++) { // save FFT_N samples
      while (!(ADCSRA & 0x10)); // wait for adc to be ready
      ADCSRA = 0xf5; // restart adc
      byte m = ADCL; // fetch adc data, read low and high byte
      byte j = ADCH; // High contains only 2 bits (total 10 bit resolution)
      int k = (j << 8) | m; // form into an int
      k -= 0x0200; // form into a signed int
      k <<= 6; // form into a 16b signed int
      fft_input[i*2] = k; // put real data into even bins
      fft_input[i*2 + 1] = 0; // set odd bins to 0
    }
    fft_window(); // window the data for better frequency response
    fft_reorder(); // reorder the data before doing the fft
    fft_run(); // process the data in the fft
    fft_mag_log(); // take the output of the fft
    sei();
    uView.clear(PAGE);
    // We have 64 rows available to display the result
    // Depending on the number of bins, not all bin values are read therefore.
    // According to the FFT docu, half of FFT_N will contain the data.
    int scale = FFT_N / 64 / 2;
    for (int i = 0; i < 64; i++) {
      // Draw a line from the bottom (i,47) up to the scaled bin value
      // The scale factor (4 in this case) is a bit of trial and error
      // Throretically the max is 1023, but if the Mic is driven with
      // e.g. 3.3 Volts it will be less.
      uView.line(i, 47, i, 47-(fft_log_out[i*scale]/4));
    }
    uView.display();
  }
}
