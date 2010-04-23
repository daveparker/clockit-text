/*
  ClockIt TEXT. A modification by Dave Parker to ClockIt
  written by Nathan Seidle at Spark Fun Electronics

  A basic alarm clock that uses a 4 digit 7-segment display. Includes alarm and snooze.
  Alarm will turn back on after 9 minutes if alarm is not disengaged. 

  To switch to text display, press and hold DOWN then press and hold SNOOZE for
  two seconds. Repeat to go back to regular display mode.

  The other modification is to dim the display at 7PM and brighten the display at 7AM.
  
  Alarm is through a piezo buzzer.
  Three input buttons (up/down/snooze)
  1 slide switch (engage/disengage alarm)
  
  Display is with PWM of segments - no current limiting resistors!
  
  Uses external 16MHz clock as time base.
  
  Set fuses:
  avrdude -p m168 -P lpt1 -c stk200 -U lfuse:w:0xE6:m
  
  program hex:
  avrdude -p m168 -P lpt1 -c stk200 -U flash:w:clockit-text.hex

  or:
  make
  make program   (you may need to alter the makefile for your programmer)

*/

#define NORMAL_TIME

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>

#define sbi(port, pin)   ((port) |= (uint8_t)(1 << pin))
#define cbi(port, pin)   ((port) &= (uint8_t)~(1 << pin))

#define FOSC 16000000 //16MHz internal osc

#define STATUS_LED  5 //PORTB

#define TRUE   1
#define FALSE  0

#define SEG_A  PORTC3
#define SEG_B  PORTC5
#define SEG_C  PORTC2
#define SEG_D  PORTD2
#define SEG_E  PORTC0
#define SEG_F  PORTC1

#define DIG_1  PORTD0
#define DIG_2  PORTD1
#define DIG_3  PORTD4
#define DIG_4  PORTD6

#define DP     PORTD5
#define COL    PORTD3

#define BUT_UP    PORTB5
#define BUT_DOWN  PORTB4
#define BUT_SNOOZE  PORTD7
#define BUT_ALARM  PORTB0

#define BUZZ1  PORTB1
#define BUZZ2  PORTB2

#define AM  1
#define PM  2

#define BRIGHT 50
#define DIM 1
#define DIM_BEFORE_HOUR 7
#define BRIGHT_AFTER_HOUR 7

enum { SHOW_TIME, SET_TIME, SHOW_ALARM, SET_ALARM } program_state = SHOW_TIME;

//Declare functions
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void ioinit (void);
void delay_ms(uint16_t x); // general purpose delay
void delay_us(uint16_t x);

void siren(void);
void display_number(uint8_t number, uint8_t digit);
void display_time(uint16_t time_on);
void display_alarm_time(uint16_t time_on);
void clear_display(void);
void check_buttons(void);
void check_alarm(void);

void update_time_str(void);
uint8_t append_str(char *dest, char *source);
uint8_t append_str_P(char *dest, char *source);
void display_time_str(uint16_t time_on);
void display_character(uint8_t character, uint8_t position);
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Declare global variables
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
uint8_t hours, minutes, seconds, ampm, flip;
uint8_t hours_alarm, minutes_alarm, seconds_alarm, ampm_alarm, flip_alarm;
uint8_t hours_alarm_snooze, minutes_alarm_snooze, seconds_alarm_snooze, ampm_alarm_snooze;
uint8_t alarm_going;
uint8_t snooze;

char time_str[30];
uint8_t time_str_display_index = 0;
uint8_t scroll_count = 0;
uint8_t show_time_str = FALSE;
uint8_t bright_level = BRIGHT;
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

