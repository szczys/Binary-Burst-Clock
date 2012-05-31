
/*******************************
* Binary Burst 
* 
*
* http://jumptuck.com 
*
********************************/

//Clock Speed
#define F_CPU 8000000		//Running at 1 MHz (this will probably change when multiplexing)

//Includes
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "i2cmaster.h"

//Shift resgister assignments
#define SHIFTREGISTER DDRA
#define SHIFTPORT PORTA
#define DATA (1<<PA1)
#define CLOCK (1<<PA2)
#define LATCH (1<<PA3)

//High-side transistor assignments
#define BLUE_REGISTER DDRB
#define BLUE_PORT PORTB
#define BLUE0 (1<<PB0)
#define BLUE1 (1<<PB1)
#define BLUE2 (1<<PB2)
#define RED_REGISTER DDRA
#define RED_PORT PORTA
#define RED0 (1<<PA0)

//Multiplexing
#define TIM0_PREWIND 255-25 	//5 kHz (8 MHz / 64 (prescaler / 25 ticks per overflow)

//I2C Bus
#define RTC_ADDR  0xDE	// For MCP79410 RTC Chip
#define MESSAGEBUF_SIZE       8

//Variables
volatile unsigned int buffer[4] = { 0x0FF0, 0x00F0, 0x000F, 0x0F0F };
unsigned char messageBuf[MESSAGEBUF_SIZE];      //Used for I2C bus

//Prototypes
void init_IO(void);
void shiftByte(unsigned char byte);
void delay_ms(int cnt);
void delay_ms(int cnt);
void testPattern(void);

/*--------------------------------------------------------------------------
  FUNC: 3/4/12 - Initialize Input/Output Pins
  PARAMS: None
  RETURNS: None
--------------------------------------------------------------------------*/
void init_IO(void){
  //Set transistor pins as outputs
  BLUE_REGISTER |= BLUE0 | BLUE1 | BLUE2;
  RED_REGISTER |= RED0;

  //Set transistor pins high
  BLUE_PORT |= BLUE0 | BLUE1 | BLUE2;
  RED_PORT |= RED0;
  
  //Set up shift registers
  SHIFTREGISTER |= (DATA | LATCH | CLOCK);	//Set outputs
  SHIFTPORT &= ~(DATA | LATCH | CLOCK);	//Set pins low
}

/*--------------------------------------------------------------------------
  FUNC: 3/5/12 - Initialize Timers
  PARAMS: None
  RETURNS: None
--------------------------------------------------------------------------*/
void init_Timers(void){
  /*500 Hz for multiplexing:
        At 8Mhz with prescler of 64: 250 ticks per interrupt
  */
  cli();							//clear global interrupts
  TCNT0 = TIM0_PREWIND;						//Prewind timer0 counter
  TIMSK0 = 1<<TOIE0;				//Enable timer0 overflow interrupt
  TCCR0B = (1<<CS01) | (1<<CS00);		//Timer0 prescaler of 64
  sei();        //set global interrupts
}

/*--------------------------------------------------------------------------
  FUNC: 3/4/12 - Transmit 8-bits ot a shift register
  PARAMS: byte of data (big-endian)
  RETURNS: None
--------------------------------------------------------------------------*/
void shiftByte(unsigned char byte) {
  for (unsigned char i=0x80; i; i>>=1){
    if (i & byte) SHIFTPORT |= DATA;
    else SHIFTPORT &= ~DATA;
    SHIFTPORT |= CLOCK;
    SHIFTPORT &= ~CLOCK;
  }
}

/*--------------------------------------------------------------------------
  FUNC: 3/4/12 - Basic delay function
  PARAMS: None
  RETURNS: None
  NOTES: Must define F_CPU and include 
    /util/delay.h for this to work.
--------------------------------------------------------------------------*/
void delay_ms(int cnt){
  while(cnt-->0) _delay_ms(1);
}

/*--------------------------------------------------------------------------
  FUNC: 3/4/12 - Turn off all transistors
  PARAMS: None
  RETURNS: None
--------------------------------------------------------------------------*/
void all_off(void){
  BLUE_PORT |= BLUE0 | BLUE1 | BLUE2;
  RED_PORT |= RED0;
}


