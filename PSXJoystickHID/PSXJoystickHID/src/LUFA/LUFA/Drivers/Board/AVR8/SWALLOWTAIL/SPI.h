  
  //-----------------------------------------------------------------------------
  //
  //  SPI_ATmega32U4.h
  //
  //  Swallowtail SPI Firmware
  //  AVR (ATmega32U4) SPI Firmware
  //
  //  Copyright (c) 2021 Swallowtail Electronics
  //
  //  Permission is hereby granted, free of charge, to any person obtaining a
  //  copy of this software and associated documentation files (the "Software"),
  //  to deal in the Software without restriction, including without limitation
  //  the rights to use, copy, modify, merge, publish, distribute, sub-license,
  //  and/or sell copies of the Software, and to permit persons to whom the
  //  Software is furnished to do so, subject to the following conditions:
  //
  //  The above copyright notice and this permission notice shall be included in
  //  all copies or substantial portions of the Software.
  //
  //  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  //  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  //  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  //  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  //  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  //  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  //  DEALINGS IN THE SOFTWARE.
  //
  //  Web:    http://tristanluther.com
  //  Email:  tristanluther28@gmail.com
  //
  //-----------------------------------------------------------------------------

  /******************** Macros *****************************/

  #define SS PB0
  #define MOSI PB2
  #define MISO PB3
  #define SCK PB1
  #define DDR_SPI DDRB
  #define PORT_SPI PORTB
  #define BIT_SET(byte, bit) (byte & (1<<bit))

  /******************** Includes ***************************/

  #include <avr/io.h>

  /******************* Globals *****************************/


  /******************** Functions **************************/

  //Initialize the USI on the ATmega168/328 for Three-Wire Operation
  void SPI_init(){
	  //Initialize the output for SPI Master Mode
	  DDR_SPI |= (1<<SS)|(1<<MOSI)|(0<<MISO)|(1<<SCK); //SS | MOSI | MISO | SCK
	  //Set the default values for outputs to zero and inputs to have pull-up resistors
	  PORT_SPI |= (0<<SS)|(0<<MOSI)|(1<<MISO)|(0<<SCK); //SS | MOSI | MISO | SCK
	  //SPI Control Register, Enable, LSB bit first, Set to master mode, Mode 1:1, f/128 SCK Frequency
	  SPCR = (1<<SPE) | (1<<DORD) | (1<<MSTR) | (1<<CPOL) | (1<<CPHA) | (1<<SPR1) | (1<<SPR0);
	  SPSR = (0<<SPI2X);
	  return; //Return to call point
  }

  //Enable the slave select line
  void SPI_Enable(){
	  //Enable SS Line
	  PORT_SPI &= ~(1<<SS);
  }

  //Enable the slave select line
  void SPI_Disable(){
	  //Disable SS Line
	  PORT_SPI |= (1<<SS);
  }

  //Writes the byte into the device
  uint8_t SPI_Transfer(uint8_t byte){
	  //Write data to SPI register
	  SPDR = byte;
	  //Wait until the transmission is complete
	  while(!(SPSR & (1<<SPIF)));
	  //Return the data in the register
	  return SPDR;
  }

  /******************** Interrupt Service Routines *********/