prog_char num_string_0[] PROGMEM = "Zero";
prog_char num_string_1[] PROGMEM = "One";
prog_char num_string_2[] PROGMEM = "Two";
prog_char num_string_3[] PROGMEM = "Three";
prog_char num_string_4[] PROGMEM = "Four";
prog_char num_string_5[] PROGMEM = "Five";
prog_char num_string_6[] PROGMEM = "Six";
prog_char num_string_7[] PROGMEM = "Seven";
prog_char num_string_8[] PROGMEM = "Eight";
prog_char num_string_9[] PROGMEM = "Nine";
prog_char num_string_10[] PROGMEM = "Ten";
prog_char num_string_11[] PROGMEM = "Eleven";
prog_char num_string_12[] PROGMEM = "Twelve";
prog_char num_string_13[] PROGMEM = "Thirteen";
prog_char num_string_14[] PROGMEM = "Fourteen";
prog_char num_string_15[] PROGMEM = "Fifteen";
prog_char num_string_16[] PROGMEM = "Sixteen";
prog_char num_string_17[] PROGMEM = "Seventeen";
prog_char num_string_18[] PROGMEM = "Eighteen";
prog_char num_string_19[] PROGMEM = "Nineteen";

PROGMEM const char *num_table[] = {   
  num_string_0,
  num_string_1,
  num_string_2,
  num_string_3,
  num_string_4,
  num_string_5,
  num_string_6,
  num_string_7,
  num_string_8,
  num_string_9,
  num_string_10,
  num_string_11,
  num_string_12,
  num_string_13,
  num_string_14,
  num_string_15,
  num_string_16,
  num_string_17,
  num_string_18,
  num_string_19,
};

prog_char tens_string_0[]  PROGMEM = "Oh";
prog_char tens_string_10[] PROGMEM = "Ten";
prog_char tens_string_20[] PROGMEM = "Twenty";
prog_char tens_string_30[] PROGMEM = "Thirty";
prog_char tens_string_40[] PROGMEM = "Forty";
prog_char tens_string_50[] PROGMEM = "Fifty";

PROGMEM const char *tens_table[] = {   
  tens_string_0,
  tens_string_10,
  tens_string_20,
  tens_string_30,
  tens_string_40,
  tens_string_50,
};

PROGMEM char CHARACTERS[] = {
//0bD0BGACFE
  0b00111111, // A
  0b10010111, // b
  0b10001011, // C
  0b10110101, // d
  0b10011011, // E
  0b00011011, // F
  0b10001111, // G
  0b00110111, // h
  0b00000011, // i
  0b10100101, // J
  0b00011111, // k
  0b10000011, // L
  0b10101010, // M
  0b00101111, // N
  0b10010101, // O
  0b00111011, // P
  0b00111110, // q
  0b00001011, // r
  0b00010110, // s
  0b10010011, // t
  0b10100111, // U
  0b10100010, // v
  0b10001101, // w
  0b10011000, // x
  0b10110110, // Y
  0b00110001, // z
};

PROGMEM char DIGITS[] = {
//0bD0BGACFE
  0b10101111, // 0
  0b00100100, // 1
  0b10111001, // 2
  0b10111100, // 3
  0b00110110, // 4
  0b10011110, // 5
  0b10011111, // 6
  0b00101100, // 7
  0b10111111, // 8
  0b10111110, // 9
};

PROGMEM char PUNCTUATION[] = {
//0bD0BGACFE
  0b00010000, // -
  0b00100000, // '
};

ISR (SIG_OVERFLOW1) 
{
  //Prescalar of 1024
  //Clock = 16MHz
  //15,625 clicks per second
  //64us per click  
    
  TCNT1 = 49911; //65536 - 15,625 = 49,911 - Preload timer 1 for 49,911 clicks. Should be 1s per ISR call

  //Debug with faster time!
  //TCNT1 = 63581; //65536 - 1,953 = 63581 - Preload timer 1 for 63581 clicks.
  // Should be 0.125s per ISR call - 8 times faster than normal time
  
  flip_alarm = 1;
  
  if(flip == 0)
    flip = 1;
  else
    flip = 0;
    
  seconds++;
  if(seconds == 60)
  {
    seconds = 0;
    minutes++;
    if(minutes == 60)
    {
      minutes = 0;
      hours++;

      if(hours == 12)
      {
        if(ampm == AM)
          ampm = PM;
        else
          ampm = AM;
      }

      if(hours == 13) hours = 1;
    }
    update_time_str();
  }
  if (program_state != SET_TIME) {
    bright_level = (((hours < DIM_BEFORE_HOUR || hours == 12) && ampm == AM) || \
      (hours > BRIGHT_AFTER_HOUR && hours !=12 && ampm == PM)) ? DIM : BRIGHT;
  }
}

