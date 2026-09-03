#ifndef _TESTERS_H_
#define _TESTERS_H_

// Helpers to populate counts from data
#define MAKE_PINS(...) \
	(sizeof((const struct Pin[]) {__VA_ARGS__}) / sizeof(struct Pin)), \
	((const struct Pin[]) {__VA_ARGS__})
#define MAKE_PULLS(...) \
	(sizeof((const struct Pull[]) {__VA_ARGS__}) / sizeof(struct Pull)), \
	((const struct Pull[]) {__VA_ARGS__})
#define MAKE_PULLS_NONE() 0, NULL
#define MAKE_INTERFACES(...) \
	(sizeof((const struct Interface[]) {__VA_ARGS__}) / sizeof(struct Interface)), \
	((const struct Interface[]) {__VA_ARGS__})
#define MAKE_PULLNAMES(...) \
	(sizeof((const struct PullName[]) {__VA_ARGS__}) / sizeof(struct PullName)), \
	((const struct PullName[]) {__VA_ARGS__})
#define MAKE_PULLNAMES_NONE() 0, NULL
#define MAKE_INTERFACES_NONE() 0, NULL

// GPIO pin
struct Pin {
	uint8_t gpio; // GPIO pin number
	const char* name; // Name
};

// Pull for GPIO pin
struct Pull {
	const char *name; // Offset into Pins array (MUST BE ADC PIN)
	uint8_t control; // Control GPIO pin 
	uint32_t r; // Resistance
	uint8_t id; // Pull system ID
};

// Pull up/down detection specs
struct PullName {
	uint8_t id; // Pull system ID
        const char* name;
        uint32_t pdmin; // Pull down minimum resistance
        uint32_t pdmax; // Pull down maximum resistance
        uint32_t pumin; // Pull up minimum resistance
        uint32_t pumax; // Pull up maximum resistance
};

// Interfaces
struct Interface {
	const char* prefix;
	const char* name;
	uint8_t detect0; // detect pins are joined via the connector
	uint8_t detect1;
};

struct Tester {
	uint8_t config_id; // Config pulls max 4 pins (none/high/low/any)
	const char * name; // Tester name
	uint8_t n_pins;
	const struct Pin *pins; // 3 are used for configuration
	uint8_t n_pulls;
	const struct Pull *pulls;
	uint8_t n_pullnames;
	const struct PullName *pullnames;
	const char *pull_high; // Pin to set high when reading pulls
	const char *pull_low; // Pin to set low when reading pulls
	uint8_t n_interfaces;
	const struct Interface *interfaces;
};

// Number of testers defined
#define N_TESTERS 9

