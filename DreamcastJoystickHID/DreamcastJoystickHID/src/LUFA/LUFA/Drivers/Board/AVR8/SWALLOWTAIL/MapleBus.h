/* Dreamcast to USB : Sega dc controllers to USB adapter
 * Copyright (C) 2013 Rapha�l Ass�nat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The author may be contacted at raph@raphnet.net
 */
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <string.h>

#define MAPLE_CMD_RQ_DEV_INFO		1
#define MAPLE_CMD_RQ_EXT_DEV_INFO	2
#define MAPLE_CMD_RESET_DEVICE		3
#define MAPLE_CMD_SHUTDOWN_DEV		4
#define MAPLE_CMD_GET_CONDITION		9
#define MAPLE_CMD_BLOCK_WRITE		12

#define MAPLE_FUNC_CONTROLLER	0x001
#define MAPLE_FUNC_MEMCARD		0x002
#define MAPLE_FUNC_LCD			0x004
#define MAPLE_FUNC_CLOCK		0x008
#define MAPLE_FUNC_MIC			0x010
#define MAPLE_FUNC_AR_GUN		0x020
#define MAPLE_FUNC_KEYBOARD		0x040
#define MAPLE_FUNC_LIGHT_GUN	0x080
#define MAPLE_FUNC_PURUPURU		0x100
#define MAPLE_FUNC_MOUSE		0x200

#define MAPLE_ADDR_PORT(id)		((id)<<6)
#define MAPLE_ADDR_PORTA		MAPLE_ADDR_PORT(0)
#define MAPLE_ADDR_PORTB		MAPLE_ADDR_PORT(1)
#define MAPLE_ADDR_PORTC		MAPLE_ADDR_PORT(2)
#define MAPLE_ADDR_PORTD		MAPLE_ADDR_PORT(3)
#define MAPLE_ADDR_MAIN			0x20
#define MAPLE_ADDR_SUB(id)		((1)<<id) /* where id is 0 to 4 */

#define MAPLE_DC_ADDR	0
#define MAPLE_HEADER(cmd,dst_addr,src_addr,len)	( (((cmd)&0xfful)<<24) | (((dst_addr)&0xfful)<<16) | (((src_addr)&0xfful)<<8) | ((len)&0xff))

void maple_init(void);

void maple_sendFrame(uint8_t cmd, uint8_t dst_addr, uint8_t src_addr, int data_len, uint8_t *data);
void maple_sendFrame1W(uint8_t cmd, uint8_t dst_addr, uint8_t src_addr, uint32_t data);
int maple_receiveFrame(uint8_t *data, unsigned int maxlen);

void maple_sendRaw(uint8_t *data, unsigned char len);

void maple_sendFrame_P(uint8_t cmd, uint8_t dst_addr, uint8_t src_addr, int data_len, PGM_P data);

#undef NOLRC
#undef TRACE_RX_START_END
#undef TRACE_DECODED
#define TRACE_PIN1_BITS

//
//
// PORTD0 : Pin 1
// PORTD1 : Pin 5
//

void maple_init(void)
{
	DDRD = 0xF3;
	PORTD = 0x0C;
}
#define transmitMode()	do { PORTD |= 0x0C; DDRD |= 0x0C; } while(0)
#define inputMode() do { PORTD |= 0x0C; DDRD &= ~0x0C; } while(0)
#define nop() asm volatile("nop\n");

#define MAPLE_BUF_SIZE	641
volatile unsigned char maplebuf[MAPLE_BUF_SIZE];
static unsigned char buf_used;
static unsigned char buf_phase;

#define PIN_1	0x04
#define PIN_5	0x08
static void buf_reset(void)
{
	buf_used = 0;
	buf_phase = 0;
}

static void buf_addBit(char value)
{
	// The values in maplebuf will be written
	// directly to PORTD	. Unused bits will be low.
	if (buf_phase & 0x01) {
		maplebuf[buf_used] = PIN_5;
		if (value) {
			maplebuf[buf_used] |= PIN_1; // prepare data
		}
		buf_used++;
	}
	else {
		maplebuf[buf_used] = PIN_1;
		if (value) {
			maplebuf[buf_used] |= PIN_5; // prepare data
		}
		buf_used++;
	}
	buf_phase ^= 1;
}