/*--------------------------------------------------------------------------
  FUNC: 3/4/12 - Test all LEDs
  PARAMS: None
  RETURNS: None
  NOTES: Non-multiplexing test pattern to ensure
      all of the LEDs are functioning correctly. This
      function should be run with interrupts disabled.
--------------------------------------------------------------------------*/
void testPattern(void){
  static unsigned int testVar = 0x0001;         //Instantiate a persistent tracking buffer
  shiftByte((char)(testVar>>8));			//Shift in high byte
  shiftByte((char)(testVar));                   		//Shift in low byte
  SHIFTPORT |= LATCH;					//Latch shifted data
  SHIFTPORT &= ~LATCH;

  //Light up each LED one at a time
  BLUE_PORT &= ~BLUE0;
  delay_ms(300);
  all_off();
  BLUE_PORT &= ~BLUE1;
  delay_ms(300);
  all_off();
  RED_PORT &= ~RED0;  
  delay_ms(300);
  all_off();
  BLUE_PORT &= ~BLUE2;
  delay_ms(300);
  all_off();
  
  //Move buffer to test next spire.
  if  (testVar == 0x0800 ) testVar = 0x0001;
  else testVar <<= 1;
}

/*--------------------------------------------------------------------------
  FUNC: 3/6/12 - Load display buffer with time data
  PARAMS: Hours (1-12), Minutess  (0-59)
  RETURNS: None
--------------------------------------------------------------------------*/
void show_binary_time(unsigned char hours, unsigned char minutes){

  unsigned char min_fives = minutes/5;
  unsigned char min_singles = minutes%5; 
  unsigned int new_blue0 = 0x0000;
  unsigned int new_blue1 = 0x0000;
  unsigned int new_blue2 = 0x0000;
  
  unsigned char singles_loc;
  if (min_fives == 11) singles_loc = 0;
  else singles_loc = min_fives+1;
    switch(min_singles) {
    case 0:
      break;
    case 1:
      new_blue0 |=  1<<(singles_loc);
      break;
    case 2:
      new_blue1 |= 1<<(singles_loc);
      break;
    case 3:
      new_blue0 |= 1<<(singles_loc);
      new_blue1 |= 1<<(singles_loc);
      break;
    case 4:
      new_blue2 |= 1<<(singles_loc);
  }
  
  while (min_fives--) {
    new_blue0 |= 1<<(min_fives+1);
    new_blue2 |= 1<<(min_fives+1);
  }
  
  //Load minutes into three BLUE led buffers
  buffer[0] = new_blue0;
  buffer[1] = new_blue1;
  buffer[2] = new_blue2;
  
  //Load hours into buffer for RED leds
  if (hours > 11) hours -= 12;        //Adjust for 24 hour clock
  buffer[3] = 1<<hours;
}

/*--------------------------------------------------------------------------
  FUNC: 3/7/12 - Animate Blue LED 'Hand'
  PARAMS: Number of Revoutions, Millisecond delay between shifts
  RETURNS: None
--------------------------------------------------------------------------*/
void blue_sweep(unsigned char revs, unsigned int delay){
  buffer[3] = 0x0000;	//Turn off all red LEDs
  while (revs--) {
    for (unsigned char i=0; i<12; i++) {
      for (unsigned char j=0; j<3; j++) {
	buffer[j] = 1<<i;
      }
      delay_ms(delay);
    }
  }  
}

/*--------------------------------------------------------------------------
  FUNC: 3/7/12 - Animate Red LED 'Hand'
  PARAMS: Number of Revoutions, Millisecond delay between shifts
  RETURNS: None
--------------------------------------------------------------------------*/
void red_sweep(unsigned char revs, unsigned int delay){
  //Turn off all blue LEDs
  buffer[0] = 0x0000;
  buffer[1] = 0x0000;
  buffer[2] = 0x0000;
  while (revs--) {
    for (unsigned char i=0; i<12; i++) {
      buffer[3] = 1<<i;
      delay_ms(delay);
    }
  }  
}

