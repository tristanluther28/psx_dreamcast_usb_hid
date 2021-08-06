  
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
  #define PIN_SPI PINB
  #define BIT_SET(byte, bit) (byte & (1<<bit))
  #define CLOCK_PER 32 //The period of the clock in micro-seconds
  #define HOLD_TIME 4 //Hold for 2us between software transactions

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

  //Writes the byte into the device and receives byte back using software
  uint8_t SPI_Transfer(uint8_t byte){
	  // 1. The clock is held high until a byte is to be sent.
	  uint8_t out = 0x00;
	  uint8_t i = 0;
	  for (i = 0; i < 8; i++){
		  // 2. When the clock edge drops low, the values on the line start to change
		  PORT_SPI &= ~(1<<SCK);
		  _delay_us(HOLD_TIME);
		  //Get bit i from byte
		if(((byte & ( 1 << i )) >> i)){
			PORT_SPI |= (1<<MOSI);
		}
		else{
			PORT_SPI &= ~(1<<MOSI);
		}
		_delay_us(CLOCK_PER / 2 - HOLD_TIME);
		// 3. When the clock goes from low to high, value are actually read
		PORT_SPI |= (1<<SCK);
		_delay_us(HOLD_TIME);
		if((PIN_SPI & (1<<MISO))){
			out |= (1<<i);
		}
		_delay_us(CLOCK_PER / 2 - HOLD_TIME);
	  }
	  return out;
  }

  /******************** Interrupt Service Routines *********/
