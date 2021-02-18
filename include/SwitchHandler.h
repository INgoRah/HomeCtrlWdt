#include <OneWireBase.h>
#include "OwDevices.h"

/* config */
#define MAX_BUS 3
/* per bus 12 addresses and each 5 latches, sometimes long presses additionally */
#define MAX_SWITCHES MAX_BUS * 12 * 5
#define MAX_TIMED_SWITCH 10
#define MAX_DIMMER 3

#define HOST_ALRM_PIN 6

/* modes */
#define MODE_ALRAM_POLLING 0x2
#define MODE_ALRAM_HANDLING 0x4
#define MODE_AUTO_SWITCH 0x8

struct _sw_tbl {
	union s_adr src;
	union d_adr dst;
};

struct _dim_tbl {
	union d_adr dst;
	struct {
		unsigned int level : 4;
		unsigned int up : 1;
		unsigned int dn : 1;
	}lvl;
};

enum _pio_mode {
	ON,
	OFF,
	TOGGLE
};

class SwitchHandler
{
	private:
		OwDevices* ow;
		OneWireBase *ds;
		uint8_t data[10];
		byte mode;
		uint16_t srcData(uint8_t busNr, uint8_t adr1);

	public:

		SwitchHandler(OwDevices* ow) { this->ow = ow; };
		void begin(OneWireBase *ds);
		void loop();
		void initSwTable();
		bool alarmHandler(byte busNr, byte mode);
		bool switchHandle(uint8_t busNr, uint8_t adr1);
		bool switchHandle(uint8_t busNr, uint8_t adr1, uint8_t latch, uint8_t mode);
		bool timerUpdate(union d_adr dst, uint16_t secs);
		bool switchPio(union d_adr dst, enum _pio_mode mode);
		bool switchLevel(union d_adr dst, uint8_t level);
};