ABOUT
-----
ClockIt TEXT. A modification by Dave Parker to ClockIt
written by Nathan Seidle at Spark Fun Electronics

Product URL: https://www.sparkfun.com/products/retired/9205

A basic alarm clock that uses a 4 digit 7-segment display. Includes alarm and snooze.
Alarm will turn back on after 9 minutes if alarm is not disengaged. 

To switch to text display, press and hold DOWN then press and hold SNOOZE for
two seconds. Repeat to go back to regular display mode.

The other modification is to dim the display at 7PM and brighten the display at 7AM.


BUILDING and PROGRAMMING
------------------------
Set fuses:
avrdude -p m168 -P lpt1 -c stk200 -U lfuse:w:0xE6:m

program hex:
avrdude -p m168 -P lpt1 -c stk200 -U flash:w:clockit-text.hex

or:
make
make program   (you may need to alter the makefile for your programmer)
