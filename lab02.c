/* (C) 2005 by
 * Richard West (richard@dopplereffect.us)
 * Eric Mesa (djotaku1282@yahoo.com)
 * Lab 02 - Cricket Call Generator
 * Lab Date: 23 Feb 2005 and 26 Feb 2005
 * Published: 14 March 2005
 * Published under GNU General Public License - http://www.linux.org/info/gnu.html
 * http://www.ericsbinaryworld.com
 *
 *
 * Hardware setup notes:
 *		Atmel Mega32 running on STK500
 *		Keypad connected to PortA.0-7
 *		DDS connected to PortB.3(OC0)
 *		LCD connected to PortC.0-7
 *		pushbuttons connected to PortD.0-7
 *		
 *	This program also used the keypad.c and
 *	keypad.h files as libraries to control
 *	the keypad.  They can be found in the
 *	same place as this file or should
 *	have been packaged along with it
 *	if you did not download this file.
 */

// Mega32 compiler directive
#include <Mega32.h>

// LCD compiler directives
#asm(".equ __lcd_port=0x15")
#include <lcd.h>

// useful C library
#include <math.h> // for sin()
#include <stdio.h> // for sprintf()
#include <stdlib.h> // for atoi()
#include <string.h> // for strcpyf()

// keypad compiler direcive
#include "keypad.h"

// operation modes
#define STOP 0x00
#define PLAY 0x01

// command modes
#define CMD_PENDING 0x00
#define CMD_WAIT 0x01
#define CMD_VIEW 0x02
#define CMD_DONE 0x03

// runtime global variables
flash unsigned char* param_names[5] =
	{"Frequency\0",
	 "Chrp-Sci. Dur.\0",
	 "# Syllables\0",
	 "Syl Duration\0",
	 "Syl. Rep. Int.\0"};
flash unsigned int call_params[4][5] =
	{{4500, 20, 1, 10, 0}, // call 0
	 {5000, 250, 4, 18, 30}, // call 1
	 {6000, 1500, 50, 8, 20}, // call 2
	 {3000, 1000, 5, 30, 50}}; // call 3
unsigned int user_params[5];
unsigned int dds_accumulator @ 0x2f0;
unsigned char dds_index @ 0x2f1;
unsigned int dds_time;
unsigned int dds_increment;
unsigned int dds_num_syl;
unsigned int dds_duration;
unsigned int dds_sri;
unsigned int dds_csd;
unsigned char mode;
unsigned char count;
unsigned char param_count;
unsigned char time;
unsigned char keypad_time;
unsigned char cmd_mode;
unsigned char dds_amplitude;
unsigned char dds_syl_count;
unsigned char lcd_buffer[16];
unsigned char dds_sineTable[256] @ 0x300;

// function prototype
void compute_sine_table(void);
void init(void);
void cmd_release(void);

// timer0 overflow interrupt service routine
interrupt [TIM0_OVF] void tim0_ovf_isr(void)
{
	// decrement and check count
	if(!(--count))
	{
		// increment global time and reset count
		count = 62 + (++time%2);

		// increment appropriate time
		if (mode == PLAY)
		{
			dds_time++;
		}
		else
		{
			keypad_time++;
		}
	}

	// check mode
	if (mode == PLAY)
	{
		// update DDS output
		dds_accumulator += dds_increment;
		OCR0 = dds_sineTable[dds_index] >> dds_amplitude;
	}
	// else mode == STOP so check keypad_time
	else if (keypad_time == 30)
	{
		// reset keypad_time
		keypad_time = 0x00;

		// get keystring
		keypad_get_string();
	}
}

