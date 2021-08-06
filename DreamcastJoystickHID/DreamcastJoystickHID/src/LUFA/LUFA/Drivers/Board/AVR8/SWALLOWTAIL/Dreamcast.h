//-----------------------------------------------------------------------------
//
//  Dreamcast.h
//
//  Swallowtail Dreamcast Controller Firmware
//  Dreamcast Interface Firmware
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
#define MAX_ERRORS 100 //Max allowable errors per frame before we need to reset the device

#define STATE_RESET_DEVICE		0 //State machine code to trigger a reset
#define STATE_GET_INFO			1
#define STATE_READ_PAD			2
#define STATE_LCD_DETECT		5
#define STATE_BANNER_DISPLAY	6
#define STATE_NULL				7

#define DC_C 0x00
#define DC_B 0x01
#define DC_A 0x02
#define DC_STRT 0x03
#define DC_UP 0x04
#define DC_DOWN 0x05
#define DC_LEFT 0x06
#define DC_RIGT 0x07
#define DC_Z 0x08
#define DC_Y 0x09
#define DC_X 0x0A
#define DC_D 0x0B
#define DC_UP2 0x0C
#define DC_DOWN2 0x0D
#define DC_LEFT2 0x0E
#define DC_RIGT2 0x0F

//Physical Pin Macros
#define DDR_Dreamcast DDRD
#define PORT_Dreamcast PORTD
#define BIT_SET(byte, bit) (byte & (1<<bit))

/******************** Includes ***************************/

#include <avr/io.h>
#include "MapleBus.h"

/******************* Globals *****************************/

//Struct for holding the device info response type
/*typedef struct DeviceInfo {
	uint32_t function;	//Function codes supported by this peripheral (or:ed together) (big endian)
	uint32_t function_data[3]; //Additional info for the supported function codes (big endian)
	uint8_t area_code; //Region Code of the peripheral
	uint8_t connector_direction; //Physical orientation of bus connection
	char product_name[30]; //Name of the peripheral
	char product_license[60]; //License statement
	uint16_t standby_power; //Standby power consumption (little endian)
	uint16_t max_power; //Maximum power consumption (little endian)
};*/
/*
Available Function Codes:
Code	Function
$001	Controller
$002	Memory card
$004	LCD display
$008	Clock
$010	Microphone
$020	AR-gun
$040	Keyboard
$080	Light gun
$100	Puru-Puru pack
$200	Mouse
*/

//Struct for holding the status of the standard controller
typedef struct ControllerStatus {
	uint16_t buttons; // digital buttons bitfield (little endian)
	uint8_t rtrigger; // right analogue trigger (0-255)
	uint8_t ltrigger; // left analogue trigger (0-255)
	uint8_t joyx; // analogue joystick X (0-255)
	uint8_t joyy; // analogue joystick Y (0-255)
	uint8_t joyx2; // second analogue joystick X (0-255)
	uint8_t joyy2; // second analogue joystick Y (0-255)
} ControllerStatus;

static unsigned char state = STATE_RESET_DEVICE; //Holds the current state machine operation
static uint8_t lcd_addr = 0; //Holds the byte address on a device to write to the VMS LCD screen
static uint16_t cur_connected_device = MAPLE_FUNC_CONTROLLER; //Default Device is a Dreamcast Controller
//LCD Data for Boosto Logo
const char lcd_data_boosto[200] PROGMEM = {
	0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x00,
	#include "EthanLogo.h"
};
//LCD Data for Swallowtail Logo
const char lcd_data_swallowtail[200] PROGMEM = {
	0x00, 0x00, 0x00, 0x04,
	0x00, 0x00, 0x00, 0x00,
	#include "SwallowtailLogo.h"
};

/******************** Functions **************************/

//Set the device that is currently connected (Gather report size)
static void setConnectedDevice(uint16_t func)
{
	cur_connected_device = func;

	switch (func)
	{
		case MAPLE_FUNC_CONTROLLER:
			//A Standard Dreamcast Controller is connected
			
		break;
	}
}

//Check the controller for any added peripherals (e.g. VMS or rumble pack)
static void checkPeripherals(void)
{
	int i, v;
	unsigned char tmp[30];

	for (i=0; i<5; i++) {
		maple_sendFrame(MAPLE_CMD_RQ_DEV_INFO,
		MAPLE_ADDR_SUB(i) | MAPLE_ADDR_PORTB,
		MAPLE_DC_ADDR | MAPLE_ADDR_PORTB,
		0, 0x00);
		v =  maple_receiveFrame(tmp, 30);
		if (v==-2) {
			_delay_ms(2);
			uint16_t func = tmp[4] | tmp[5]<<8;

			if (func & MAPLE_FUNC_LCD) {
				lcd_addr = MAPLE_ADDR_SUB(i) | MAPLE_ADDR_PORTB;
			}
		}
	}
}

//Initialize the USI on the ATmega168/328 for Three-Wire Operation
void Joystick_Init(){
	
	//Initialize the Maple Bus Connection
	maple_init();
	return; //Return to call point
}

