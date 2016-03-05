
#include <system.h>
#include <memory.h>

// 8MHz internal oscillator block, reset disabled
#pragma DATA _CONFIG1, _FOSC_INTOSC & _WDTE_OFF & _MCLRE_OFF &_CLKOUTEN_OFF
#pragma DATA _CONFIG2, _WRT_OFF & _PLLEN_OFF & _STVREN_ON & _BORV_19 & _LVP_OFF
#pragma CLOCK_FREQ 8000000
typedef unsigned char byte;

#define P_LED		lata.0
#define P_OUTPUT	lata.2
#define P_SENSE		porta.5

// timer stuff
volatile byte msTick = 0; 

#define TIMER_0_INIT_SCALAR		5

// This value (in milliseconds) determines how long the sound trigger
// input must be LOW before we consider there to be no sound present
#define SENSE_TIMEOUT			200

// This value is the minimum length of an output pulse to the relay
// in milliseconds
#define OUTPUT_TIMEOUT		100

enum {
	MODE_PULSE,
	MODE_HOLD,
	MODE_TOGGLE,
};

#define SEL_UPPER   150
#define SEL_MID 	50

////////////////////////////////////////////////////////////
// INTERRUPT HANDLER CALLED WHEN CHARACTER RECEIVED AT 
// SERIAL PORT OR WHEN TIMER 1 OVERLOWS
void interrupt( void )
{
	if(intcon.2)
	{
		tmr0 = TIMER_0_INIT_SCALAR;
		msTick = 1;
		intcon.2 = 0;
	}
}

////////////////////////////////////////////////////////////
// ENTRY POINT
void main()
{ 
	// osc control / 8MHz / internal
	osccon = 0b01110010;
	
	trisa = 0b00100010;              
		
	ansela = 0b00000010;
	porta=0;
	
	adcon0 = 0b00000101;
	adcon1 = 0b01100000;
	
	// Configure timer 0 (controls systemticks)
	// 	timer 0 runs at 2MHz
	// 	prescaled 1/8 = 250kHz
	// 	rollover at 250 = 1kHz
	// 	1ms per rollover	
	//tmr0 = TIMER_0_INIT_SCALAR;	
	//cpscon0.0 = 0;
	option_reg.5 = 0; // timer 0 driven from instruction cycle clock
	option_reg.3 = 0; // timer 0 is prescaled
	option_reg.2 = 0; // }
	option_reg.1 = 1; // } 1/8 prescaler
	option_reg.0 = 0; // }
	intcon.5 = 1; 	  // enabled timer 0 interrrupt
	intcon.2 = 0;     // clear interrupt fired flag
	
	// enable interrupts	
	intcon.7 = 1; //GIE
	intcon.6 = 1; //PEIE	

	// startup flash
	P_LED=1; delay_ms(200);
	P_LED=0; delay_ms(200);
	P_LED=1; delay_ms(200);
	P_LED=0; delay_ms(200);
	P_LED=1; delay_ms(200);
	P_LED=0; delay_ms(200);


	int senseTimeout = 0;
	int switchMode = MODE_TOGGLE;
	
	
	int outputStatus = 0;
	int outputTimeout = 0;
	
	
	byte isSoundPresent = 0;
	for(;;) {
		byte tick = msTick;
		msTick = 0;

		if(!adcon0.1) {	
			
					if(adresh > SEL_UPPER) {
						switchMode = MODE_HOLD;
					}
					else if(adresh > SEL_MID) {
						switchMode = MODE_TOGGLE;
					}
					else {
						switchMode = MODE_PULSE;
					}
			adcon0.1 = 1;
		}

		byte wasSoundPresent = isSoundPresent;
		
		// First step is to determine whether sound is present.. we apply
		// a timeout to the most recent input trigger detected 
		if(P_SENSE) {
			isSoundPresent = 1;
			senseTimeout = SENSE_TIMEOUT;
		}
		else if(tick && senseTimeout) {
			--senseTimeout;
			if(!senseTimeout) {
				isSoundPresent = 0;
			}
		}

		switch(switchMode) {			
		
			// HOLD MODE
			case MODE_HOLD:	
				if(isSoundPresent) {				
					outputStatus = 1;
					outputTimeout = OUTPUT_TIMEOUT;
				}
				else if(outputTimeout && tick) {
					outputStatus = 1;
					--outputTimeout;
				}
				if(!outputTimeout) {
					outputStatus = 0;
				}
				break;
			
			// PULSE MODE		
			case MODE_PULSE:						
				if(isSoundPresent && !wasSoundPresent) {
					outputStatus = 1;
					outputTimeout = OUTPUT_TIMEOUT;
				}
				else if(outputTimeout && tick) {
					outputStatus = 1;
					--outputTimeout;
				}
				if(!outputTimeout) {
					outputStatus = 0;
				}
				break;
					
			// TOGGLE MODE
			case MODE_TOGGLE:
				if(isSoundPresent && !wasSoundPresent) {
					outputStatus = !outputStatus;
					outputTimeout = OUTPUT_TIMEOUT;
				}
				break;
		}
		
		P_LED = outputStatus;
		P_OUTPUT = outputStatus;
	}

}