void main(void)
{
	// start in STOP mode
	mode = STOP;

	// map keypad buttons
	keypad_define_terminator('#');
	keypad_map_keysymbol('*','.');

	compute_sine_table(); // compute sine table
	lcd_init(16); // initialize LCD
	init(); // initialize MCU

	// quick welcome message
	lcd_clear();
	lcd_gotoxy(0,0);
	lcd_putsf("welcome#");

	cmd_release(); // release the command

	while(1)
	{
		if (mode == PLAY)
		{
			// ramp up syllable amplitude
			if (dds_time == 1) dds_amplitude = 0x03;
			if (dds_time == 2) dds_amplitude = 0x02;
			if (dds_time == 3) dds_amplitude = 0x01; // syllable duration start
			if (dds_time == 4) dds_amplitude = 0x00;

			// ramp down syllable amplitude
			if (dds_time == (2+dds_duration)) dds_amplitude = 0x01; // syllable duration end
			if (dds_time == (3+dds_duration)) dds_amplitude = 0x02;
			if (dds_time == (4+dds_duration)) dds_amplitude = 0x03;
			if (dds_time == (5+dds_duration)) dds_amplitude = 0x04;

			// stop syllable
			if (dds_time == (6+dds_duration)) TCCR0 = 0x01;

			// wait syllable repeat interval
			// skip if no SRI is specified
			if (dds_time == dds_sri && dds_sri != 0)
			{
				// increment syllable count
				dds_syl_count++;

				if (dds_syl_count < dds_num_syl)
				{
					// reset amplitude scaling for division by 16
					dds_amplitude = 0x04;

					// phase lock the sine generator and timer
					dds_time = 0x00;
					dds_accumulator = 0x00;
					TCNT0 = 0x00; 
					OCR0 = 0x00;

					// reset timer0 to fast pwm
					TCCR0 = 0x69;
				}
			}

			// wait chirp-scilence durration
			if (dds_time == dds_csd)
			{
				// reset syllable count
				dds_syl_count = 0x00;

				// reset amplitude scaling for division by 16
				dds_amplitude = 0x04;

				// phase lock the sine generator and timer
				dds_time = 0x00;
				dds_accumulator = 0x00;
				TCNT0 = 0x00; 
				OCR0 = 0x00;

				// reset timer0 to fast pwm
				TCCR0 = 0x69;
			}

			// poll button0 for stop
			if (!(PIND & 0x01))
			{
				TCCR0 = 0x01;
				mode = STOP;

				lcd_clear();
				lcd_gotoxy(0,0);
				lcd_putsf("Stop");

				cmd_release();
			}
		}
		else
		{
			if (cmd_mode == CMD_PENDING)
			{
				if (keystring_ready)
				{
					// extract first character
					unsigned char c;
					c = keystring[0];

					// reset the lcd
					lcd_clear();
					lcd_gotoxy(0,0);

					// check if predefined call
					if (c == 'A')
					{
						// extract second character
						c = keystring[1];

						if ((c >= '0') && (c <= '3'))
						{
							unsigned char i;

							// convert ascii to decimal number
							c -= 0x30;

							// copy predefined params into user params
							for (i = 0; i < 5; i++)
							{
								user_params[i] = call_params[c][i];
							}

							// display call selected
							sprintf(lcd_buffer, "Call %u", c);
							lcd_puts(lcd_buffer);
							lcd_gotoxy(0,1);
							lcd_putsf("Press play");

							// switch to done mode
							cmd_mode = CMD_DONE;
						}
						// else invalid call
						else
						{
							lcd_putsf("Invalid");
						}
					}
					// check if user defined call
					else if (c == 'B')
					{
						// display parameter name
						strcpyf(lcd_buffer, param_names[param_count]);
						lcd_puts(lcd_buffer);

						// display current value
						sprintf(lcd_buffer, "%u", user_params[param_count]);
						lcd_gotoxy(0,1);
						lcd_puts(lcd_buffer);

						// indicate that value is current
						lcd_gotoxy(13,1);
						lcd_putsf("CUR");

						// switch to wait mode
						cmd_mode = CMD_WAIT;
					}
					// else invalid command
					else
					{
						lcd_putsf("Invalid");
					}

					// release the keypad
					keypad_release();
				}
			}
			else if (cmd_mode == CMD_WAIT)
			{
				if (keystring_ready)
				{
					// check to see if user has changed parameter
					if (keystring[0] != '\0')
					{
						// convert and store param
						user_params[param_count] = atoi(keystring);
					}

					// display new value
					sprintf(lcd_buffer, "%u", user_params[param_count]);
					lcd_gotoxy(0,1);
					lcd_puts(lcd_buffer);

					// indicate that the value is okay
					lcd_gotoxy(13,1);
					lcd_putsf("OK#");

					// increment param_count
					param_count++;

					// switch to view mode
					cmd_mode = CMD_VIEW;

					// release the keypad
					keypad_release();
				}
			}
			else if (cmd_mode == CMD_VIEW)
			{
				if (keystring_ready)
				{
					if (param_count > 4)
					{
						param_count = 0x00;
					}
					// reset the lcd
					lcd_clear();
					lcd_gotoxy(0,0);

					// display parameter name
					strcpyf(lcd_buffer, param_names[param_count]);
					lcd_puts(lcd_buffer);

					// display current value
					sprintf(lcd_buffer, "%u", user_params[param_count]);
					lcd_gotoxy(0,1);
					lcd_puts(lcd_buffer);

					// indicate that value is current
					lcd_gotoxy(13,1);
					lcd_putsf("CUR");

					// switch to wait mode
					cmd_mode = CMD_WAIT;

					// release the keypad
					keypad_release();
				}
			}

			// poll button1 for play
			if (!(PIND & 0x02))
			{
				lcd_clear();
				lcd_gotoxy(0,0);
				lcd_putsf("playing");

				// compute new DDS values
				dds_increment = (int)(1.048576*(float)user_params[0]);
				dds_num_syl = user_params[2];
				dds_duration = user_params[3];
				dds_sri = user_params[4];
				dds_csd = (user_params[1] - ((dds_sri * dds_num_syl) - (dds_sri - dds_duration - 6)));
				dds_syl_count = 0x00;

				// reset amplitude scaling for division by 16
				dds_amplitude = 0x04;

				// phase lock the sine generator and timer
				dds_time = 0x00;
				dds_accumulator = 0x00;
				TCNT0 = 0x00; 
				OCR0 = 0x00;

				// reset timer0 to fast pwm
				TCCR0 = 0x69;
				mode = PLAY;
			}
		}
	}
}

void compute_sine_table(void)
{
	unsigned char i = 0x00;
	do
	{
		dds_sineTable[i] = 128 + (char)(127.0*sin(6.283*((float)i)/256.0));
	} while(++i);
}

void init(void)
{
	// initialize ports
	DDRB = 0xFF; // set portB as output

	// setup timer0
	// 16 MHz system clock
	// prescaler = 1 -> 16 MHz timer
	TIMSK = 0x01; // enable the timer0 overflow interrupt
	TCCR0 = 0x01; // set timer0 to system clock

	// enable interrupts
	#asm("sei")
}

void cmd_release(void)
{
	// start command state machine
	param_count = 0x00;
	cmd_mode = CMD_PENDING;

	// release the keypad
	keypad_release();
}
