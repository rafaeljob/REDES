/**********************************************************************
 *
 * Filename:    main.c
 * 
 * Description: A simple test program for the CRC implementations.
 *
 * Notes:       To test a different CRC standard, modify crc.h.
 *
 * 
 * Copyright (c) 2000 by Michael Barr.  This software is placed into
 * the public domain and may be used for any purpose.  However, this
 * notice must not be changed or removed and no warranty is either
 * expressed or implied by its publication or distribution.
 **********************************************************************/

#include "crc.h"

void main(void)
{
	unsigned char  test[] = "batata";
	/*
	 * Print the check value for the selected CRC algorithm.
	 */
	printf("The check value for the %s standard is 0x%X\n", CRC_NAME, CHECK_VALUE);
	
	/*
	 * Compute the CRC of the test message, slowly.
	 */
	printf("The crcSlow() of %s is 0x%X\n", test, crcSlow(test, strlen(test)));
	
	/*
	 * Compute the CRC of the test message, more efficiently.
	 */
	crcInit();
	printf("The crcFast() of %s is 0x%X\n", test, crcFast(test, strlen(test)));
}