static int maplebus_decode(unsigned char *data, unsigned int maxlen)
{
	unsigned char dst_b;
	unsigned int dst_pos;
	unsigned char last;
	unsigned char last_fell;
	int i;

#ifdef TRACE_DECODED
	PORTB |= 0x10;
	PORTB &= ~0x10;
	PORTB |= 0x10;
	PORTB &= ~0x10;
	PORTB |= 0x10;
	PORTB &= ~0x10;
#endif

	// Look for the initial phase 1 (Pin 1 high, Pin 5 low). This
	// is to skip what we got of the sync/start of frame sequence.
	// 
	for (i=0; i<MAPLE_BUF_SIZE; i++) {
		if ((maplebuf[i]&0x0C) == 0x04)
			break;
	}
	if (i==MAPLE_BUF_SIZE) {
		return -1; // timeout
	}

	dst_pos = 0;
	data[0] = 0;
	dst_b = 0x80;
	last = maplebuf[i] & 0x0C;
	last_fell = 0;
	for (; i<MAPLE_BUF_SIZE; i++) {
		unsigned char fell;
		unsigned char cur = maplebuf[i] & 0xC;

#ifdef TRACE_PIN1_BITS
		if (cur & 1) {
			PORTB |= 0x10;
		} else {
			PORTB &= ~0x10;
		}
#endif

		if (cur == last) {
			continue; // no change
		}

		fell = last & (cur ^ last);

		if (!fell) {
			// pin(s) changed, but none fell.
			last = cur;
			continue;
		}

		if (fell == last_fell) {
			// two identical consecutive phases marks the end of the packet.
#ifdef TRACE_DECODED
				PORTB |= 0x10;
				PORTB &= ~0x10;
				PORTB |= 0x10;
				PORTB &= ~0x10;
				PORTB |= 0x10;
				PORTB &= ~0x10;
#endif
			break;
		}

		// when any of the two pins fall, the
		// other pin is the data.
		if (fell) {
			if (fell == 0x0C) {
				// two pins at the same time!
				PORTB |= 0x10;
				PORTB &= ~0x10;
			}

			if (cur) {
				data[dst_pos] |= dst_b;
#ifdef TRACE_DECODED
				PORTB |= 0x10;
#endif
			}
			else {
#ifdef TRACE_DECODED
				PORTB &= ~0x10;
#endif
			}
		}		
		
		dst_b >>= 1;
		if (!dst_b) {
			dst_b = 0x80;
			dst_pos++;
			if (dst_pos >= maxlen) {
#ifdef TRACE_DECODED
				PORTB &= ~0x10;
#endif
				return -3;
			}
			data[dst_pos] = 0;
		}

		last_fell = fell;
		last = cur;
	}

#ifdef TRACE_DECODED
	PORTB &= ~0x10;
#endif

	return dst_pos;
}

/**
 * \param data Destination buffer to store reply (payload + crc + eot)
 * \param maxlen The length of the destination buffer
 * \return -1 on timeout, -2 lrc/frame error, -3 too much data. Otherwise the number of bytes received
 */
int maple_receiveFrame(unsigned char *data, unsigned int maxlen)
{
	unsigned char lrc;
	unsigned char timeout;
	int res, i;

	//
	//  __       _   _   _
	//    |_____| |_| |_| |_
	//  ___   _     _   _
	//     |_| |___| |_| |_
	//   310022011023102310
	//     ^   ^  ^  ^^  ^^
	//

	asm volatile( 
			"	push r30		\n" // 2
			"	push r31		\n"	// 2
			"	clr %0			\n" // 1 (result=0, no timeout)
			
			"	ldi r30, lo8(maplebuf)	\n"
			"	ldi r31, hi8(maplebuf)	\n"
//			"	sbi 0x5, 4		\n" // PB4
//			"	cbi 0x5, 4		\n"

			// Loop until a change is detected.	
			"	ldi r19, 20		\n"
			"wait_start_outer:	\n"
			"	dec r19			\n"
			"	breq timeout	\n"
			"	ldi r18, 255	\n"
			"	in r17, %1		\n"
			"wait_start_inner:		\n"
			"	dec r18			\n"
			"	breq wait_start_outer	\n"
			"	in r16, %1		\n"
			"	cp r16, r17		\n"
			"	breq wait_start_inner	\n"
			"	rjmp start_rx	\n"

"timeout:\n"
			"	inc %0			\n" // 1 for timeout
			"	sbi 0xB, 4		\n" // PD4
			"	cbi 0xB, 4		\n"
			"	jmp done		\n"

"start_rx:			\n"
#ifdef TRACE_RX_START_END
			"	sbi 0xB, 4		\n" // PD4
			"	cbi 0xB, 4		\n"
#endif

			// We will loose the first bit(s), but
			// it's only the start of frame.
			#include "rxcode.asm"			

"done:\n"
#ifdef TRACE_RX_START_END
			"	sbi 0xB, 4		\n" // PD4
			"	cbi 0xB, 4		\n"
#endif
			"	pop r31			\n" // 2
			"	pop r30			\n" // 2
		: "=r"(timeout)
		: "I" (_SFR_IO_ADDR(PIND))
		: "r16","r17","r18","r19") ;

	if (timeout){
		return -1;
	}
	res = maplebus_decode(data, maxlen);
	if (res<=0)
		return res;

	// A packet contains n groups of 4 bytes, plus 1 byte crc.
	if (((res-1) & 0x3) != 0) {
		return -2; // frame error
	}

#ifndef NOLRC
	for (lrc=0, i=0; i<res; i++) {
		lrc ^= data[i];
	}
	if (lrc)
		return -2; // LRC error
#endif

	/* Reverse each group of 4 bytes */
	for (i=0; i<(res-1); i+=4) {
		unsigned char tmp;

		tmp = data[i+3];
		data[i+3] = data[i];
		data[i] = tmp;

		tmp = data[i+2];
		data[i+2] = data[i+1];
		data[i+1] = tmp;
	}

	return res-1; // remove lrc
}