ISR (SIG_OVERFLOW2) 
{
  if (program_state == SHOW_TIME && show_time_str == TRUE) {
    display_time_str(10);
  } else {
    display_time(10); //Display current time for 1ms
  }

  if (scroll_count == 10) {
    scroll_count = 0;
    time_str_display_index++;
    if (strlen(time_str) - time_str_display_index < 4) {
     time_str_display_index = 0;
    }
  } else {
    scroll_count++;
  }
}

void update_time_str(void)
{
  uint8_t index = 0;
  char *spacer = "    ";
  char *space = " ";

  index += append_str(time_str + index, spacer) - 1;
  index += append_str_P(time_str + index, (char*)pgm_read_word(&(num_table[hours]))) - 1;
  index += append_str(time_str + index, space) - 1;

  if (minutes == 0) {
    index += append_str(time_str + index, "O'Clock") - 1;
  } else if (minutes >= 10 && minutes <= 19) {
      index += append_str_P(time_str + index, (char*)pgm_read_word(&(num_table[minutes]))) - 1;
  } else {
    uint8_t tens = minutes / 10;
    uint8_t ones = minutes % 10;
    index += append_str_P(time_str + index, (char*)pgm_read_word(&(tens_table[tens]))) - 1;
    if(ones != 0) {
      index += append_str(time_str + index, "-") - 1;
      index += append_str_P(time_str + index, (char*)pgm_read_word(&(num_table[ones]))) - 1;
    }
  }

  if (ampm == AM) {
    index += append_str(time_str + index, " AM") - 1;
  } else {
    index += append_str(time_str + index, " PM") - 1;
  }

  index += append_str(time_str + index, spacer) - 1;
  time_str_display_index = 0;
}

uint8_t append_str(char *dest, char *source)
{
  uint8_t index = 0;
  char temp_char;
  do {
    temp_char = source[index];
    dest[index] = temp_char;
    index++;
  } while (temp_char != 0);
  return index;
}

uint8_t append_str_P(char *dest, char *source)
{
  uint8_t index = 0;
  char temp_char;
  do {
    temp_char = (char)pgm_read_byte(source + index);
    dest[index] = temp_char;
    index++;
  } while (temp_char != 0);
  return index;
}


int main (void)
{
  ioinit(); //Boot up defaults

  hours = 12;
  minutes = 00;
  seconds = 00;
  ampm = PM;

  hours_alarm = 10;
  minutes_alarm = 00;
  seconds_alarm = 00;
  ampm_alarm = AM;

  hours_alarm_snooze = 12;
  minutes_alarm_snooze = 00;
  seconds_alarm_snooze = 00;
  ampm_alarm_snooze = AM;  
  alarm_going = FALSE;
  snooze = FALSE;

  update_time_str();
  sei(); //Enable interrupts
  siren(); //Make some noise at power up

  while(1)
  {
    check_buttons(); //See if we need to set the time or snooze
    check_alarm(); //See if the current time is equal to the alarm time
  }
  return(0);
}

//Check to see if the time is equal to the alarm time
void check_alarm(void)
{
  //Check wether the alarm slide switch is on or off
  if( (PINB & (1<<BUT_ALARM)) != 0)
  {
    if (alarm_going == FALSE)
    {
      //Check to see if the time equals the alarm time
      if( (hours == hours_alarm) && (minutes == minutes_alarm) && \
          (seconds == seconds_alarm) && (ampm == ampm_alarm) && (snooze == FALSE) )
      {
        //Set it off!
        alarm_going = TRUE;
      }

      //Check to see if we need to set off the alarm again after a ~9 minute snooze
      if( (hours == hours_alarm_snooze) && (minutes == minutes_alarm_snooze) && \
          (seconds == seconds_alarm_snooze) && (ampm == ampm_alarm_snooze) && (snooze == TRUE) )
      {
        //Set it off!
        alarm_going = TRUE;
      }
    }
  }
  else
    alarm_going = FALSE;
}