//Write to the VMS LCD Screen (ID indicates whether to write the Swallowtail Logo or Ethan's Logo
uint8_t Dreamcast_VMS_LCD_Write(char id){
	//Create a Maple Frame with the bulk write command (Code 12) targeted at the VMS LCD screen ($004)
	unsigned char tmp[30];

	if (lcd_addr) {
		maple_sendFrame_P(MAPLE_CMD_BLOCK_WRITE, lcd_addr, MAPLE_DC_ADDR | MAPLE_ADDR_PORTB, 200, id ? lcd_data_boosto : lcd_data_swallowtail);
		maple_receiveFrame(tmp, 30);
	}
	return 0;
}

//Writes the byte into the device
uint8_t Joystick_GetStatus(ControllerStatus *controller){
	//Dreamcast Controller is queried with the Get condition request (Code 9)
	//The condition structure for the Controller function code ($001) will be entered into the given ControllerStatus struct
	static uint8_t success = 0x00; //Flag to indicate if communication is successful
	static unsigned char err_count = 0;
	unsigned char tmp[30];
	//static unsigned char func_data[4];
	static int lcd_detect_count = 0;
	int v;
	//MapleBusFrame frame;
	switch (state)
	{
		case STATE_NULL:
		{
			//Do nothing
		}
		break;

		case STATE_RESET_DEVICE:
		{
			maple_sendFrame(MAPLE_CMD_RESET_DEVICE,
			MAPLE_ADDR_MAIN | MAPLE_ADDR_PORTA,
			MAPLE_DC_ADDR, 0, 0x00);
			
			state = STATE_GET_INFO;
			success = 0x01;
		}
		break;

		case STATE_GET_INFO:
		{
			maple_sendFrame(MAPLE_CMD_RQ_DEV_INFO,
			MAPLE_ADDR_MAIN | MAPLE_ADDR_PORTB,
			MAPLE_DC_ADDR | MAPLE_ADDR_PORTB, 0, 0x00);

			v = maple_receiveFrame(tmp, 30);

			// Too much data arrives and we stop listening before the controller stop transmitting. The delay
			// here is to wait until the bus is idle again before continuing.
			_delay_ms(2);
			if (v==-2) {
				uint16_t func;

				// 0-3 Header
				// 4-7 Func
				// ...
				
				func = tmp[4] | tmp[5]<<8;
				//If the device connected is a Dreamcast Controller/Then set the connected device as such
				if (func & MAPLE_FUNC_CONTROLLER) {
					setConnectedDevice(MAPLE_FUNC_CONTROLLER);
					state = STATE_LCD_DETECT;
					lcd_detect_count = 0;
				}
			}
			success = 0x01;
			err_count = 0;
		}
		break;

		// Try for 2 seconds to find the address of the LCD.
		//
		// After 2 seconds of trying, if found, send the
		// image.
		//
		// Sending the image right away after detection does not
		// seem to work. This delay works around this.
		case STATE_LCD_DETECT:
		{
			checkPeripherals(); //Check to see if the LCD screen is attached
			if (!lcd_addr)
			{
				//There is no VMS to write to
			}
			else {
				if (lcd_detect_count > 220) {
					Dreamcast_VMS_LCD_Write(0);
					state = STATE_BANNER_DISPLAY;
					lcd_detect_count = 0;
					break;
				}
			}
			
			lcd_detect_count++;
			if (lcd_detect_count > 400) {
				state = STATE_READ_PAD;
			}
			success = 0x01;
		}
		break;

		case STATE_BANNER_DISPLAY:
		{
			lcd_detect_count++;
			if (lcd_detect_count > 400) {
				Dreamcast_VMS_LCD_Write(1);
				state = STATE_READ_PAD;
			}
			success = 0x01;
		}
		break;
		case STATE_READ_PAD:
		{
			maple_sendFrame1W(MAPLE_CMD_GET_CONDITION, MAPLE_ADDR_PORTB | MAPLE_ADDR_MAIN, MAPLE_DC_ADDR | MAPLE_ADDR_PORTB, MAPLE_FUNC_CONTROLLER);

			v = maple_receiveFrame(tmp, 30);
			//Register any errors involved with receiving that frame of data
			if (v<=0) {
				err_count++;
				//If the amount of errors passes a threshold return an error code 
				if (err_count > MAX_ERRORS) {
					state = STATE_GET_INFO; //We need to re-capture the device information
				}
				return 0x00;
			}
			err_count = 0;

			if (v < 16){
				return 0x00;
			}

			controller->joyx = tmp[12]; // 12 : Joy X axis
			controller->joyy = tmp[13]; // 13 : Joy Y axis
			controller->rtrigger = tmp[10] / 2 + 0x80; // 10 : R trig
			controller->ltrigger = tmp[11] / 2 + 0x80; // 11 : L trig
			controller->buttons = (uint16_t)((tmp[9] ^ 0xff) << 8) | ((tmp[8] ^ 0xff) & 0xFF); // 8 : Buttons
			success = 0x01;
		}
		break;
		}
	
	return success;
}

/******************** Interrupt Service Routines *********/