static void maple_sendByte(uint8_t data)
{{{
	// Phase 1 initial state (pin 1 high, pin 5 low);
	PORTD = 0x04;
	_delay_us(1);
	if (data & 0x80)
		PORTD |= 0x08;
	nop();
	PORTD &= ~0x04;
	_delay_us(1);
	
	// Phase 2 initial state (pin 1 low, pin 5 high;
	PORTD = 0x08;
	_delay_us(1);
	if (data & 0x40)
		PORTD |= 0x04;
	nop();
	PORTD &= ~0x08;
	_delay_us(1);

	// Phase 1 initial state (pin 1 high, pin 5 low);
	PORTD = 0x04;
	_delay_us(1);
	if (data & 0x20)
		PORTD |= 0x08;
	nop();
	PORTD &= ~0x04;
	_delay_us(1);
	
	// Phase 2 initial state (pin 1 low, pin 5 high;
	PORTD = 0x08;
	_delay_us(1);
	if (data & 0x10)
		PORTD |= 0x04;
	nop();
	PORTD &= ~0x08;
	_delay_us(1);

	// Phase 1 initial state (pin 1 high, pin 5 low);
	PORTD = 0x04;
	_delay_us(1);
	if (data & 0x08)
		PORTD |= 0x08;
	nop();
	PORTD &= ~0x04;
	_delay_us(1);
	
	// Phase 2 initial state (pin 1 low, pin 5 high;
	PORTD = 0x08;
	_delay_us(1);
	if (data & 0x04)
		PORTD |= 0x04;
	nop();
	PORTD &= ~0x08;
	_delay_us(1);

	// Phase 1 initial state (pin 1 high, pin 5 low);
	PORTD = 0x04;
	_delay_us(1);
	if (data & 0x02)
		PORTD |= 0x08;
	nop();
	PORTD &= ~0x04;
	_delay_us(1);
	
	// Phase 2 initial state (pin 1 low, pin 5 high;
	PORTD = 0x08;
	_delay_us(1);
	if (data & 0x01)
		PORTD |= 0x04;
	nop();
	PORTD &= ~0x08;
	_delay_us(1);
}}}

/* Slower C implementation for sending data from program memory. */
void maple_sendRaw_P(unsigned char header_data[4], PGM_P data, unsigned char len)
{
	int i;
	uint8_t tmp;
	uint8_t lrc = 0;

	transmitMode();

	// Initially both lines are high
	PORTD = 0x0C;
	// Pin 1 falls
	PORTD = 0x08;
	_delay_us(1);
	
	// Pin 5 falls
	PORTD = 0x00;
	_delay_us(1);

	// 3 pulses on pin 5
	PORTD = 0x08;
	_delay_us(1);
	PORTD = 0x00;
	_delay_us(1);
	PORTD = 0x08;
	_delay_us(1);
	PORTD = 0x00;
	_delay_us(1);
	PORTD = 0x08;
	_delay_us(1);
	PORTD = 0x00;
	_delay_us(1);

	PORTD = 0x08;
	_delay_us(1);

	PORTD = 0x0C;
	_delay_us(1);
	
	// Phase 1 initial state (pin 1 high, pin 5 low);
	PORTD = 0x04;

	for (i=0; i<4; i++) {
		maple_sendByte(header_data[i]);
		lrc ^= header_data[i];
	}

	for (i=0; i<len; i++) {
		// Swap byte order in each word
		tmp = pgm_read_byte(data + (i&0xfc) + (3-(i&0x03)));
		maple_sendByte(tmp);
		lrc ^= tmp;
		if (i && (i%8==0)) {
			_delay_us(30);
		}
	}
	
	maple_sendByte(lrc);

	// End of frame initial state (Pin 1 high, pin 5 low)
	PORTD = 0x04;
	_delay_us(1);

	// pulse on pin5
	PORTD = 0x0C;
	_delay_us(1);
	PORTD = 0x04;
	_delay_us(1);

	// pin 1 falls
	PORTD = 0x00;
	_delay_us(1);

	// pin 1 pulse
	PORTD = 0x04;
	_delay_us(1);
	PORTD = 0x00;
	_delay_us(1);

	// pin 1 rise
	PORTD = 0x04;
	_delay_us(1);

	// pin 5 rise
	PORTD = 0x0C;

	inputMode();
}