//Checks buttons for system settings
void check_buttons(void)
{
  uint8_t i;
  uint8_t sling_shot = 0;
  uint8_t minute_change = 1;
  uint8_t previous_button = 0;
  
  //If the user hits snooze while alarm is going off, record time so that we can set off alarm again in 9 minutes
  if ( (PIND & (1<<BUT_SNOOZE)) == 0 && alarm_going == TRUE)
  {
    alarm_going = FALSE; //Turn off alarm
    snooze = TRUE; //But remember that we are in snooze mode, alarm needs to go off again in a few minutes
    
    seconds_alarm_snooze = 0;
    minutes_alarm_snooze = minutes + 9; //Snooze to 9 minutes from now
    hours_alarm_snooze = hours;
    ampm_alarm_snooze = ampm;
    
    if(minutes_alarm_snooze > 59)
    {
      minutes_alarm_snooze -= 60;
      hours_alarm_snooze++;

      if(hours_alarm_snooze == 12)
      {
        if(ampm_alarm_snooze == AM) 
          ampm_alarm_snooze = PM;
        else
          ampm_alarm_snooze = AM;
      }

      if(hours_alarm_snooze == 13) hours_alarm_snooze = 1;
    }
    
  }

  // toggle time display
  if ((PINB & (1<<BUT_DOWN)) == 0 && (PIND & (1<<BUT_SNOOZE)) == 0)
  {
    delay_ms(2000);

    if ((PINB & (1<<BUT_DOWN)) == 0 && (PIND & (1<<BUT_SNOOZE)) == 0)
    {
      show_time_str = (show_time_str == TRUE) ? FALSE : TRUE;
      while((PIND & (1<<BUT_SNOOZE)) == 0) ; //Wait for you to release button
    }
  }

  //Check for set time
  if ( (PINB & ((1<<BUT_UP)|(1<<BUT_DOWN))) == 0)
  {
    delay_ms(2000);

    if ( (PINB & ((1<<BUT_UP)|(1<<BUT_DOWN))) == 0)
    {
      //You've been holding up and down for 2 seconds
      //Set time!
      program_state = SET_TIME;
      //siren(); //Make some noise to show that you're setting the time

      while( (PINB & ((1<<BUT_UP)|(1<<BUT_DOWN))) == 0) //Wait for you to stop pressing the buttons
        display_time(1000); //Display current time for 1000ms

      while(1)
      {
        if ( (PIND & (1<<BUT_SNOOZE)) == 0) //All done!
        {
          TIMSK2 = (1<<TOIE2); //Re-enable the timer 2 interrupt
          for(i = 0 ; i < 3 ; i++)
          {
            display_time(250); //Display current time for 100ms
            clear_display();
            delay_ms(250);
          }
          
          while((PIND & (1<<BUT_SNOOZE)) == 0) ; //Wait for you to release button
          update_time_str();
          program_state = SHOW_TIME;
          break; 
        }

        if ( (PINB & (1<<BUT_UP)) == 0)
        {
          //Ramp minutes faster if we are holding the button
          //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
          if(previous_button == BUT_UP) 
            sling_shot++;
          else
          {  
            sling_shot = 0;
            minute_change = 1;
          }
            
          previous_button = BUT_UP;
          
          if (sling_shot > 5)
          {
            minute_change++;
            if(minute_change > 30) minute_change = 30;
            sling_shot = 0;
          }
          //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
          
          minutes += minute_change;
          if (minutes > 59)
          {
            minutes -= 60;
            hours++;

            if(hours == 13) hours = 1;

            if(hours == 12)
            {
              if(ampm == AM) 
                ampm = PM;
              else
                ampm = AM;
            }
          }
          delay_ms(100);
        }
        else if ( (PINB & (1<<BUT_DOWN)) == 0)
        {
          //Ramp minutes faster if we are holding the button
          //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
          if(previous_button == BUT_DOWN) 
            sling_shot++;
          else
          {
            sling_shot = 0;
            minute_change = 1;
          }
            
          previous_button = BUT_DOWN;
          
          if (sling_shot > 5)
          {
            minute_change++;
            if(minute_change > 30) minute_change = 30;
            sling_shot = 0;
          }
          //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


          minutes -= minute_change;
          if(minutes > 60)
          {
            minutes = 59;
            hours--;
            if(hours == 0) hours = 12;

            if(hours == 11)
            {
              if(ampm == AM) 
                ampm = PM;
              else
                ampm = AM;
            }
          }
          delay_ms(100);
        }
        else
        {
          previous_button = 0;
        }
        
        //clear_display(); //Blink display
        //delay_ms(100);
      }
    }
  }


  //Check for set alarm
  if ( (PIND & (1<<BUT_SNOOZE)) == 0)
  {
    TIMSK2 = 0;
    display_alarm_time(2000);

    if ( (PIND & (1<<BUT_SNOOZE)) == 0)
    {
      //You've been holding snooze for 2 seconds
      //Set alarm time!

      //Disable the regular display clock interrupt
      TIMSK2 = 0;

      while( (PIND & (1<<BUT_SNOOZE)) == 0) //Wait for you to stop pressing the buttons
      {
        clear_display();
        delay_ms(250);

        display_alarm_time(250); //Display current time for 1000ms
      }

      while(1)
      {
        display_alarm_time(100); //Display current time for 100ms
        
        if ( (PIND & (1<<BUT_SNOOZE)) == 0) //All done!
        {
          for(i = 0 ; i < 4 ; i++)
          {
            display_alarm_time(250); //Display current time for 100ms
            clear_display();
            delay_ms(250);
          }
          
          while((PIND & (1<<BUT_SNOOZE)) == 0) ; //Wait for you to release button
          
          TIMSK2 = (1<<TOIE2); //Re-enable the timer 2 interrupt
          
          break; 
        }

        if ( (PINB & (1<<BUT_UP)) == 0)
        {
          //Ramp minutes faster if we are holding the button
          //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
          if(previous_button == BUT_UP) 
            sling_shot++;
          else
          {  
            sling_shot = 0;
            minute_change = 1;
          }
            
          previous_button = BUT_UP;
          
          if (sling_shot > 5)
          {
            minute_change++;
            if(minute_change > 30) minute_change = 30;
            sling_shot = 0;
          }
          //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

          minutes_alarm += minute_change;
          if (minutes_alarm > 59)
          {
            minutes_alarm -= 60;
            hours_alarm++;
            if(hours_alarm == 13) hours_alarm = 1;

            if(hours_alarm == 12)
            {
              if(ampm_alarm == AM) 
                ampm_alarm = PM;
              else
                ampm_alarm = AM;
            }
          }
          //delay_ms(100);
        }
        else if ( (PINB & (1<<BUT_DOWN)) == 0)
        {
          //Ramp minutes faster if we are holding the button
          //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
          if(previous_button == BUT_DOWN) 
            sling_shot++;
          else
          {  
            sling_shot = 0;
            minute_change = 1;
          }
            
          previous_button = BUT_DOWN;
          
          if (sling_shot > 5)
          {
            minute_change++;
            if(minute_change > 30) minute_change = 30;
            sling_shot = 0;
          }
          //=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

          minutes_alarm -= minute_change;
          if(minutes_alarm > 60)
          {
            minutes_alarm = 59;
            hours_alarm--;
            if(hours_alarm == 0) hours_alarm = 12;

            if(hours_alarm == 11)
            {
              if(ampm_alarm == AM) 
                ampm_alarm = PM;
              else
                ampm_alarm = AM;
            }
          }
          //delay_ms(100);
        }
        else
        {
          previous_button = 0;
        }
        
        //clear_display(); //Blink display
        //delay_ms(100);
      }
    }
    else
      TIMSK2 = (1<<TOIE2); //Re-enable the timer 2 interrupt

  }

}

