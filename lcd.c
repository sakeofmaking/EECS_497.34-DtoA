/******************************************************************************
 * File Name:	lcd.c
 * Program:		Assembly_ram for Real Time Embedded Programming class	
 * Author:		Robert Weber, Tom Lill
 * Purpose:		Contains control functions for LCD on STK502
 *
 *  Date		Changed by:	Changes:
 * -------		-----------	-------------------------------------------------------
 * 05 Dec 07	T Lill		replaced signal.h with interrupt.h
 ******************************************************************************/

/********************************* Includes ***********************************/
#include <avr/io.h>
#include <avr/interrupt.h>

#include "interrpt.h"
#include "errors.h"
#include "lib.h"
#include "lcd.h"

/******************************************************************************
 * global variables
 *****************************************************************************/
/* This array defines what segments need to be turned on for each character.
 * The lowest nibble gets written to the lowest LCDDR register, the highest
 * nibble to the highest LCDDR register. */
const static unsigned int NumberSegments[10] =
{
	0x1551,		// 0
	0x0110,		// 1
	0x1E11,		// 2
	0x1B11,		// 3
	0x0B50,		// 4
	0x1B41,		// 5
	0x1F40,		// 6
	0x0111,		// 7
	0x1F51,		// 8
	0x0B51 		// 9
};

// Values for controlling what's displayed on LCD
typedef enum  
{
	VOLTAGE_ACTUAL,
	FREQUENCY_ACTUAL,
	VOLTAGE_DESIRED,
	FREQUENCY_DESIRED
} LCDValueType;

/******************************************************************************
 * Function prototypes
 *****************************************************************************/
eErrorType GetLCDDRValues(char LCDChar,
			   char *LCDDRx, char *LCDDRx5, char *LCDDRx10, char *LCDDRx15);
eErrorType GetLCDDRx(char CharPosition, char *LCDDRx, char *Nibble);
void LCDWrite(char LCDChar, unsigned char Position);

/******************************************************************************
 * Initialization routines for used interrupts
 *
 * Note: The oscillator frequency affects the update rate of the LCD.
 *       To get the LCD to work, the TOSC needs to select the 32 KHz crystal,
 *		 not the oscillator from the STK500. I don't know why we cannot use
 *		 the STK500's oscillator and scale it down.
 *****************************************************************************/
void InitLCD()
{
	/*------------------ Set LCDCRA values --------------------------
	 * Bit(s)   7: LCDEN  = 1  Enable the LCD
	 *          6: LCDAB  = 0  Default (non-low power) waveform
	 *          5: Unused = 0
	 *          4: LCDIF  = 0  LCD interrupt status
	 *          3: LCDIE  = 0  Disable LCD interrupt
	 *        2-1: Unused = 0
	 *          0: LCDBL  = 0  Do not blank LCD
	 */
	LCDCRA = _BV(LCDEN);

	/*------------------ Set LCDCRB values --------------------------
	 * Bit(s)   7: LCDCS  = 1  Use external clock
	 *          6: LCD2B  = 0  Use 1/3 bias for STK502 LCD
	 *          5: LCDMUX1= 1  Use 1/4 duty cycle
	 *          4: LCDMUX0= 1
	 *          3: Unused = 0
	 *          2: LCDPM2 = 1  Use 25 segments on LCD
	 *          1: LCDPM1 = 1
	 *          0: LCDPM0 = 1
	 */
	LCDCRB = _BV (LCDCS) | _BV(LCDMUX1) | _BV(LCDMUX0) |
		_BV(LCDPM2) | _BV(LCDPM1) | _BV(LCDPM0);

	/*------------------ Set LCDFRR values --------------------------
	 * Bit(s)   7: Unused = 0
	 *          6: LCDPS2 = 0  Set prescaler to 64. This assumes we're using
	 *          5: LCDPS1 = 0  the external, 32.768 KHz oscillator
	 *          4: LCDPS0 = 1
	 *          3: Unused = 0
	 *          2: LCDCD2 = 0  and clock divide by 1 (LCDCD values + 1)
	 *          1: LCDCD1 = 0
	 *          0: LCDCD0 = 0
	 *    This should give us a frame rate of 64Hz.
	 *	  Internal oscillator of 32.768 KHz.
	 *    Frame Rate = 32.768 KHz/(8 * Prescaler * (1 + LCDCD))
	 */
	LCDFRR = _BV(LCDPS0);

	/*------------------ Set LCDCCR values --------------------------
	 * Bit(s) 7-4: Unused = 0
	 *          3: LCDCC3 = 1  Set LCD voltage to 3.0V
	 *          2: LCDCC2 = 0
	 *          1: LCDCC1 = 0
	 *          0: LCDCC0 = 0
	 */
	LCDCCR = _BV(LCDCC3);

	// Set all characters off
	LCDDR0  = 0;
	LCDDR1  = 0;
	LCDDR2  = 0;
	LCDDR3  = 0;
	LCDDR5  = 0;
	LCDDR6  = 0;
	LCDDR7  = 0;
	LCDDR8  = 0;
	LCDDR10 = 0;
	LCDDR11 = 0;
	LCDDR12 = 0;
	LCDDR13 = 0;
	LCDDR15 = 0;
	LCDDR16 = 0;
	LCDDR17 = 0;
}

