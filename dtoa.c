/******************************************************************************
 * File Name:	dtoa.c
 * Program:		Project for Real-Time Embedded Systems Programming class
 * Author:		Robert Weber
 * Purpose:		Contains functions to retrieve analog values from the D/A.
 *
 *  Date	Changed by:	Changes:
 * -------	-----------	-------------------------------------------------------
 * 07Mar02	R Weber		Original file
 * 24Feb03  R Weber     Updated to increase speed.
 * 08Mar04  R Weber     Updated for Atmega169
 * 29Sep05	T Lill		Removed deprecated functions
 ******************************************************************************/
#include "lib.h"
#include "errors.h"
#include "dtoa.h"
#include "serial.h"

// Number of bytes to send on each SPI transmission
#define SPI_NUM_BYTES               2

// When in DEBUG mode
//#define DEBUG

/******************************************************************************
 * This function initializes the SPI port for communication with the D/A 
 * converter.
 ******************************************************************************/
void InitDtoA(void)
{
    /* Set SPI Control register, with:
     *   SPIE:  0 - SPI Interrupt disabled
     *   SPE:   1 - SPI Enabled.
     *   DORD:  0 - Data order is MS bit first
     *   MSTR:  1 - CPU is the master
     *   CPOL:  0 - SCLK line is low when SPI is idling
     *   CPHA:  0 - Data sampled on clocks rising edge
     *   SPR1:	0 - 
	 *   SPR0:  1 - Sck frequency = Fosc/8
	 *              when SPI2X is set to 1
	 */
	SPCR = ( _BV(MSTR) | _BV(SPR0) | _BV(SPE) );
	
	/* Set SPI2X on SPSR to finish setting SCK frequency 
	 * Note that since this only writable bit in this register is the SPI2X
	 * bit, We can just write. */
	SPSR = _BV(SPI2X);
	
	/* Set Port B, pin 4 to be an output. This is actually already done in 
	 * main.c, but I like to do it again here in case, sometime down the road,
	 * it's not setup in main. Keep your configuration close to where it's needed.
	 */
	SET_BIT(DDRB, D2A_CS_BIT);
	
	/* And set PB4 high, so D/A is not selected */
	SET_BIT(PORTB, D2A_CS_BIT); 
	
	/* Set D/A to 0 initially */
	WriteDtoASample(0);
}

/******************************************************************************
 * This function writes data to the D/A.
 *
 * Our interface is 12 bits. To match the format of our D/A, we need
 * to do a 16-bit write, with the following format, Most-Significant bits first:
 * 4 MS bits: dummy bits. Don't care.
 * 10 next MS bits: The value we want to write.
 * 2 LS bits: extra bits. Don't care.
 *
 * Note: There are 2 errors that can be detected in our SPI system.
 *       Mode Fault:        This indicates two devices have tried to be a master
 *                          at the same time. This is determined to have
 *                          happened when we're a Master and the Slave Select
 *                          (SS) line is externally  driven low. Since only we
 *                          drive the PD5_SS pin, this should never happen.
 *       Write Collision:   If we try to write a sample into the data register
 *                          before the last one has been transmitted, we'll see
 *                          this error. To prevent this, we must always check
 *                          that the data register is empty before writing to
 *                          it.
 ******************************************************************************/
/* This union is used so we can access the individual bytes of the passed-in
 * integer. The endianness of the processor will affect the order of the MSB
 * and LSB. */
typedef union
{
	unsigned int WholeInt;
	struct
	{
		unsigned int LSB : 8;
		unsigned int MSB : 8;
	}Bytes;
} SampleType;

void WriteDtoASample ( unsigned int Value )
{

	/* Assignment
	 * In this function, we write 2 bytes to the D/A, MSB first.
	 * Follow these steps:
	 * 1. Shift the input value left by 2 to match TLC5615 format
	 * 2. Enable the D/A.
	 * 3. Read the status register, and check for any errors. Report an error
	 *    if one exists. A write collision is the only detectable error.
	 *    Some chips require you to clear any existing error by reading the
	 *    status register, or it won't transmit. the Atmega169 doesn't seem to
	 *    have this restriction.
	 * 4. Write the MSB to the D/A.
	 * 5. Wait for the Tx to be complete. Alternatively, you could implement an
	 *    ISR to do this.
	 * 6. Check the status register for errors again.
	 * 7. Write the LSB.
	 * 8. Wait for this Tx to complete.
	 * 9. Deselect the D/A
	 */	

	// 1. Shift input value left by 2 bits to match TLC5615 format
	Value = Value << 2;

	// 2. Enable the D/A
	CLEAR_BIT(PORTB, D2A_CS_BIT);

	// 3. Read status register and check for any errors.
	if(SPSR & (1 << 6)){
		SCIWriteString_P(PSTR("Collision detected\n\r"));
	}

	// 4. Write the MSB to the D/A
	SPDR = (unsigned char)(Value >> 8);

	// 5. Wait for transmission to be complete
	while(!(SPSR & (1 << SPIF)));

	// 6. Read status register for errors again
	if(SPSR & (1 << 6)){
		SCIWriteString_P(PSTR("Collision detected\n\r"));
	}

	// 7. Write the LSB to the D/A
	SPDR = (unsigned char)Value;

	// 8. Wait for transmission to be complete
	while(!(SPSR & (1 << SPIF)));

	// 9. Deselect D/A
	SET_BIT(PORTB, D2A_CS_BIT);

#ifdef DEBUG
	char *ptrValueStr;
	char ValueStr[20];
	
	// Prints int value to serial
	SCIWriteString_P(PSTR("Value = "));
    ptrValueStr = ValueStr;
    _itoa(&ptrValueStr, Value, 10);
    SCIWriteString(ValueStr);
    SCIWriteString_P(PSTR("\n\r"));
#endif /* DEBUG */

}  /* End of WriteDtoASample */