void clear_display(void)
{
  PORTC = 0;
  PORTD &= 0b10000000;
}

void display_number(uint8_t number, uint8_t digit)
{
  //Set the digit
  PORTD |= (1<<DIG_1)|(1<<DIG_2)|(1<<DIG_3)|(1<<DIG_4)|(1<<COL);
  
  switch(digit)
  {
    case 1:
      PORTD &= ~(1<<DIG_1);//Select Digit 1
      break;
    case 2:
      PORTD &= ~(1<<DIG_2);//Select Digit 2
      break;
    case 3:
      PORTD &= ~(1<<DIG_3);//Select Digit 3
      break;
    case 4:
      PORTD &= ~(1<<DIG_4);//Select Digit 4
      break;
    case 5:
      PORTD &= ~(1<<COL);//Select Digit COL
      break;
    default: 
      break;
  }
  
  PORTC = 0; //Clear all segments
  PORTD &= ~((1<<2)|(1<<5)); //Clear D segment and decimal point
  
  switch(number)
  {
    case 0:
      PORTC |= 0b00101111; //Segments A, B, C, D, E, F
      PORTD |= 0b00000100;
      break;
    case 1:
      PORTC |= 0b00100100; //Segments B, C
      //PORTD |= 0b00000000;
      break;
    case 2:
      PORTC |= 0b00111001; //Segments A, B, D, E, G
      PORTD |= 0b00000100;
      break;
    case 3:
      PORTC |= 0b00111100; //Segments ABCDG
      PORTD |= 0b00000100;
      break;
    case 4:
      PORTC |= 0b00110110; //Segments BCGF
      PORTD |= 0b00000000;
      break;
    case 5:
      PORTC |= 0b00011110; //Segments AFGCD
      PORTD |= 0b00000100;
      break;
    case 6:
      PORTC |= 0b00011111; //Segments AFGDCE
      PORTD |= 0b00000100;
      break;
    case 7:
      PORTC |= 0b00101100; //Segments ABC
      PORTD |= 0b00000000;
      break;
    case 8:
      PORTC |= 0b00111111; //Segments ABCDEFG
      PORTD |= 0b00000100;
      break;
    case 9:
      PORTC |= 0b00111110; //Segments ABCDFG
      PORTD |= 0b00000100;
      break;

    case 10:
      //Colon
      PORTC |= 0b00101000; //Segments AB
      //PORTD |= 0b00000000;
      break;

    case 11:
      //Alarm dot
      PORTD |= 0b00100000;
      break;

    case 12:
      //AM dot
      PORTC |= 0b00000100; //Segments C
      break;

    default: 
      PORTC = 0;
      break;
  } 
}

