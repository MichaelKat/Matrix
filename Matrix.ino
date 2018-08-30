//-------------------------------------------------------------------------------------
//  This program allows an AtMega328p microcontroller (Arduino UNO and the like) to
//  control and animate an LED matrix consisting of 32 columns and 16 rows, for a total 
//  of 512 LED's. The AtMega328p communicates with three TLC5940 IC's; one TLC5940
//  multiplexes the rows, while the other two TLC5940's set each row's values at a given
//  time.
//
//  To avoid flickering and achieve smooth animations, an interrupt service routine (ISR)
//  is implemented to update one row every 16 microseconds.
//
//  So far, this code implements the matrix's ability to:
//    - React to an input audio signal with various animations;
//    - Display scrolling text.
//  
//  Features that are in potential development:
//    - Displaying the time in digital form;
//    - Simple games (e.g. snake).
//
//  The expansions are, however, boundless; it is possible to introduce an ESP8266 wifi
//  module, for example, and make the matrix interactive with any phone through an app.
//
//
//  Author: Michael Katilevsky
//
//  Started: June 6, 2018
//
//
//  #TODO: Add another ISR to keep track of pushbutton inputs to replace inputHandler()
//  #TODO: Remove FHT code duplication in loop()
//  
//-------------------------------------------------------------------------------------  

#include <Tlc5940.h> // include the Tlc5940 library for controlling TLC5940 IC's
#include <tlc_config.h>

#define LIN_OUT8 1 // use the linear 8-bit output function
#define FHT_N 256 // set to 256 point fht

#include <FHT.h> // include the FHT library for audio analysis

#define NUM_ROWS 16
#define NUM_COLUMNS 32

