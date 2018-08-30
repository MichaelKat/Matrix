# Matrix
Controlling a 32X16 LED Matrix Using an AtMega328p and TLC5940's

This program allows an AtMega328p microcontroller (Arduino UNO and the like) to
control and animate an LED matrix consisting of 32 columns and 16 rows, for a total 
of 512 LED's. The AtMega328p communicates with three TLC5940 IC's; one TLC5940
multiplexes the rows, while the other two TLC5940's set each row's values at a given
time.

This is a work in progress. So far, this code implements the matrix's ability to:
    - React to an input audio signal with various animations;
    - Display scrolling text.
  
  Features that are in development:
    - Being able to switch operation modes given user input;
    - Displaying the time in digital form;
    - Potentially simple games (e.g. snake).

Author: Michael Katilevsky

Started: June 6, 2018