//Displays current time
//Brightness level is an amount of time the LEDs will be in - 200us is pretty dim but visible.
//Amount of time during display is around : [ BRIGHT_LEVEL(us) * 5 + 10ms ] * 10
//Roughly 11ms * 10 = 110ms
//Time on is in (ms)
void display_time(uint16_t time_on)
{
  for(uint16_t j = 0 ; j < time_on ; j++)
  {

#ifdef NORMAL_TIME
    //Display normal hh:mm time
    if(hours > 9)
    {
      display_number(hours / 10, 1); //Post to digit 1
      delay_us(bright_level);
    }

    display_number(hours % 10, 2); //Post to digit 2
    delay_us(bright_level);

    display_number(minutes / 10, 3); //Post to digit 3
    delay_us(bright_level);

    display_number(minutes % 10, 4); //Post to digit 4
    delay_us(bright_level);
#else
    //During debug, display mm:ss
    display_number(minutes / 10, 1); 
    delay_us(bright_level);

    display_number(minutes % 10, 2); 
    delay_us(bright_level);

    display_number(seconds / 10, 3); 
    delay_us(bright_level);

    display_number(seconds % 10, 4); 
    delay_us(bright_level);
#endif
    
    //Check whether it is AM or PM and turn on dot
    if(ampm == AM)
    {
      display_number(12, 5); //Turn on dot on digit 3
      delay_us(bright_level);
    }
    
    //Flash colon for each second
    if(flip == 0 && program_state == SHOW_TIME) 
    {
      display_number(255, 5); //Post to digit COL
    }
    else
    {
      display_number(10, 5); //Post to digit COL
    }
    delay_us(bright_level);
    
    //Indicate wether the alarm is on or off
    if( (PINB & (1<<BUT_ALARM)) != 0)
    {
      display_number(11, 4); //Turn on dot on digit 4
      delay_us(bright_level);

      //If the alarm slide is on, and alarm_going is true, make noise!
      if(alarm_going == TRUE && flip_alarm == 1)
      {
        clear_display();
        siren();
        flip_alarm = 0;
      }
    }
    else
    {
      snooze = FALSE; //If the alarm switch is turned off, this resets the ~9 minute addtional snooze timer
      
      hours_alarm_snooze = 88; //Set these values high, so that normal time cannot hit the snooze time accidentally
      minutes_alarm_snooze = 88;
      seconds_alarm_snooze = 88;
    }

    clear_display();
    delay_us(bright_level);
  }
}

