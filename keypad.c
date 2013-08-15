/* (C) 2005 by
 * Richard West (richard@dopplereffect.us)
 * Lab Date: 23 Feb 2005 and 26 Feb 2005
 * Published: 14 March 2005
 * Published under GNU General Public License - http://www.linux.org/info/gnu.html
 * http://www.ericsbinaryworld.com
 *
 * Keypad Function Definition File
 * (see keypad.h for documentation)
 *
 * Hardware setup notes:
 *		Keypad connected to PortA.0-7
 */

#ifndef _KEYPAD_C_
#define _KEYPAD_C_
// keypad header include
#include "keypad.h"

// keypad states
#define KEY_RELEASE 0x00
#define KEY_DEBOUNCE_DOWN 0x01
#define KEY_PRESSED 0x02
#define KEY_DEBOUNCE_UP 0x03
#define KEY_DONE 0x04

// "private" keypad runtime global variables
unsigned char keymode;
unsigned char keycount;
unsigned char keymaybe;
unsigned char keyterminate;
unsigned char keysymbol_table[16] = {'1', '2', '3', 'A',
									 '4', '5', '6', 'B',
									 '7', '8', '9', 'C',
									 '*', '0', '#', 'D'};
flash unsigned char keycode_table[16] = {0xEE, 0xDE, 0xBE, 0x7E,
										 0xED, 0xDD, 0xBD, 0x7D,
										 0xEB, 0xDB, 0xBB, 0x7B,
										 0xE7, 0xD7, 0xB7, 0x77};

// "private" function prototypes
unsigned char keypad_scan(void);
unsigned char keypad_lookup(unsigned char keycode);

void keypad_map_keysymbol(unsigned char old_keysymbol, unsigned char new_keysymbol)
{
	unsigned char i;

	// loop through entire table
	for (i=0; i<16; i++)
	{
		// lookup old keysymbol
		if (keysymbol_table[i] == old_keysymbol)
		{
			// map new keysymbol
			keysymbol_table[i] = new_keysymbol;

			// break loop execution
			break;
		}
	}
}

void keypad_define_terminator(unsigned char keysymbol)
{
	// map keysymbol to '\0'
	keypad_map_keysymbol(keysymbol, '\0');
}

unsigned char keypad_scan(void)
{
	unsigned char keycode;

	// get lower nibble
	DDRA = 0x0F;
	PORTA = 0xF0;
	delay_us(5);
	keycode = PINA;

	// get upper nibble
	DDRA = 0xF0;
	PORTA = 0x0F;
	delay_us(5);
	keycode |= PINA;

	// return keycode
	return keycode;
}

unsigned char keypad_lookup(unsigned char keycode)
{
	unsigned char i;
	unsigned char keysymbol = 0xFF; // default is invalid

	// check for keycode
	if (keycode != 0xFF)
	{
		// loop through entire table
		for (i=0; i<16; i++)
		{
			// lookup keycode
			if (keycode_table[i] == keycode)
			{
				// lookup keysymbol
				keysymbol = keysymbol_table[i];
				break;
			}
		}
	}

	// return keysymbol, 0xFF if invalid
	return keysymbol;
}

void keypad_get_string(void)
{
	unsigned char keycode;
	unsigned char keysymbol;

	// scan keypad for keycode
	keycode = keypad_scan();

	if (keymode == KEY_RELEASE)
	{
		// check for keycode
		if (keycode != 0xFF)
		{
			keymaybe = keycode;
			keymode = KEY_DEBOUNCE_DOWN;
		}
	}
	else if (keymode == KEY_DEBOUNCE_DOWN)
	{
		// check for keycode match
		if (keycode == keymaybe)
		{
			// lookup keysymbol based on keycode
			keysymbol = keypad_lookup(keycode);

			// check for terminator
			if (keysymbol == '\0')
			{
				// terminate key string and signal terminator
				keystring[keycount] = '\0';
				keyterminate = 0x01;
			}
			else if (keysymbol != 0xFF)
			{
				// add keysymbol to keystring and increment keycount
				keystring[keycount++] = keysymbol;
			}
			// else invalid keycode/keysymbol

			keymode = KEY_PRESSED;
		}
		else
		{
			keymode = KEY_RELEASE;
		}
	}
	else if (keymode == KEY_PRESSED)
	{
		// check for keycode mismatch
		if (keycode != keymaybe)
		{
			keymode = KEY_DEBOUNCE_UP;
		}
	}
	else if (keymode == KEY_DEBOUNCE_UP)
	{
		// check for keycode match
		if (keycode == keymaybe)
		{
			keymode = KEY_PRESSED;
		}
		else
		{
			// check if terminator was pushed
			if (keyterminate != 0x01)
		   	{
		   		// release for next button
				keymode = KEY_RELEASE;
			}
			else
			{
				// signal ready
				keystring_ready = 0x01;

				// suspend keypad state machine
				keymode = KEY_DONE;
			}
		}
	}
}

void keypad_release(void)
{
	// reset keypad variables
	keycount = 0x00;
	keystring_ready = 0x00;
	keystring[0] = '\0';
	keyterminate = 0x00;

	// restart keypad state machine
	keymode = KEY_RELEASE;
}
#endif