const struct Tester testers [N_TESTERS] = {
	{ // All GPIO (No pulls)
		// DO NOT REMOVE 0x00 - used for fallback config
		0x00, // GP0=None,GP1=None,GP2=None
		"Pico GPIO >=GP0",
#if NUM_BANK0_GPIOS == 30
		MAKE_PINS(
			{ 0, "GP0" },
			{ 1, "GP1" },
			{ 2, "GP2" },
			{ 3, "GP3" },
			{ 4, "GP4" },
			{ 5, "GP5" },
			{ 6, "GP6" },
			{ 7, "GP7" },
			{ 8, "GP8" },
			{ 9, "GP9" },
			{10, "GP10"},
			{11, "GP11"},
			{12, "GP12"},
			{13, "GP13"},
			{14, "GP14"},
			{15, "GP15"},
			{16, "GP16"},
			{17, "GP17"},
			{18, "GP18"},
			{19, "GP19"},
			{20, "GP20"},
			{21, "GP21"},
			{22, "GP22"},
			{26, "GP26"},
			{27, "GP27"},
			{28, "GP28"},
		),
#elif NUM_BANK0_GPIOS == 48
		MAKE_PINS(
			{ 0, "GP0" },
			{ 1, "GP1" },
			{ 2, "GP2" },
			{ 3, "GP3" },
			{ 4, "GP4" },
			{ 5, "GP5" },
			{ 6, "GP6" },
			{ 7, "GP7" },
			{ 8, "GP8" },
			{ 9, "GP9" },
			{10, "GP10"},
			{11, "GP11"},
			{12, "GP12"},
			{13, "GP13"},
			{14, "GP14"},
			{15, "GP15"},
			{16, "GP16"},
			{17, "GP17"},
			{18, "GP18"},
			{19, "GP19"},
			{20, "GP20"},
			{21, "GP21"},
			{22, "GP22"},
			{26, "GP26"},
			{27, "GP27"},
			{28, "GP28"},
			{29, "GP29"},
			{30, "GP30"},
			{31, "GP31"},
			{32, "GP32"},
			{33, "GP33"},
			{34, "GP34"},
			{35, "GP35"},
			{36, "GP36"},
			{37, "GP37"},
			{38, "GP38"},
			{39, "GP39"},
			{40, "GP40"},
			{41, "GP41"},
			{42, "GP42"},
			{43, "GP43"},
			{44, "GP44"},
			{45, "GP45"},
			{46, "GP46"},
			{47, "GP47"},
		),
#endif
		MAKE_PULLS_NONE(),
		MAKE_PULLNAMES_NONE(),
		NULL, NULL, // VBUS/GND pull names
		MAKE_INTERFACES_NONE(),
	},
	{ // Unknown - config pins have pulls but not defined in this array
		// DO NOT REMOVE 0xFF - used for fallback config
		0xFF, // Unknown pulls
		"Pico GPIO >=GP3",
#if NUM_BANK0_GPIOS == 30
		MAKE_PINS(
			{ 3, "GP3" },
			{ 4, "GP4" },
			{ 5, "GP5" },
			{ 6, "GP6" },
			{ 7, "GP7" },
			{ 8, "GP8" },
			{ 9, "GP9" },
			{10, "GP10"},
			{11, "GP11"},
			{12, "GP12"},
			{13, "GP13"},
			{14, "GP14"},
			{15, "GP15"},
			{16, "GP16"},
			{17, "GP17"},
			{18, "GP18"},
			{19, "GP19"},
			{20, "GP20"},
			{21, "GP21"},
			{22, "GP22"},
			{26, "GP26"},
			{27, "GP27"},
			{28, "GP28"},
		),
#elif NUM_BANK0_GPIOS == 48
		MAKE_PINS(
			{ 3, "GP3" },
			{ 4, "GP4" },
			{ 5, "GP5" },
			{ 6, "GP6" },
			{ 7, "GP7" },
			{ 8, "GP8" },
			{ 9, "GP9" },
			{10, "GP10"},
			{11, "GP11"},
			{12, "GP12"},
			{13, "GP13"},
			{14, "GP14"},
			{15, "GP15"},
			{16, "GP16"},
			{17, "GP17"},
			{18, "GP18"},
			{19, "GP19"},
			{20, "GP20"},
			{21, "GP21"},
			{22, "GP22"},
			{26, "GP26"},
			{27, "GP27"},
			{28, "GP28"},
			{29, "GP29"},
			{30, "GP30"},
			{31, "GP31"},
			{32, "GP32"},
			{33, "GP33"},
			{34, "GP34"},
			{35, "GP35"},
			{36, "GP36"},
			{37, "GP37"},
			{38, "GP38"},
			{39, "GP39"},
			{40, "GP40"},
			{41, "GP41"},
			{42, "GP42"},
			{43, "GP43"},
			{44, "GP44"},
			{45, "GP45"},
			{46, "GP46"},
			{47, "GP47"},
		),
#endif
		MAKE_PULLS_NONE(),
		MAKE_PULLNAMES_NONE(),
		NULL, NULL, // VBUS/GND pull names
		MAKE_INTERFACES_NONE(),
	},
	{ // USB-C Socket 16-pin
		0x80, // GP0=Low/GP1=None/GP2=None
		"USB-C (16)",
		MAKE_PINS(
			{ 6, "A3"  },
			{ 7, "A2"  },
			{ 8, "GND" },
			{ 9, "B11" },
			{10, "B10" },
			{11, "B8"  },
			{12, "D-2" },
			{13, "D+2" },
			{14, "B2"  },
			{15, "B3"  },
			{16, "A11" },
			{17, "A10" },
			{18, "VBUS"},
			{19, "A8"  },
			{20, "D-1" },
			{21, "D+1" },
			{26, "CC2" },
			{27, "CC1" },
		),
		MAKE_PULLS(
			{"CC2", 22, 10000, 0}, // CC1, GPIO22, 10k, Pull system=0
			{"CC1", 28, 10000, 0}, // CC2, GPIO28, 10k, Pull system=0
		),
		MAKE_PULLNAMES(
			{ 0, "Pu56k", 250000, 0xFFFFFFFF, 44800,  67200 },      // 56k 20% Pull Up Default (500mA USB2 / 900mA USB3)
			{ 0, "Pu56k", 250000, 0xFFFFFFFF, 44800,  67200 },      // 22k 5% Pull Up (check 10%)
			{ 0, "Pu10k", 250000, 0xFFFFFFFF, 9000,   11000 },      // 5.1k 10% Pull Down
			{ 0, "Pd5k1", 4590,   5610,       250000, 0xFFFFFFFF }, // 5.1k 10% Pull Down
			{ 0, "Ra",    800,    1200,       250000, 0xFFFFFFFF }, // Ra 800-1200R Pull Down
		),
		"VBUS", "GND",
		MAKE_INTERFACES_NONE(),
	},
	{ // 15-pin
		0xC0, // GP0=Low/GP1=Low/GP2=None
		"FFC 15-pin (1mm)",
		MAKE_PINS(
			{15, "PIN1"},
			{14, "PIN2"},
			{13, "PIN3"},
			{12, "PIN4"},
			{11, "PIN5"},
			{10, "PIN6"},
			{ 9, "PIN7"},
			{ 8, "PIN8"},
			{ 7, "PIN9"},
			{ 6, "PIN10"},
			{ 5, "PIN11"},
			{ 4, "PIN12"},
			{18, "PIN13"},
			{17, "PIN14"},
			{16, "PIN15"},
		),
		MAKE_PULLS_NONE(),
		MAKE_PULLNAMES_NONE(),
		NULL, NULL, // VBUS/GND pull names
		MAKE_INTERFACES_NONE(),
	},
	{ // 22-pin
		0x40, // GP0=None/GP1=Low/GP2=None
		"FFC 22-pin (0.5mm)",
		MAKE_PINS(
			{15, "PIN1" },
			{14, "PIN2" },
			{13, "PIN3" },
			{12, "PIN4" },
			{11, "PIN5" },
			{10, "PIN6" },
			{ 9, "PIN7" },
			{ 8, "PIN8" },
			{ 7, "PIN9" },
			{ 6, "PIN10"},
			{ 5, "PIN11"},
			{ 4, "PIN12"},
			{28, "PIN13"},
			{27, "PIN14"},
			{26, "PIN15"},
			{22, "PIN16"},
			{21, "PIN17"},
			{20, "PIN18"},
			{19, "PIN19"},
			{18, "PIN20"},
			{17, "PIN21"},
			{16, "PIN22"},
		),
		MAKE_PULLS_NONE(),
		MAKE_PULLNAMES_NONE(),
		NULL, NULL, // VBUS/GND pull names
		MAKE_INTERFACES_NONE(),
	},
	{ // USB3 MicroB Socket + USB3 Type-B Socket
		0x48, // GP0=High/GP1=Low/GP2=None
		"MicroB3 + Type-B3",
		MAKE_PINS(
			// USB3 Micro-B
			{17, "uB_RX+"},  // 10
			{18, "uB_RX-"},  // 9
			{19, "uB_GND1"}, // 8
			{20, "uB_TX+"},  // 7
			{21, "uB_TX-"},  // 6
			{ 9, "uB_GND2"}, // 5 
			{10, "uB_OTG"},  // 4
			{11, "uB_D+"},   // 3
			{12, "uB_D-"},   // 2
			{14, "uB_VBUS"}, // 1
			//{15, "uB_SHIELD"},
			// USB3 Type-B
			{27, "B_TX+"}, 
			{22, "B_TX-"},
			{ 8, "B_RX+"},
			{ 6, "B_RX-"},
			{26, "B_D+"},
			{28, "B_D-"},
			{ 7, "B_GND"},
			{ 4, "B_DRAIN"},
			//{13, "B_SHIELD"},
			{ 5, "B_VBUS"},
		),
		MAKE_PULLS_NONE(),
		MAKE_PULLNAMES_NONE(),
		NULL, NULL, // VBUS/GND pull names
		MAKE_INTERFACES(
			[0] = {"uB", "USB3 MicroB", 15, 16},
			[1] = {"B",  "USB3 Type-B", 13, 16},
		),
	},
	{ // USB2 MicroB + USB3 Type-A Socket
		0x08, // GP0=High/GP1=None/GP2=None
		"MicroB2 + Type-A3",
		MAKE_PINS(
			// Micro USB2
			{14, "uB_VBUS"},
			{11, "uB_D+"},
			{13, "uB_D-"},
			{18, "uB_ID"},
			{17, "uB_GND"},
			{15, "uB_SHIELD"},
			// USB3 Type-A"
			{21, "A_VBUS"},
			{ 5, "A_D-"},
			{ 7, "A_D+"},
			{ 9, "A_GND"},
			{10, "A_RX-"},
			{ 8, "A_RX+"},
			{ 6, "A_DRAIN"},
			{ 4, "A_TX-"},
			{20, "A_TX+"},
		),
		MAKE_PULLS_NONE(),
		MAKE_PULLNAMES_NONE(),
		NULL, NULL, // VBUS/GND pull names
		MAKE_INTERFACES(
			{"uB", "USB2 MicroB", 15, 16},
			{"A",  "USB3 Type-A", 12, 19},
		),
	},
	{ // Dual 3/4-pin SH, Barrel Jack, 6-pin
                0x84, // GP0=Low/GP1=High/GP2=None
                "2x3-4SH + 6p + BJ",
		MAKE_PINS(
                        // 4-pin SH (A)
                        { 3, "A_1"},
                        { 4, "A_2"},
                        { 6, "A_3"},
                        { 5, "A_4"},
			// 4-pin SH (B)
                        { 7, "B_1"},
                        { 8, "B_2"},
                        { 9, "B_3"},
                        {10, "B_4"},
			// 3-pin SH (C)
                        {28, "C_1"},
                        {27, "C_2"},
                        {26, "C_3"},
			// 3-pin SH (D)
                        {22, "D_1"},
                        {20, "D_2"},
                        {21, "D_3"},
			// 6-pin Header (E)
                        {12, "E_1"},
			{16, "E_2"},
			{11, "E_3"},
			{17, "E_4"},
			{19, "E_5"},
			{18, "E_6"},
			// Barrel Jack (F)
			{15, "F_TIP"},
			{13, "F_DETECT"},
			{14, "F_SLEAVE"},
                ),
		MAKE_PULLS_NONE(),
		MAKE_PULLNAMES_NONE(),
		NULL, NULL, // VBUS/GND pull names
		MAKE_INTERFACES(
                        [0] = {"A", "4-pin SH", 0xFF, 0xFF},
                        [1] = {"B", "4-pin SH", 0xFF, 0xFF},
			[2] = {"C", "3-pin SH", 0xFF, 0xFF},
			[3] = {"D", "3-pin SH", 0xFF, 0xFF},
			[4] = {"E", "6-pin Header", 0xFF, 0xFF},
			[5] = {"F", "Barrel Jack", 0xFF, 0xFF},
                ),
        },
        { // 8P8C Ethernet/RJ45/RJ11
                0x48, // GP0=High/GP1=Low/GP2=None
                "8P8C",
                MAKE_PINS(
                        {17, "P1"},  // 10
                        {18, "P2"},  // 9
                        {19, "P3"}, // 8
                        {20, "P4"},  // 7
                        {11, "P5"},  // 6
                        {12, "P6"}, // 5
                        {13, "P7"},  // 4
                        {14, "P8"},   // 3
                ),
                MAKE_PULLS_NONE(),
		MAKE_PULLNAMES_NONE(),
                NULL, NULL, // VBUS/GND pull names
                MAKE_INTERFACES(
                        [0] = {"", "8P8C", 15, 16},
                ),
        },
};

#endif