//Displays current alarm time
//Brightness level is an amount of time the LEDs will be in - 200us is pretty dim but visible.
//Amount of time during display is around : [ BRIGHT_LEVEL(us) * 5 + 10ms ] * 10
//Roughly 11ms * 10 = 110ms
//Time on is in (ms)
void display_alarm_time(uint16_t time_on)
{
  for(uint16_t j = 0 ; j < time_on ; j++)
  {
    //Display normal hh:mm time
    if(hours_alarm > 9)
    {
      display_number(hours_alarm / 10, 1); //Post to digit 1
      delay_us(bright_level);
    }

    display_number(hours_alarm % 10, 2); //Post to digit 2
    delay_us(bright_level);

    display_number(minutes_alarm / 10, 3); //Post to digit 3
    delay_us(bright_level);

    display_number(minutes_alarm % 10, 4); //Post to digit 4
    delay_us(bright_level);

    
    //During debug, display mm:ss
    /*display_number(minutes_alarm / 10, 1); 
    delay_us(bright_level);

    display_number(minutes_alarm % 10, 2); 
    delay_us(bright_level);

    display_number(seconds_alarm / 10, 3); 
    delay_us(bright_level);

    display_number(seconds_alarm % 10, 4); 
    delay_us(bright_level);

    display_number(10, 5); //Display colon
    delay_us(bright_level);*/


    //Check whether it is AM or PM and turn on dot
    if(ampm_alarm == AM)
    {
      display_number(12, 5); //Turn on dot on digit 3
      delay_us(bright_level);
    }    

    display_number(10, 5); //Post to digit COL
    delay_us(bright_level);

    clear_display();
    delay_ms(1);
  }
  
}

void display_time_str(uint16_t time_on)
{  
  for(uint16_t j = 0 ; j < time_on ; j++)
  {
    display_character(time_str[time_str_display_index], 1);
    delay_us(bright_level);

    display_character(time_str[time_str_display_index + 1], 2);
    delay_us(bright_level);

    display_character(time_str[time_str_display_index + 2], 3);
    delay_us(bright_level);
    
    display_character(time_str[time_str_display_index + 3], 4);
    delay_us(bright_level);
    
    //Indicate wether the alarm is on or off
    if( (PINB & (1<<BUT_ALARM)) != 0)
    {
      display_number(11, 4); //Turn on dot on digit 4
      delay_us(bright_level);

      //If the alarm slide is on, and alarm_going is true, make noise!
      if(alarm_going == TRUE && flip_alarm == 1)
      {
        clear_display();
        siren();
        flip_alarm = 0;
      }
    }
    else
    {
      snooze = FALSE; //If the alarm switch is turned off, this resets the ~9 minute addtional snooze timer
      hours_alarm_snooze = 88; //Set these values high, so that normal time cannot hit the snooze time accidentally
      minutes_alarm_snooze = 88;
      seconds_alarm_snooze = 88;
    }

    clear_display();
    delay_us(bright_level);
  }
}

void display_character(uint8_t character, uint8_t position)
{
  //Set the digit
  PORTD |= (1<<DIG_1)|(1<<DIG_2)|(1<<DIG_3)|(1<<DIG_4)|(1<<COL);
  
  switch(position)
  {
    case 1:
      PORTD &= ~(1<<DIG_1);//Select Digit 1
      break;
    case 2:
      PORTD &= ~(1<<DIG_2);//Select Digit 2
      break;
    case 3:
      PORTD &= ~(1<<DIG_3);//Select Digit 3
      break;
    case 4:
      PORTD &= ~(1<<DIG_4);//Select Digit 4
      break;
    case 5:
      PORTD &= ~(1<<COL);//Select Digit COL
      break;
    default: 
      break;
  }
  
  PORTC = 0; //Clear all segments
  PORTD &= ~((1<<2)|(1<<5)); //Clear D segment and decimal point

  char* base_address;
  if (character >= 'A' && character <= 'Z') {
    base_address = CHARACTERS - 'A';
  } else if (character >= 'a' && character <= 'z') {
    base_address = CHARACTERS - 'a';
  } else if (character >= '0' && character <= '9') {
    base_address = DIGITS - '0';
  } else if (character == '-') {
    base_address = PUNCTUATION - character;
  } else if (character == '\'') {
    base_address = PUNCTUATION - character + 1;
  } else {
    return;
  }

  uint8_t character_bitmap = pgm_read_byte(base_address + character);

  PORTC |= character_bitmap;
  if (character_bitmap >> 7) {
    PORTD |= 0b00000100;
  } 
}

