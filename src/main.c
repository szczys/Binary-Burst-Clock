/*******************************
* Binary Burst                                  *
*                                                            *
*				*
* http://jumptuck.com 		*
*				*
********************************/

//Includes
#include <avr/io.h>
#include <util/delay.h>

//Clock Speed
#define F_CPU 1000000		//Running at 1 MHz (this will probably change when multiplexing)


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

int main(void)
{
  init_IO();
  
  while(1) {
    testPattern();
  }
}