/*--------------------------------------------------------------------------
  FUNC: 3/17/12 - Convert RTC time to decimal
  PARAMS: bcd - tens in upper byte, ones in lower byte
  RETURNS: Decimal value
--------------------------------------------------------------------------*/
unsigned int rtc_to_dec(unsigned char bcd){
  return (bcd & 0x0F) + (bcd>>4)*10;
}


int main(void)
{
  init_IO();
  init_Timers();
  
  //Preload first set of display data
  shiftByte((char)(buffer[0]>>8));			//Shift in high byte
  shiftByte((char)(buffer[0]));                   		//Shift in low byte
  SHIFTPORT |= LATCH;					//Latch shifted data
  //Don't latch, that will happen on the next interrupt
  
  blue_sweep(3,20);
  red_sweep(5,12);
  blue_sweep(1,50);
  
  //Setup the  RTC
  //Initialize the I2C Master
  i2c_init();
  unsigned char read_seconds;
  unsigned char read_minutes;
  unsigned char read_hours;
  unsigned char clock_ticks;
  
  //Read timer from RTC
  //Send the register address we want to read from
  
  unsigned char return_value;
  return_value = i2c_start(RTC_ADDR+I2C_WRITE);
  if (return_value)
  {
    //Error occurred: display 5:55 as an error message
    i2c_stop();
    show_binary_time(5,55);
    //TODO: What should happen if there's an error?
    while(1) { } //trap after error.
  }
  else
  {
    //RTC communications successful. Read in current time
    i2c_write(0x00);
    i2c_rep_start(RTC_ADDR+I2C_READ);
    read_seconds = i2c_readAck();
    read_minutes = i2c_readAck();
    read_hours = i2c_readNak();
    i2c_stop();
    
    show_binary_time(rtc_to_dec(read_hours),rtc_to_dec(read_minutes));
    
    clock_ticks = rtc_to_dec(read_seconds & 0x7F);       //Sync seconds with RTC
  }
  
  while(1) {
    //The following is a hack to keep the displayed time up-to-date. It delays 1 seconds at a time, for roughly 60 seconds.
    //At the 30 second mark it pulls in the time from the RTC, displaying it and resyncing the seconds timer with the RTC's
    //precision clock.
    
    //TODO: Use a timer instead of a blocking function for synchronization
    //TODO: Come up with a better method of syncing time with the RTC.
    
    delay_ms(1000);     //Wait 1 second
    ++clock_ticks;
    
    switch (clock_ticks) {
      case 30:
        //Read in time from RTC
        i2c_start(RTC_ADDR+I2C_WRITE);
        i2c_write(0x00);
        i2c_rep_start(RTC_ADDR+I2C_READ);
        read_seconds = i2c_readAck();
        read_minutes = i2c_readAck();
        read_hours = i2c_readNak();
        i2c_stop();
    
        show_binary_time(rtc_to_dec(read_hours),rtc_to_dec(read_minutes));
    
        clock_ticks = rtc_to_dec(read_seconds & 0x7F);       //Sync seconds with RTC
        break;
      case 60:
        clock_ticks = 0;
        break;
    }
  }
}

ISR (TIM0_OVF_vect) {
  TCNT0 = TIM0_PREWIND;
  
  static unsigned char tracker = 0;
    
  all_off();				//Turn off all high-side transistors
  //Shift in data for next interupt
  shiftByte((char)(buffer[tracker]>>8));			//Shift in high byte
  shiftByte((char)(buffer[tracker]));                   		//Shift in low byte
  SHIFTPORT |= LATCH;					//Latch shifted data
  SHIFTPORT &= ~LATCH;	//Latch previously shifted data
  
  switch (tracker) {
    case 0:
      BLUE_PORT &= ~BLUE0;
      break;
    case 1:
      BLUE_PORT &= ~BLUE1;
      break;
    case 2:
      BLUE_PORT &= ~BLUE2;
      break;
    case 3:
      RED_PORT &= ~RED0;
      break;
  }
  
  if (++tracker > 7) tracker = 0;
  

  

  //Don't latch, that will happen on the next interrupt
  
  
}
