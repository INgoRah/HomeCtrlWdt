/* LICENSE
 *
 */
/*
 * Includes
 */
#if ARDUINO >= 100
#include "Arduino.h"			 // for delayMicroseconds, digitalPinToBitMask, etc
#else
#include "WProgram.h"			// for delayMicroseconds
#include "pins_arduino.h"	// for digitalPinToBitMask, etc
#endif
#include <avr/sleep.h>
#include <avr/pgmspace.h>

#if defined(AVRSIM)
/*
 * Library classs includes
 */
#include <Wire.h>
#include <OneWireBase.h>
#include <DS2482.h>
#include "WireWatchdog.h"
#include "TwiHost.h"
#include "CmdCli.h"
#include "OwDevices.h"
#include "SwitchHandler.h"

extern unsigned long timer0_millis;

/*
 * Local constants
 */

/*
 * Objects
 */
extern SwitchHandler swHdl;
extern OneWireBase *ds;
extern uint8_t pio_data[0x0f];
/*
 * Local variables
 */

/*
* Function declarations
*/

extern struct _sw_tbl sw_tbl[MAX_SWITCHES];
extern struct _sw_tbl timed_tbl[MAX_TIMED_SWITCH];
extern struct _dim_tbl dim_tbl[MAX_DIMMER];

void testSetup() {
	Serial.begin(115200);

	Serial.print(F("One Wire Control..."));
	swHdl.begin(ds);

	PCMSK1 |= (_BV(PCINT8) | _BV(PCINT9) | _BV(PCINT10) | _BV(PCINT11));
    PCIFR |= _BV(PCIF1); // clear any outstanding interrupt
    PCICR |= _BV(PCIE1); // enable interrupt for the group
}

void testLoop()
{
	swHdl.loop();
}

void tableSetup()
{
	sw_tbl[0].src.data = 0;
	sw_tbl[0].dst.data = 0;

	// global switch #0: 0, 1, 2 for on/off type
	// dst 1 2 1
	sw_tbl[0].src.sa.bus = 0;
	sw_tbl[0].src.sa.adr = 1;
	sw_tbl[0].src.sa.latch = 2; // 2nd bit = 2
	sw_tbl[0].dst.da.bus = 1;
	sw_tbl[0].dst.da.adr = 2;
	sw_tbl[0].dst.da.pio = 1;
	// global switch #1: 0, 1, 4 for level type
	// dst 3 3 0
	sw_tbl[1].src.data = 0;
	sw_tbl[1].dst.data = 0;
	sw_tbl[1].src.sa.bus = 0;
	sw_tbl[1].src.sa.adr = 1;
	sw_tbl[1].src.sa.latch = 3; // 3rd bit, 1, 2, 4
	sw_tbl[1].dst.da.bus = 3;
	sw_tbl[1].dst.da.adr = 3;
	sw_tbl[1].dst.da.pio = 0;
	// global switch: 0, 2, 2 for on/off type
	// dst 2 1 0
	sw_tbl[2].src.data = 0;
	sw_tbl[2].dst.data = 0;
	sw_tbl[2].src.sa.bus = 0;
	sw_tbl[2].src.sa.adr = 2;
	sw_tbl[2].src.sa.latch = 2; // 3rd bit, 1, 2, 4
	sw_tbl[2].dst.da.bus = 2;
	sw_tbl[2].dst.da.adr = 1;
	sw_tbl[2].dst.da.pio = 0;

	// global timer switch (PIR): 0, 2, 4 for level type
	// dst 3 3 0
	timed_tbl[0].src.data = 0;
	timed_tbl[0].dst.data = 0;
	timed_tbl[0].src.sa.bus = 0;
	timed_tbl[0].src.sa.adr = 2;
	timed_tbl[0].src.sa.latch = 3; // 3rd bit = 4 (1, 2, 4)
	timed_tbl[0].dst.da.bus = 3;
	timed_tbl[0].dst.da.adr = 3;
	timed_tbl[0].dst.da.pio = 0;
	// global timer switch (PIR): 2, 2, 1 for on/off type
	// dst 2 1 0
	timed_tbl[1].src.data = 0;
	timed_tbl[1].dst.data = 0;
	timed_tbl[1].src.sa.bus = 2;
	timed_tbl[1].src.sa.adr = 2;
	timed_tbl[1].src.sa.latch = 1;
	timed_tbl[1].dst.da.bus = 2;
	timed_tbl[1].dst.da.adr = 1;
	timed_tbl[1].dst.da.pio = 0;

	dim_tbl[0].dst.da.bus = 3;
	dim_tbl[0].dst.da.adr = 3;
	dim_tbl[0].dst.da.pio = 0;
	// optional
	dim_tbl[1].dst.da.bus = 2;
	dim_tbl[1].dst.da.adr = 7;
	dim_tbl[1].dst.da.pio = 0;
}