void maple_sendRaw(unsigned char *data, unsigned char len)
{
	int i;
	unsigned char b;

	buf_reset();
	for (i=0; i<len; i++) {
		for (b=0x80; b; b>>=1)
		{
			buf_addBit(data[i] & b);
		}
	}

	// Output
	transmitMode();

	// DC controller pin 1 and pin 5
#define SET_1		"	sbi %0, 2\n"
#define CLR_1		"	cbi %0, 2\n"
#define SET_5		"	sbi %0, 3\n"
#define CLR_5		"	cbi %0, 3\n"
#define DLY_8		"	nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
#define DLY_5		"	nop\nnop\nnop\nnop\nnop\n"
#define DLY_4		"	nop\nnop\nnop\nnop\n"
#define DLY_3		"	nop\nnop\nnop\n"

	asm volatile(
		"push r31\n"
		"push r30\n"

		"mov r19, %1	\n" // Length in bytes		
		"ldi r20, 0x04	\n" // phase 1 pin 1 high, pin 5 low
		"ldi r21, 0x08	\n" // phase 2 pin 1 low, pin 2 high

		"ld r16, z+		\n"

		// Sync
		SET_1 SET_5 DLY_8 

		CLR_1 DLY_4
		CLR_5 DLY_3
		
		SET_5 DLY_3
		CLR_5
		DLY_3 
		SET_5 
		DLY_3 
		CLR_5 
		DLY_3
		SET_5 
		DLY_3 
		CLR_5 DLY_3 SET_5
		DLY_5 SET_1 CLR_5

		// Pin 5 is low, Pin 1 is high. Ready for 1st phase
		// Note: Coded for 16Mhz (8 cycles = 500ns)
"next_byte:\n"

		"out %0, r20	\n" // 1  initial phase 1 state
//		"nop			\n" // 1
		"out %0, r16	\n" // 1  data
		"cbi %0, 2		\n" // 1  falling edge on pin 1
		"ld r16, z+		\n" // 2  load phase 2 data
//		"nop			\n" // 1
		
		"out %0, r21	\n" // 1  initial phase 2 state
//		"nop			\n"
		"out %0, r16	\n" // 1  data
		"cbi %0, 3		\n" // 1  falling edge on pin 5
		"ld r16, z+		\n" // 2
		"dec r19		\n" // 1  Decrement counter for brne below
		"brne next_byte	\n" // 2

		// End of transmission
		SET_1
		DLY_4

		SET_5 CLR_5 DLY_3

		CLR_1 
		DLY_3 
		SET_1 
		DLY_3 
		CLR_1 
		DLY_3 
		SET_1 
		DLY_3
		SET_5

		"pop r30		\n"
		"pop r31		\n"


		:
		: "I" (_SFR_IO_ADDR(PORTD)), "r"(buf_used/2), "z"(maplebuf)
		: "r1","r16","r17","r18","r19","r20","r21"
	);

	// back to input to receive the answer
	inputMode();
}

void maple_sendFrame1W(uint8_t cmd, uint8_t dst_addr, uint8_t src_addr, uint32_t data)
{
	uint8_t tmp[4] = { data, data >> 8, data >> 16, data >> 24 };
	maple_sendFrame(cmd, dst_addr, src_addr, 4, tmp);
}

void maple_sendFrame_P(uint8_t cmd, uint8_t dst_addr, uint8_t src_addr, int data_len, PGM_P data)
{
	unsigned char header_data[4];

	header_data[0] = data_len >> 2;
	header_data[1] = src_addr;
	header_data[2] = dst_addr;
	header_data[3] = cmd;

	// LRC is generated and sent by the function below.
	maple_sendRaw_P(header_data, data, data_len);
}

/* 
 * data is in bus order
 */
void maple_sendFrame(uint8_t cmd, uint8_t dst_addr, uint8_t src_addr, int data_len, uint8_t *data)
{
	unsigned char tmp[4 + data_len + 1];
	uint8_t lrc=0;
	int i;
	int len = 4 + data_len + 1;

	tmp[0] = data_len >> 2;
	tmp[1] = src_addr;
	tmp[2] = dst_addr;
	tmp[3] = cmd;

	if (data_len) {
		memcpy(tmp + 4, data, data_len);
	}
	
	for (lrc=0, i=0; i<data_len+4; i++) {
		lrc ^= tmp[i];
	}

	tmp[i] = lrc;
	
	maple_sendRaw(tmp, len);	
}