//-------------------------------------------------------------------------------------
// These arrays contain values for different representations. The name of each array
// represents what each array is for (e.g. array A is for the letter 'A'). These are
// stored in program memory to not waste the scarce dynamic memory of the AtMega328p.
//
// Note: each one-directional array has 16 values in it, as my application uses 16 rows.
// If you decide to change NUM_ROWS, make sure you add or remove 0's at the beginning of 
// each array accordingly (i.e. if you were to define NUM_ROWS as 8, you would remove 8 
// 0's from each array because 8 - 16 = -8 0's to add (aka 8 0's to remove). If you were 
// to define NUM_ROWS as 32, you would need to add 16 0's to each array as 32 - 16 = 16 
// 0's to add).
//
// Also, as painful as it is, these have to be put in the beginning of the sketch...
// Genuinely sorry for the inconvenience, and good luck scrolling :(
  
  const int PROGMEM BLANK[NUM_ROWS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  const int PROGMEM A[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 0, 0, 0}};

  const int PROGMEM B[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0}};

  const int PROGMEM C[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM D[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 0, 0}};

  const int PROGMEM E[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0}};

  const int PROGMEM F[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM G[4][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 4095, 0, 0}};

  const int PROGMEM H[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM I[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM J[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM K[4][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM L[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}};

  const int PROGMEM M[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM N[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM O[4][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM P[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM Q[4][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 4095, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM R[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM S[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 4095, 0, 0}};

  const int PROGMEM T[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM U[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM V[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM W[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM X[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM Y[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM Z[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM ZERO[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM ONE[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}};

  const int PROGMEM TWO[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM THREE[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM FOUR[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM FIVE[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 4095, 0, 0}};

  const int PROGMEM SIX[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 4095, 0, 0}};

  const int PROGMEM SEVEN[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM EIGHT[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM NINE[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM EXCLAMATION[NUM_ROWS] = 
                        {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 4095, 4095, 0, 0};

  const int PROGMEM COMMA[2][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0, 0}, 
                        {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}};

  const int PROGMEM APOSTROPHE[2][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM PERIOD[NUM_ROWS] = 
                        {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0};

  const int PROGMEM QUESTION[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0, 4095, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 4095, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 0, 0}};

  const int PROGMEM TILDE[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0}};

  const int PROGMEM FSLASH[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM BSLASH[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}};

  const int PROGMEM DASH[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0}};

  const int PROGMEM UNDERSCORE[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0}};

  const int PROGMEM COLON[NUM_ROWS] = 
                        {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 4095, 0, 0, 0};

  const int PROGMEM SEMICOLON[2][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 4095, 0, 0, 0}};

  const int PROGMEM CARET[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0}};

  const int PROGMEM PERCENT[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 0, 0, 4095, 0, 0}};

  const int PROGMEM DOLLAR[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 0, 4095, 0, 4095, 4095, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 4095, 0, 0}};

  const int PROGMEM PLUS[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0}};

  const int PROGMEM LSQUAREBRK[2][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM RSQUAREBRK[2][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}};

  const int PROGMEM LTHAN[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM GTHAN[3][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 0}};

  const int PROGMEM LPARENTHESIS[2][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}};

  const int PROGMEM RPARENTHESIS[2][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 0, 0, 0}};

  const int PROGMEM AMPERSAND[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 4095, 0, 4095, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 4095, 0, 4095, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 0, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0, 0}};

  const int PROGMEM HASH[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 4095, 4095, 4095, 4095, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0}};

  const int PROGMEM EQUALS[5][NUM_ROWS] = 
                        {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0},
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0}, 
                         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4095, 0, 4095, 0, 0, 0}};

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------


// Which FHT output bins we want to be displayed (preferred frequencies that we wanna see)
const byte selectedBins[NUM_COLUMNS] PROGMEM = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                             17, 18, 19, 21, 23, 25, 27, 29, 36, 43, 51, 58, 72, 87, 101, 116}; // for 256 FHT

//const  byte selectedBins[NUM_COLUMNS] PROGMEM = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
//                             17, 18, 19, 20, 21, 22, 23, 24, 26, 28, 30, 34, 38, 44, 51, 58}; // for 128 FHT

// Amount of milliseconds to wait until shifting in the next column. At 32 columns, text 
// would take ~4 seconds (32*128 = 4096 ms) to travel from the right to the left side of 
// the screen.
#define TEXT_SPEED 128

// Stores values of all pixels of the next frame to be displayed
int frameValues[NUM_COLUMNS][NUM_ROWS];

// Keeps track of the current row that is lit up
byte currentRow;

// A variable that determines which mode the matrix currently operates in.
byte mode;

// A variable that determines which submode the matrix currently operates in.
byte submode;



/*  Interrupt called every 16 us to multiplex the matrix, lights up one row at every call.
 */
ISR(TIMER0_COMPA_vect) {

  if(currentRow == 16) {
    currentRow = 0;
  }
  
  // Turn on current row, i
  Tlc.set(currentRow, 0);
  
  // Set the brightness of every LED on row i
  for(byte j = 0; j < NUM_COLUMNS; j++) {

    Tlc.set(j + NUM_ROWS, frameValues[j][currentRow]);
    
  }
  
  // Display the values of row i
  Tlc.update();

  // Turn off current row at the next call of Tlc.update()
  Tlc.set(currentRow, 4095);
  
  currentRow++;
}


//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
// Below is the executable code


void setup() {

  // Modifying the AtMega's sampling rate to analyze a wider range of audio frequencies
  
  ADCSRA &= ~(bit (ADPS0) | bit (ADPS1) | bit (ADPS2)); // clear prescaler bits
  
  // Reminder: Bin's Hz = Sampling Rate / FHT_N * Bin Number
  //
  // Sampling rate = 16,000,000 / 13 / selected prescaler value
  // Note that there is a loss of precision when prescaler is less than 16
  //
  // Uncomment the desired prescaler value:

//  ADCSRA |= bit (ADPS0);                             //   2  
//  ADCSRA |= bit (ADPS1);                             //   4  
//  ADCSRA |= bit (ADPS0) | bit (ADPS1);               //   8 
//  ADCSRA |= bit (ADPS2);                             //  16 -> 76.92 KHz ADC sampling rate
//  ADCSRA |= bit (ADPS0) | bit (ADPS2);               //  32 -> 38.46 KHz ADC sampling rate
  ADCSRA |= bit (ADPS1) | bit (ADPS2);                 //  64 -> 19.23 KHz ADC sampling rate
//  ADCSRA |= bit (ADPS0) | bit (ADPS1) | bit (ADPS2); // 128 -> 9.615 Khz ADC sampling rate

  ADCSRA = 0xe5; // set the adc to free running mode
  ADMUX = 0x40; // use adc0
  DIDR0 = 0x01; // turn off the digital input for adc0


  // Timer0's compare interrupt triggers the ISR in the code above
  OCR0A = 0x0F; // Set Timer0's compare register to 15
  TIMSK0 |= _BV(OCIE0A); // Set the compare interrupt for Timer0

  
  Tlc.init();

  // Set pins 2 and 4 to be inputs for momentary switches (push buttons)
  pinMode(2, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);

  // Make sure we have a clear frame to begin
  clearFrameValues();
  
}


void loop() {
  mode = 0;
  submode = 0;
  
  // To test quality of LED's
  while(mode == 0) {
    if(submode == 0) {
      allFrameValuesOn();
    }
    
    else if(submode == 1) {
      displayScrollText("Good Morning!");
    }
    
    else if(submode == 2) {
      displayScrollText("The quick brown fox jumps over the lazy dog.");    
    }
  }

  
  // FHT columns going up
  while(mode == 1) {
    for (int i = 0 ; i < FHT_N ; i++) { // save samples
      while(!(ADCSRA & 0x10)); // wait for adc to be ready
      ADCSRA = 0xf5; // restart adc
      byte m = ADCL; // fetch adc data
      byte n = ADCH;
      int k = (n << 8) | m; // form into an int
      k -= 478; // form into a signed int
      k <<= 6; // form into a 16b signed int
      fht_input[i] = k; // put real data into bins
    }
    fht_window(); // window the data for better frequency response
    fht_reorder(); // reorder the data before doing the fht
    fht_run(); // process the data in the fht
    fht_mag_lin8(); // take the output of the fht

    // Display just the first 32 bins
    if(submode == 0) {
      for(byte i = 0; i < NUM_COLUMNS; i++) {
    
        int val = fht_lin_out8[i];

        byte j;

        // Turn on necessary LED's in column i
        for(j = 0; j < val && j < NUM_ROWS; j ++) {
    
          frameValues[i][j] = 4095;
          
        }

        // Turn off all other LED's
        for(j; j < NUM_ROWS; j++) {
          frameValues[i][j] = 0;
        }
      }
    }

    // Display the selected bins; smoothly decreasing columns
    else if(submode == 1) {
      for(byte i = 0; i < NUM_COLUMNS; i++) {

        int val = fht_lin_out8[pgm_read_byte(&(selectedBins[i]))];

        byte j;

        // Turn on necessary LED's in column i
        for(j = 0; j < val && j < NUM_ROWS; j ++) {
    
          frameValues[i][j] = 4095;
          
        }

        // Turn off the other LED's such that only one is decremented at once
        for(j; j < NUM_ROWS; j++) {
          
          if(frameValues[i][j] == 0) {
            break;
          }

          if(j == NUM_ROWS - 1 || frameValues[i][j + 1] == 0) {
            frameValues[i][j] = 0;
            break;
          }
          
        }
      }
    }

    // Display the selected bins
    else if(submode == 2) {
      for(byte i = 0; i < NUM_COLUMNS; i++) {
    
        int val = fht_lin_out8[pgm_read_byte(&(selectedBins[i]))];

        byte j;

        // Turn on necessary LED's in column i
        for(j = 0; j < val && j < NUM_ROWS; j ++) {
    
          frameValues[i][j] = 4095;
          
        }

        // Turn off all other LED's
        for(j; j < NUM_ROWS; j++) {
          frameValues[i][j] = 0;
        }
      }
    }

    else {
      submode = 0;
    }
  }
  
  
  // FHT columns going down
  while(mode == 2) { 
    for (int i = 0 ; i < FHT_N ; i++) { // save samples
      while(!(ADCSRA & 0x10)); // wait for adc to be ready
      ADCSRA = 0xf5; // restart adc
      byte m = ADCL; // fetch adc data
      byte n = ADCH;
      int k = (n << 8) | m; // form into an int
      k -= 478; // form into a signed int
      k <<= 6; // form into a 16b signed int
      fht_input[i] = k; // put real data into bins
    }
    fht_window(); // window the data for better frequency response
    fht_reorder(); // reorder the data before doing the fht
    fht_run(); // process the data in the fht
    fht_mag_lin8(); // take the output of the fht

    // Display just the first 32 bins
    if(submode == 0) {
      for(byte i = 0; i < NUM_COLUMNS; i++) {
    
        int val = fht_lin_out8[i];

        byte j;

        // Turn on necessary LED's in column i
        for(j = NUM_ROWS - 1; NUM_ROWS - 1 - j < val && j > -1; j --) {
    
          frameValues[i][j] = 4095;
          
        }

        // Turn off all other LED's
        for(j; j > -1; j--) {
          frameValues[i][j] = 0;
        }
      }
    }

    // Display selected bins; smoothly decreasing columns
    else if(submode == 1) {
      for(byte i = 0; i < NUM_COLUMNS; i++) {

        int val = fht_lin_out8[pgm_read_byte(&(selectedBins[i]))];

        byte j;

        // Turn on necessary LED's in column i
        for(j = NUM_ROWS - 1; NUM_ROWS - 1 - j < val && j > -1; j --) {
    
          frameValues[i][j] = 4095;
          
        }

        // Turn off the other LED's such that only one is decremented at once
        for(j; j > -1; j--) {
          
          if(frameValues[i][j] == 0) {
            break;
          }

          if(j == 0 || frameValues[i][j - 1] == 0) {
            frameValues[i][j] = 0;
            break;
          }
          
        }
      }
    }

    // Display the selected bins
    else if(submode == 2) {
      for(byte i = 0; i < NUM_COLUMNS; i++) {
    
        int val = fht_lin_out8[pgm_read_byte(&(selectedBins[i]))];

        byte j;

        // Turn on necessary LED's in column i
        for(j = NUM_ROWS - 1; NUM_ROWS - 1 - j < val && j > -1; j --) {
    
          frameValues[i][j] = 4095;
          
        }

        // Turn off all other LED's
        for(j; j > -1; j--) {
          frameValues[i][j] = 0;
        }
      }
    }

    else {
      submode = 0;
    }
  }


  // FHT rows going to the right
  while(mode == 3) { 
    for (int i = 0 ; i < FHT_N ; i++) { // save samples
      while(!(ADCSRA & 0x10)); // wait for adc to be ready
      ADCSRA = 0xf5; // restart adc
      byte m = ADCL; // fetch adc data
      byte n = ADCH;
      int k = (n << 8) | m; // form into an int
      k -= 478; // form into a signed int
      k <<= 6; // form into a 16b signed int
      fht_input[i] = k; // put real data into bins
    }
    fht_window(); // window the data for better frequency response
    fht_reorder(); // reorder the data before doing the fht
    fht_run(); // process the data in the fht
    fht_mag_lin8(); // take the output of the fht

    // Display just the first 16 bins
    if(submode == 0) {
      for(byte i = 0; i < NUM_ROWS; i++) {
    
        int val = fht_lin_out8[i];

        byte j;

        // Turn on necessary LED's in row i
        for(j = 0; j < val && j < NUM_COLUMNS; j ++) {
    
          frameValues[j][i] = 4095;
          
        }

        // Turn off all other LED's
        for(j; j < NUM_COLUMNS; j++) {
          frameValues[j][i] = 0;
        }
      }
    }

    // Display the selected bins; smoothly decreasing rows
    else if(submode == 1) {
      for(byte i = 0; i < NUM_ROWS; i++) {

        int val = fht_lin_out8[pgm_read_byte(&(selectedBins[i*2]))];

        byte j;

        // Turn on necessary LED's in column i
        for(j = 0; j < val && j < NUM_COLUMNS; j ++) {
    
          frameValues[j][i] = 4095;
          
        }

        // Turn off the other LED's such that only one is decremented at once
        for(j; j < NUM_COLUMNS; j++) {
          
          if(frameValues[j][i] == 0) {
            break;
          }

          if(j == NUM_COLUMNS - 1 || frameValues[j + 1][i] == 0) {
            frameValues[j][i] = 0;
            break;
          }
          
        }
      }
    }

    // Display the selected bins
    else if(submode == 2) {
      for(byte i = 0; i < NUM_ROWS; i++) {
    
        int val = fht_lin_out8[pgm_read_byte(&(selectedBins[i*2]))];

        byte j;

        // Turn on necessary LED's in column i
        for(j = 0; j < val && j < NUM_COLUMNS; j ++) {
    
          frameValues[j][i] = 4095;
          
        }

        // Turn off all other LED's
        for(j; j < NUM_COLUMNS; j++) {
          frameValues[j][i] = 0;
        }
      }
    }

    else {
      submode = 0;
    }
  }
  
  
  // FHT rows going to the left
  while(mode == 4) { 
    for (int i = 0 ; i < FHT_N ; i++) { // save samples
      while(!(ADCSRA & 0x10)); // wait for adc to be ready
      ADCSRA = 0xf5; // restart adc
      byte m = ADCL; // fetch adc data
      byte n = ADCH;
      int k = (n << 8) | m; // form into an int
      k -= 478; // form into a signed int
      k <<= 6; // form into a 16b signed int
      fht_input[i] = k; // put real data into bins
    }
    fht_window(); // window the data for better frequency response
    fht_reorder(); // reorder the data before doing the fht
    fht_run(); // process the data in the fht
    fht_mag_lin8(); // take the output of the fht

    // Display just the first 16 bins
    if(submode == 0) {
      for(byte i = 0; i < NUM_ROWS; i++) {
    
        int val = fht_lin_out8[i];

        byte j;

        // Turn on necessary LED's in column i
        for(j = NUM_COLUMNS - 1; NUM_COLUMNS - 1 - j < val && j > -1; j --) {
    
          frameValues[j][i] = 4095;
          
        }

        // Turn off all other LED's
        for(j; j > -1; j--) {
          frameValues[j][i] = 0;
        }
      }
    }

    // Display selected bins; smoothly decreasing rows
    else if(submode == 1) {
      for(byte i = 0; i < NUM_ROWS; i++) {

        int val = fht_lin_out8[pgm_read_byte(&(selectedBins[i*2]))];

        byte j;

        // Turn on necessary LED's in row i
        for(j = NUM_COLUMNS - 1; NUM_COLUMNS - 1 - j < val && j > -1; j --) {
    
          frameValues[j][i] = 4095;
          
        }

        // Turn off the other LED's such that only one is decremented at once
        for(j; j > -1; j--) {
          
          if(frameValues[j][i] == 0) {
            break;
          }

          if(j == 0 || frameValues[j - 1][i] == 0) {
            frameValues[j][i] = 0;
            break;
          }
          
        }
      }
    }

    // Display the selected bins
    else if(submode == 2) {
      for(byte i = 0; i < NUM_ROWS; i++) {
    
        int val = fht_lin_out8[pgm_read_byte(&(selectedBins[i*2]))];

        byte j;

        // Turn on necessary LED's in column i
        for(j = NUM_COLUMNS - 1; NUM_COLUMNS - 1 - j < val && j > -1; j --) {
    
          frameValues[j][i] = 4095;
          
        }

        // Turn off all other LED's
        for(j; j > -1; j--) {
          frameValues[j][i] = 0;
        }
      }
    }

    else {
      submode = 0;
    }
  }

  
  // shiftUp VU
  while(mode == 5) {
    while(!(ADCSRA & 0x10)); // wait for adc to be ready
    ADCSRA = 0xf5; // restart adc
    byte m = ADCL; // fetch adc data
    byte n = ADCH;
    int k = (n << 8) | m; // form into an int
    k -= 512; // form into a signed int
    k <<= 6; // form into a 16b signed int
    k = abs(k);

    int arr[NUM_COLUMNS];

    byte j;

    for(j = 0; j < NUM_COLUMNS/2 && j*25 < k; j++) {

      arr[NUM_COLUMNS/2 + j] = 4095;

      // if NUM_COLUMNS is odd, remove " - 1 "
      arr[NUM_COLUMNS/2 - j - 1] = 4095;
      
    }

    for(j; j < NUM_COLUMNS/2; j++) {

      arr[NUM_COLUMNS/2 + j] = 0;

      // if NUM_COLUMNS is odd, remove " - 1 "
      arr[NUM_COLUMNS/2 - j - 1] = 0;
      
    }

    shiftUp(arr);
    
  }

  // shiftLeft VU
  while(mode == 6) {
    while(!(ADCSRA & 0x10)); // wait for adc to be ready
    ADCSRA = 0xf5; // restart adc
    byte m = ADCL; // fetch adc data
    byte n = ADCH;
    int k = (n << 8) | m; // form into an int
    k -= 512; // form into a signed int
    k <<= 6; // form into a 16b signed int
    k = abs(k);

    int arr[NUM_ROWS];

    byte j;

    for(j = 0; j < NUM_ROWS/2 && j*50 < k; j++) {

      arr[NUM_ROWS/2 + j] = 4095;

      // if NUM_ROWS is odd, remove " - 1 "
      arr[NUM_ROWS/2 - j - 1] = 4095;
      
    }

    for(j; j < NUM_ROWS/2; j++) {

      arr[NUM_ROWS/2 + j] = 0;

      // if NUM_COLUMNS is odd, remove " - 1 "
      arr[NUM_ROWS/2 - j - 1] = 0;
      
    }

    shiftLeft(arr);
    
  }
}



//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
// Below are all functions


/*
 * Displays in the form of scroll text the current selected mode and submode.
 */
void displayMode() {

  String str1 = "Mode ";
  String str2 = ".";

  displayScrollText(str1 + mode + str2 + submode);
  
}

//-------------------------------------------------------------------------------------

/*
 * Updates the matrix's mode and submode of operation given the user inputs.
 * 
 * TO BE REPLACED BY ISR
 * 
 */
void inputHandler() {

  if(digitalRead(2) == LOW) {
    mode++;
    delay(200); // To filter noise from push button
  }
  
  if(digitalRead(4) == LOW) {
    submode++;
    delay(200); // To filter noise from push button
  }
}

//-------------------------------------------------------------------------------------

/*
 * Turns off all LED's, resets the screen.
 */
void clearFrameValues() {
 allFrameValuesOn(0);
}

//-------------------------------------------------------------------------------------

/*
 * Sets all LED's to maximum brightness
 */
void allFrameValuesOn() {
  allFrameValuesOn(4095);
}

//-------------------------------------------------------------------------------------

/*
 * Sets all LED's to specified brightness
 */
void allFrameValuesOn(int brightness) {
  
  for(byte i = 0; i < NUM_ROWS; i++) {
    for(byte j = 0; j < NUM_COLUMNS; j++) { 
        
       frameValues[j][i] = brightness;
       
    }
  }
}

//-------------------------------------------------------------------------------------

/*
 * Implements a delay in milliseconds
 */
void holdFrameForMillis(int waitTime) {

  delay(waitTime);
}


//-------------------------------------------------------------------------------------


/*
 * Implements a delay in seconds
 */
void holdFrameForSeconds(int waitTime) {

  delay(waitTime*1000);
}


//-------------------------------------------------------------------------------------

/*
 * Shifts new brightness values into the matrix from the left
 */
void shiftRight(int arr[NUM_ROWS]) {

  for(byte i = 0; i < NUM_ROWS; i++) {
      for(byte j = 0; j < NUM_COLUMNS - 1; j++) { 
        
         // Translate all values one column to the right
         frameValues[j][i] = frameValues[j+1][i];
         
      }
    }
  
    for(byte i = 0; i < NUM_ROWS; i++) { 
        
      // Set the rightmost column equal to the input values
      frameValues[NUM_COLUMNS-1][i] = arr[i];
         
    }
    
}


//-------------------------------------------------------------------------------------

/*
 * Shifts new brightness values into the matrix from the right
 */
void shiftLeft(int arr[NUM_ROWS]) {

  for(byte i = 0; i < NUM_ROWS; i++) {
      for(byte j = NUM_COLUMNS - 1; j > 0; j--) { 
        
         // Translate all values one column to the left
         frameValues[j][i] = frameValues[j-1][i];
         
      }
    }
  
    for(byte i = 0; i < NUM_ROWS; i++) { 
        
      // Set the leftmost column equal to the input values
      frameValues[0][i] = arr[i];
         
    }
    
}


//-------------------------------------------------------------------------------------

/*
 * Shifts new brightness values into the matrix from the bottom
 */
void shiftUp(int arr[NUM_COLUMNS]) {

  for(byte i = NUM_ROWS; i > 0; i--) {
      for(byte j = NUM_COLUMNS - 1; j > 0; j--) { 
        
         // Translate all values one row up
         frameValues[j][i] = frameValues[j][i-1];
         
      }
    }
  
    for(byte i = 0; i < NUM_COLUMNS; i++) { 
        
      // Set the bottom row equal to the input values
      frameValues[i][0] = arr[i];
         
    }
    
}


//-------------------------------------------------------------------------------------

/*
 * Shifts new brightness values into the matrix from the top
 */
void shiftDown(int arr[NUM_COLUMNS]) {

  for(byte i = 0; i < NUM_ROWS; i++) {
      for(byte j = NUM_COLUMNS - 1; j > 0; j--) { 
        
         // Translate all values one row down
         frameValues[j][i] = frameValues[j][i+1];
         
      }
    }
  
    for(byte i = 0; i < NUM_COLUMNS; i++) { 
        
      // Set the top row equal to the input values
      frameValues[i][NUM_ROWS-1] = arr[i];
         
    }
    
}

//-------------------------------------------------------------------------------------

/*
 * Displays 'text' by smoothly shifting left the characters of the input string,
 * creating a scrolling effect.
 */
void displayScrollText(String text) {
  
  // Blank the screen for half a second and clear values
  clearFrameValues();
  holdFrameForMillis(500);

  text.trim();
  text.toLowerCase();

  // Iterate through every letter and add it onto the frame one at a time
  for(int i = 0; i < text.length(); i++) {

    displayLetter(text.charAt(i));
    
  }

  int copy[NUM_ROWS];

  // Read values of a 'blank' column from memory and store them in 'copy'
  for(int j = 0; j < NUM_ROWS; j++) {
      copy[j] = pgm_read_word(&(BLANK[j]));
  }
  
  // Allow the last letters of the text to exit the frame smoothly
  for(byte i = 0; i < NUM_COLUMNS; i++) {
    
    // Shift left an empty/blank column
    shiftLeft(copy);
    
    // Hold for a brief moment
    holdFrameForMillis(TEXT_SPEED);
    
  }      
}


//-------------------------------------------------------------------------------------

/*
 * Reads values for each letter from memory and shifts them into the frame
 */
void displayLetter(char c) {

  int copy[NUM_ROWS];

  switch(c) {
    case 'a':                              
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(A[i][j]));
        }
        
        shiftLeft(copy); 
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'b':                        
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(B[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      } 
      break;
  
    case 'c':                        
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(C[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'd':                        
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(D[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'e':                        
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(E[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'f':                        
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(F[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'g':                        
      for(byte i = 0; i < 4; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(G[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'h':                        
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(H[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'i':                        
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(I[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'j':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(J[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'k':
      for(byte i = 0; i < 4; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(K[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'l':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(L[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'm':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(M[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      } 
      break;
  
    case 'n':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(N[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'o':
      for(byte i = 0; i < 4; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(O[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'p':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(P[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'q':
      for(byte i = 0; i < 4; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(Q[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'r':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(R[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 's':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(S[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      } 
      break;
  
    case 't':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(T[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'u':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(U[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'v':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(V[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'w':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(W[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'x':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(X[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
    break;
  
    case 'y':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(Y[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case 'z':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(Z[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '0':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(ZERO[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '1':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(ONE[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '2':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(TWO[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '3':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(THREE[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '4':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(FOUR[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '5':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(FIVE[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '6':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(SIX[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '7':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(SEVEN[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '8':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(EIGHT[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '9':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(NINE[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case ' ':
      
      for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(BLANK[j]));
      }
        
      for(int i = 0; i < 2; i++) {
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '!':   
      for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(EXCLAMATION[j]));
        }
        
        shiftLeft(copy);
      holdFrameForMillis(TEXT_SPEED);
      break;
  
    case ',':                    
      for(byte i = 0; i < 2; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(COMMA[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '\'':
      for(byte i = 0; i < 2; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(APOSTROPHE[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;

    case '`':
      for(byte i = 0; i < 2; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(APOSTROPHE[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '.':  
      for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(PERIOD[j]));
        }
        
        shiftLeft(copy);
      holdFrameForMillis(TEXT_SPEED);
      break;
  
    case '?':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(QUESTION[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '~':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(TILDE[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '/':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(FSLASH[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '\\':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(BSLASH[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '-':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(DASH[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '_':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(UNDERSCORE[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case ':': 
      for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(COLON[j]));
        }
        
      shiftLeft(copy);
      holdFrameForMillis(TEXT_SPEED);
      break;
  
    case ';':
      for(byte i = 0; i < 2; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(SEMICOLON[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '^':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(CARET[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '%':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(PERCENT[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '$': 
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(DOLLAR[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '+':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(PLUS[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
    
    case '[':
      for(byte i = 0; i < 2; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(LSQUAREBRK[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;

    case '{':
      for(byte i = 0; i < 2; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(LSQUAREBRK[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case ']':
      for(byte i = 0; i < 2; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(RSQUAREBRK[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;

    case '}':
      for(byte i = 0; i < 2; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(RSQUAREBRK[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '<':
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(LTHAN[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '>': 
      for(byte i = 0; i < 3; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(GTHAN[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '(':
      for(byte i = 0; i < 2; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(LPARENTHESIS[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
      
    case ')':   
      for(byte i = 0; i < 2; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(RPARENTHESIS[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
      
    case '&':   
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(AMPERSAND[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '#':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(HASH[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    case '=':
      for(byte i = 0; i < 5; i++) {
        for(int j = 0; j < NUM_ROWS; j++) {
          copy[j] = pgm_read_word(&(EQUALS[i][j]));
        }
        
        shiftLeft(copy);
        holdFrameForMillis(TEXT_SPEED);
      }
      break;
  
    default:
      return;
      
  }
  
  // Add a blank line after each letter to avoid clustering and 
  // improve readability
  for(int j = 0; j < NUM_ROWS; j++) {
        copy[j] = pgm_read_word(&(BLANK[j]));
      }
      
      shiftLeft(copy);
  holdFrameForMillis(TEXT_SPEED);

}