int MainTest(int test)
{
	static const byte latch = 6;
	union d_adr dst;
	uint8_t dadr, dpio;

	testSetup();
	tableSetup();
	if (test == 1) {
		/*
		Case 1:
		switch #0 on (before off) - and off
		*/
		dadr = sw_tbl[0].dst.da.adr;
		dpio = 1 << sw_tbl[0].dst.da.pio;
		uint8_t mask = ~dpio;
		pio_data[dadr] = 0xff;
		/* bus, id, pio data read, mode */
		swHdl.switchHandle(0, 1, 2, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != mask)
			return __LINE__;
		testLoop();
		// off
		swHdl.switchHandle(0, 1, 2, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != 0xff)
			return __LINE__;
		testLoop();
	}
	if (test == 2) {
		/*
		Case 2:
		switch level on ... 2nd ... off
		*/
		dadr = sw_tbl[1].dst.da.adr;
		dpio = sw_tbl[1].dst.da.pio;
		pio_data[dadr] = 0x0;

		swHdl.switchHandle(0, 1, 4, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != 0x71)
			return __LINE__;
		// check 1st level
		swHdl.switchHandle(0, 1, 4, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != 0xf1)
			return __LINE__;
		// check 2nd level
		swHdl.switchHandle(0, 1, 4, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		// check off
		if (pio_data[dadr] != 0x0)
			return __LINE__;
	}
	/*
	Case 3:
	timer on and off level type
	*/
	if (test == 3) {
		// timer switch (PIR): 0, 2, 4 for level type
		// dst 3 3 0
		dadr = timed_tbl[0].dst.da.adr;
		dpio = timed_tbl[0].dst.da.pio;
		pio_data[dadr] = 0x0;

		swHdl.switchHandle(0, 2, 4, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != 0xF1)
			return __LINE__;
		timer0_millis += DEF_SECS * 1000 + 100;
		swHdl.loop();
		if (pio_data[dadr] != 0x0)
			return __LINE__;
	}

	/*
	Case 4:
	switch on + timer on (not switching)
	*/
	if (test == 4) {
		// global switch #2: 0, 2, 2 for on/off type

		// global timer switch #2 (PIR): 2, 2, 1 for on/off type
		// dst 2 1 0
		dadr = sw_tbl[2].dst.da.adr;
		dpio = sw_tbl[2].dst.da.pio;
		pio_data[dadr] = 0xff;

		swHdl.switchHandle(0, 2, 2, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != 0xfe)
			return __LINE__;
		// timer switch (PIR): 0, 2, 4 for level type
		swHdl.switchHandle(2, 2, 1, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		// no change!
		if (pio_data[dadr] != 0xfe)
			return __LINE__;
		swHdl.switchHandle(0, 2, 2, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != 0xff)
			return __LINE__;
	}
	/*
	Case 5:
	level on + timer on (not switching)
	*/
	if (test == 5) {
		dadr = sw_tbl[1].dst.da.adr;
		dpio = sw_tbl[1].dst.da.pio;
		pio_data[dadr] = 0x0;
		// switch on
		swHdl.switchHandle(0, 1, 4, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != 0x71)
			return __LINE__;
		// no try to trigger the timer
		swHdl.switchHandle(0, 2, 4, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != 0x71)
			return __LINE__;
		timer0_millis += DEF_SECS * 1000 + 1000;
		swHdl.loop();
		if (pio_data[dadr] != 0x71)
			return __LINE__;
		// switch off
		swHdl.switchHandle(0, 1, 4, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != 0xf1)
			return __LINE__;
		swHdl.switchHandle(0, 1, 4, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != 0x00)
			return __LINE__;
	}
	/*
	Case 6:
	timer on and retrigger on / timer off
	*/
	if (test == 6) {
		// timer switch (PIR): 0, 2, 4 for level type
		// dst 3 3 0
		dadr = timed_tbl[0].dst.da.adr;
		dpio = timed_tbl[0].dst.da.pio;
		pio_data[dadr] = 0x0;

		swHdl.switchHandle(0, 2, 4, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != 0xF1)
			return __LINE__;
		timer0_millis += DEF_SECS * 500 + 100;
		swHdl.loop();
		swHdl.switchHandle(0, 2, 4, MODE_ALRAM_HANDLING | MODE_AUTO_SWITCH);
		if (pio_data[dadr] != 0xF1)
			return __LINE__;
		timer0_millis += DEF_SECS * 500 + 100;
		swHdl.loop();
		// still on
		if (pio_data[dadr] != 0xF1)
			return __LINE__;

		timer0_millis += DEF_SECS * 500 + 100;
		swHdl.loop();
		if (pio_data[dadr] != 0xF1)
			return __LINE__;
		timer0_millis += DEF_SECS * 500 + 100;
		swHdl.loop();
		// now off
		if (pio_data[dadr] != 0x0)
			return __LINE__;
	}
	dst.da.bus = 2;
	dst.da.adr = 7;
	dst.da.pio = 0;
	//swHdl.switchLevel(dst, 50);

	if (test == 7) {
		dst.da.bus = 1;
		dst.da.adr = 7;
		dst.da.pio = 1;
		swHdl.switchLevel(dst, 100);
		timed_tbl[0].src.data = 0;
		timed_tbl[0].src.sa.bus = 1;
		timed_tbl[0].src.sa.adr = 3;
		timed_tbl[0].src.sa.latch = latch;
		timed_tbl[0].dst.data = 0;
		timed_tbl[0].dst.da.adr = 7;
		timed_tbl[0].dst.da.bus = 2;
		timed_tbl[0].dst.da.pio = 1;
		dim_tbl[0].dst.data = timed_tbl[0].dst.data;
		/*
		swHdl.switchHandle(1, 3, (1 << (latch - 1)), (320 / 32), MODE_ALRAM_HANDLING | MODE_ALRAM_POLLING | MODE_AUTO_SWITCH);
		swHdl.switchHandle(1, 3, (1 << (latch - 1)), (320 / 32), MODE_ALRAM_HANDLING | MODE_ALRAM_POLLING | MODE_AUTO_SWITCH);
		swHdl.switchHandle(1, 3, (1 << (latch - 1)), (320 / 32), MODE_ALRAM_HANDLING | MODE_ALRAM_POLLING | MODE_AUTO_SWITCH);
		swHdl.switchHandle(1, 3, (1 << (latch - 1)), (320 / 32), MODE_ALRAM_HANDLING | MODE_ALRAM_POLLING | MODE_AUTO_SWITCH);
		swHdl.switchHandle(1, 3, (1 << (latch - 1)), (320 / 32), MODE_ALRAM_HANDLING | MODE_ALRAM_POLLING | MODE_AUTO_SWITCH);
		*/
		/*
		union d_adr dst;
		dst.da.bus = 1;
		dst.da.adr = 2;
		swHdl.timerUpdate(dst, 1);
		*/
	}
	return 0;
}

int main()
{
	int ret = 0, i;

	for (i = 1; i < 8; i++) {
		ret = MainTest(i);
		if (ret) {
			do {
				Serial.print(ret);
			} while (ret);
		}
	}
	return ret;
}
#endif /* AVRSIM */