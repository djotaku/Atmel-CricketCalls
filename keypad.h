/* (C) 2005 by
 * Richard West (richard@dopplereffect.us)
 * Lab Date: 23 Feb 2005 and 26 Feb 2005
 * Published: 14 March 2005
 * Published under GNU General Public License - http://www.linux.org/info/gnu.html
 * http://www.ericsbinaryworld.com
 *
 * Keypad Header File
 *
 * Hardware setup notes:
 *		Keypad connected to PortA.0-7
 */

#ifndef _KEYPAD_H_
#define _KEYPAD_H_
// Mega32 compiler directive
#include <Mega32.h>

// required header file
#include <delay.h> // for delay_us()

// "public" keypad runtime global variables
unsigned char keystring_ready;
unsigned char keystring[17];

// "public" function prototypes
void keypad_map_keysymbol(unsigned char old_keysymbol, unsigned char new_keysymbol);
/* function: replaces old_keysymbol in keysymbol_table with new_keysymbol
 * use for: modifying entries in the keysymbol_table for specific keypad buttons
 */

void keypad_define_terminator(unsigned char keysymbol);
/* function: modifies an entry in the keysymbol_table to be the terminator ('\0')
 * use for: defining a terminator
 * note: a terminator must be defined for keypad_get_string() to work
 */

void keypad_get_string(void);
/* function: scans and debounces the keypad
 * use for: reading string from keypad as part of a timer isr
 * note: runs as a state machine; completion indicated by keystring_ready == 0x01;
 * keystring contains the user's input
 */

void keypad_release(void);
/* function: resets keypad state machine
 * use for: resetting the keypad once the keystring has been interpreted by
 * calling program
 * note: the state machine will only work once if not reset
 */
#endif
