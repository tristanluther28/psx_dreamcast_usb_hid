/*
             LUFA Library
     Copyright (C) Dean Camera, 2017.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2017  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the Joystick demo. This file contains the main tasks of
 *  the demo and is responsible for the initial application hardware configuration.
 */

#include "Joystick.h"

/** Buffer to hold the previously generated HID report, for comparison purposes inside the HID class driver. */
static uint8_t PrevJoystickHIDReportBuffer[sizeof(USB_JoystickReport_Data_t)];

/** LUFA HID Class driver interface configuration and state information. This structure is
 *  passed to all HID Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_HID_Device_t Joystick_HID_Interface =
	{
		.Config =
			{
				.InterfaceNumber              = INTERFACE_ID_Joystick,
				.ReportINEndpoint             =
					{
						.Address              = JOYSTICK_EPADDR,
						.Size                 = JOYSTICK_EPSIZE,
						.Banks                = 1,
					},
				.PrevReportINBuffer           = PrevJoystickHIDReportBuffer,
				.PrevReportINBufferSize       = sizeof(PrevJoystickHIDReportBuffer),
			},
	};


/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	SetupHardware();

	GlobalInterruptEnable();

	for (;;)
	{
		HID_Device_USBTask(&Joystick_HID_Interface);
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
#if (ARCH == ARCH_AVR8)
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
#elif (ARCH == ARCH_XMEGA)
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
#endif

	/* Hardware Initialization */
	Joystick_Init();
	USB_Init();
	
	DDRD |= (1<<PD0);
	PORTD |= (0<<PD0);
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
	
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= HID_Device_ConfigureEndpoints(&Joystick_HID_Interface);

	USB_Device_EnableSOFEvents();
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	HID_Device_ProcessControlRequest(&Joystick_HID_Interface);
}

/** Event handler for the USB device Start Of Frame event. */
void EVENT_USB_Device_StartOfFrame(void)
{
	HID_Device_MillisecondElapsed(&Joystick_HID_Interface);
}

/** HID class driver callback function for the creation of HID reports to the host.
 *
 *  \param[in]     HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in,out] ReportID    Report ID requested by the host if non-zero, otherwise callback should set to the generated report ID
 *  \param[in]     ReportType  Type of the report to create, either HID_REPORT_ITEM_In or HID_REPORT_ITEM_Feature
 *  \param[out]    ReportData  Pointer to a buffer where the created report should be stored
 *  \param[out]    ReportSize  Number of bytes written in the report (or zero if no report is to be sent)
 *
 *  \return Boolean \c true to force the sending of the report, \c false to let the library determine if it needs to be sent
 */
bool CALLBACK_HID_Device_CreateHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                         uint8_t* const ReportID,
                                         const uint8_t ReportType,
                                         void* ReportData,
                                         uint16_t* const ReportSize)
{
	USB_JoystickReport_Data_t* JoystickReport = (USB_JoystickReport_Data_t*)ReportData;

	//Get the controller status
	static PSXControllerStatus controller;
	uint8_t success = Joystick_GetStatus(&controller);
	//If the X button was pressed/LED ON
	if(success){
		//Fill in the report
		if (controller.buttons & (1<<PSX_SQR))
			JoystickReport->Button = (1 << 0);
		if (controller.buttons & (1<<PSX_X))
			JoystickReport->Button |= (1 << 1);
		if (controller.buttons & (1<<PSX_CIRC))
			JoystickReport->Button |= (1 << 2);
		if (controller.buttons & (1<<PSX_TRGL))
			JoystickReport->Button |= (1 << 3);
		if (controller.buttons & (1<<PSX_R1))
			JoystickReport->Button |= (1 << 5);
		if (controller.buttons & (1<<PSX_L1))
			JoystickReport->Button |= (1 << 4);
		if (controller.buttons & (1<<PSX_R2)){
			JoystickReport->Button |= (1 << 7);
			JoystickReport->Ry = 127;
		}
		else{
			JoystickReport->Ry = -127;
		}
		if (controller.buttons & (1<<PSX_L2)){
			JoystickReport->Button |= (1 << 6);
			JoystickReport->Rx = 127;
		}
		else{
			JoystickReport->Rx = -127;
		}
		int8_t dpad = ((controller.buttons & 0xF000)>>12);
		switch (dpad)
		{
		case 0x04:
			//Down
			JoystickReport->Slider = 0x00;
			break;
		case 0x0C:
			//Down Right
			JoystickReport->Slider = 0x20;
			break;
		case 0x08:
			//Right
			JoystickReport->Slider = 0x40;
			break;
		case 0x09:
			//Up Right
			JoystickReport->Slider = 0x60;
			break;
		case 0x01:
			//Up
			JoystickReport->Slider = 0x81;
			break;
		case 0x03:
			//Up Left
			JoystickReport->Slider = 0xA0;
			break;
		case 0x02:
			//Left
			JoystickReport->Slider = 0xC0;
			break;
		case 0x06:
			//Left Down
			JoystickReport->Slider = 0xE0;
			break;
		default:
			//Act like nothing was pressed
			JoystickReport->Slider = 0x80;
			break;
		}
		if ((controller.buttons & (1<<PSX_STRT)) && !(controller.buttons & (1<<PSX_SLCT)))
			JoystickReport->Button |= (1 << 9);
		if (controller.buttons & (1<<PSX_R3))
			JoystickReport->Button |= (1 << 10);
		if (controller.buttons & (1<<PSX_L3))
			JoystickReport->Button |= (1 << 11);
		if (!(controller.buttons & (1<<PSX_STRT)) && (controller.buttons & (1<<PSX_SLCT)))
			JoystickReport->Button |= (1 << 8);
		if ((controller.buttons & (1<<PSX_STRT)) && (controller.buttons & (1<<PSX_SLCT)))
			JoystickReport->Button |= (1 << 12);
		
		JoystickReport->X = -1*(controller.joylx-127);
		JoystickReport->Y = -1*(controller.joyly-127);
		JoystickReport->Rz = -1*(controller.joyry-127);
		JoystickReport->Z = -1*(controller.joyrx-127);
		//Send that report out
		*ReportSize = sizeof(USB_JoystickReport_Data_t);
	}
	return false;
}

/** HID class driver callback function for the processing of HID reports from the host.
 *
 *  \param[in] HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in] ReportID    Report ID of the received report from the host
 *  \param[in] ReportType  The type of report that the host has sent, either HID_REPORT_ITEM_Out or HID_REPORT_ITEM_Feature
 *  \param[in] ReportData  Pointer to a buffer where the received report has been stored
 *  \param[in] ReportSize  Size in bytes of the received HID report
 */
void CALLBACK_HID_Device_ProcessHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                          const uint8_t ReportID,
                                          const uint8_t ReportType,
                                          const void* ReportData,
                                          const uint16_t ReportSize)
{
	// Unused (but mandatory for the HID class driver) in this demo, since there are no Host->Device reports
}