//Make noise for time_on in (ms)
void siren(void)
{

  for(int i = 0 ; i < 500 ; i++)
  {
    cbi(PORTB, BUZZ1);
    sbi(PORTB, BUZZ2);
    delay_us(300);

    sbi(PORTB, BUZZ1);
    cbi(PORTB, BUZZ2);
    delay_us(300);
  }

  cbi(PORTB, BUZZ1);
  cbi(PORTB, BUZZ2);

  delay_ms(50);
  
  for(int i = 0 ; i < 500 ; i++)
  {
    cbi(PORTB, BUZZ1);
    sbi(PORTB, BUZZ2);
    delay_us(300);

    sbi(PORTB, BUZZ1);
    cbi(PORTB, BUZZ2);
    delay_us(300);
  }

  cbi(PORTB, BUZZ1);
  cbi(PORTB, BUZZ2);
}

void ioinit(void)
{
  //1 = output, 0 = input 
  DDRB = 0b11111111 & ~((1<<BUT_UP)|(1<<BUT_DOWN)|(1<<BUT_ALARM)); //Up, Down, Alarm switch  
  DDRC = 0b11111111;
  DDRD = 0b11111111 & ~(1<<BUT_SNOOZE); //Snooze button
  
  PORTB = (1<<BUT_UP)|(1<<BUT_DOWN)|(1<<BUT_ALARM); //Enable pull-ups on these pins
  PORTD = (1<<BUT_SNOOZE); //Enable pull-up on snooze button
  PORTC = 0;

  //Init Timer0 for delay_us
  TCCR0B = (1<<CS01); //Set Prescaler to clk/8 : 1click = 0.5us(assume we are running at external 16MHz). CS01=1 
  
  //Init Timer1 for second counting
  TCCR1B = (1<<CS12)|(1<<CS10); //Set prescaler to clk/1024 :1click = 64us (assume we are running at 16MHz)
  TIMSK1 = (1<<TOIE1); //Enable overflow interrupts
  TCNT1 = 49911; //65536 - 15,625 = 49,911 - Preload timer 1 for 49,911 clicks. Should be 1s per ISR call
  
  //Init Timer2 for updating the display via interrupts
  TCCR2B = (1<<CS22)|(1<<CS21)|(1<<CS20); //Set prescalar to clk/1024 : 1 click = 64us (assume 16MHz)
  TIMSK2 = (1<<TOIE2); //TCNT2 should overflow every 16.384 ms (256 * 64us)
  
}

//General short delays
void delay_ms(uint16_t x)
{
  for (; x > 0 ; x--)
  {
    delay_us(250);
    delay_us(250);
    delay_us(250);
    delay_us(250);
  }
}

//General short delays
void delay_us(uint16_t x)
{
  x *= 2; //Correction for 16MHz
  
  while(x > 256)
  {
    TIFR0 = (1<<TOV0); //Clear any interrupt flags on Timer2
    TCNT0 = 0; //256 - 125 = 131 : Preload timer 2 for x clicks. Should be 1us per click
    while( (TIFR0 & (1<<TOV0)) == 0);
    
    x -= 256;
  }

  TIFR0 = (1<<TOV0); //Clear any interrupt flags on Timer2
  TCNT0 = 256 - x; //256 - 125 = 131 : Preload timer 2 for x clicks. Should be 1us per click
  while( (TIFR0 & (1<<TOV0)) == 0);
}