/******************************************************************************
 * Get values for LCDDR registers
 *****************************************************************************/
eErrorType GetLCDDRValues(char LCDChar,
			   char *LCDDRx, char *LCDDRx5, char *LCDDRx10, char *LCDDRx15)
{
	int index = LCDChar - '0';

	if ((LCDChar >= '0') && (LCDChar <= '9'))
	{
		*LCDDRx   =  NumberSegments[index] & 0x000F;
		*LCDDRx5  = (NumberSegments[index] & 0x00F0) >> 4;
		*LCDDRx10 = (NumberSegments[index] & 0x0F00) >> 8;
		*LCDDRx15 = (NumberSegments[index] & 0xF000) >> 12;
		return NO_ERROR;
	}

	else if (LCDChar == ' ')
	{
		*LCDDRx   = 0x00;
		*LCDDRx5  = 0x00;
		*LCDDRx10 = 0x00;
		*LCDDRx15 = 0x00;
		return NO_ERROR;
	}

	else
	{
		return LCD_INVALID_CHAR;
	}
}

/******************************************************************************
 * Get values for LCDDR registers
 *****************************************************************************/
eErrorType GetLCDDRx(char CharPosition, char *LCDDRx, char *Nibble)
{
	if ((CharPosition >= 2) && (CharPosition <= 7))
	{
		*LCDDRx = (CharPosition / 2) - 1;
		*Nibble = CharPosition % 2;
		return NO_ERROR;
	}
	else
	{
		return LCD_INVALID_POS;
	}
}

/******************************************************************************
 * Write to LCD
 *****************************************************************************/
void LCDWrite(char LCDChar, unsigned char Position)
{
	char Nibble, LCDDRx;
	char LCDDRxValue, LCDDRx5Value, LCDDRx10Value, LCDDRx15Value;
	eErrorType error;

	if (((error = GetLCDDRx(Position, &LCDDRx, &Nibble)) == NO_ERROR) &&
	    ((error = GetLCDDRValues(LCDChar, &LCDDRxValue, &LCDDRx5Value,
					&LCDDRx10Value, &LCDDRx15Value)) == NO_ERROR))
	{
		switch (LCDDRx)
		{
			case 0:
				if (Nibble == 0)
				{
					LCDDR0  = (LCDDR0  & 0xF0) | LCDDRxValue;
					LCDDR5  = (LCDDR5  & 0xF0) | LCDDRx5Value;
					LCDDR10 = (LCDDR10 & 0xF0) | LCDDRx10Value;
					LCDDR15 = (LCDDR15 & 0xF0) | LCDDRx15Value;
				}
				else
				{
					LCDDR0  = (LCDDR0  & 0x0F) | (LCDDRxValue << 4);
					LCDDR5  = (LCDDR5  & 0x0F) | (LCDDRx5Value << 4);
					LCDDR10 = (LCDDR10 & 0x0F) | (LCDDRx10Value << 4);
					LCDDR15 = (LCDDR15 & 0x0F) | (LCDDRx15Value << 4);
				}
				break;

			case 1:
				if (Nibble == 0)
				{
					LCDDR1  = (LCDDR1  & 0xF0) | LCDDRxValue;
					LCDDR6  = (LCDDR6  & 0xF0) | LCDDRx5Value;
					LCDDR11 = (LCDDR11 & 0xF0) | LCDDRx10Value;
					LCDDR16 = (LCDDR16 & 0xF0) | LCDDRx15Value;
				}
				else
				{
					LCDDR1  = (LCDDR1  & 0x0F) | (LCDDRxValue << 4);
					LCDDR6  = (LCDDR6  & 0x0F) | (LCDDRx5Value << 4);
					LCDDR11 = (LCDDR11 & 0x0F) | (LCDDRx10Value << 4);
					LCDDR16 = (LCDDR16 & 0x0F) | (LCDDRx15Value << 4);
				}
				break;

			case 2:
				if (Nibble == 0)
				{
					LCDDR2  = (LCDDR2  & 0xF0) | LCDDRxValue;
					LCDDR7  = (LCDDR7  & 0xF0) | LCDDRx5Value;
					LCDDR12 = (LCDDR12 & 0xF0) | LCDDRx10Value;
					LCDDR17 = (LCDDR17 & 0xF0) | LCDDRx15Value;
				}
				else
				{
					LCDDR2  = (LCDDR2  & 0x0F) | (LCDDRxValue << 4);
					LCDDR7  = (LCDDR7  & 0x0F) | (LCDDRx5Value << 4);
					LCDDR12 = (LCDDR12 & 0x0F) | (LCDDRx10Value << 4);
					LCDDR17 = (LCDDR17 & 0x0F) | (LCDDRx15Value << 4);
				}
				break;

			default:
				// Should never happen
				break;
		}
	}
	else
	{
		ReportError(error);
	}
}
