/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2026 Chris Burton
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "bsp/board.h"
#include "tusb.h"

#include "pico/bootrom.h"
#include "pico/unique_id.h"
#include "pico/stdlib.h"
#include "pico/version.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"

#include "version.h"
#include "testers.h"

#define PULL_NONE 0
#define PULL_UP 1
#define PULL_DOWN 2
#define CLI_BUFFER_SIZE 256
#define OUTPUT_BUFFER_SIZE 1024 /* Plenty of space for long pin names even on RP235x devices */
#define ERR_PIN_NOT_FOUND 0xFF

// I2C Port (for I2C defined testers)
#define I2C_PORT     i2c0
#define I2C_SDA_PIN  0
#define I2C_SCL_PIN  1
#define I2C_BAUDRATE 100000

// I2C EEPROM (24C32)
#define EEPROM_ADDR 0x50
#define EEPROM_SIZE 4096
#define EEPROM_MAGIC "CABLETESTER"

const struct Tester *active_tester;
char usb_serial[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
char usb_product[64];
uint8_t config; // Config pin pull status
uint8_t tid; // tester id
char cli_buffer[CLI_BUFFER_SIZE];
uint16_t cli_buffer_used;
char buf[OUTPUT_BUFFER_SIZE+4];
char buf2[OUTPUT_BUFFER_SIZE];
uint32_t delay = 50;

// We're not using core1 so use SCRATCH_X for the I2C EEPROM cache
uint8_t __in_scratch_x("i2c_eeprom_cache") i2c_cache[4096];
static struct Tester i2c_tester;
static void* i2c_backing_memory = NULL; // Used to store pointers to data

// Busy wait by running tud_task()
static void sleep_ms_safe(uint32_t ms) {
	uint64_t end = time_us_64() + ((uint64_t)ms * 1000);
	while(time_us_64() < end) tud_task();
}

// Send string to the USB CDC port
static void usbprint(char buf[]) {
	// Get the length of the string bounded by the size of buf if NULL is somehow missing
	uint32_t len = strnlen(buf, OUTPUT_BUFFER_SIZE+4);
	uint32_t cnt = 0;

	// Chunked write
	while(cnt < len) {
		// If CDC isn't connected there's no point trying to send anything
		if(!tud_cdc_connected()) return;
		uint32_t count = tud_cdc_n_write(0, buf + cnt, len - cnt);
		cnt += count;
		tud_cdc_n_write_flush(0);
		if(cnt < len) tud_task();
  	}
}

// Reset all tester pins to input with specified pull 
static void resetpins(uint8_t type) {
	// Reset tester GPIO pins
	for(uint8_t i=0;i<active_tester->n_pins;i++) { // PINS
		gpio_init(active_tester->pins[i].gpio); // Reset to input
		gpio_set_drive_strength(active_tester->pins[i].gpio, GPIO_DRIVE_STRENGTH_12MA);
		if(type==PULL_NONE) gpio_disable_pulls(active_tester->pins[i].gpio); // No pulls
		if(type==PULL_UP) gpio_pull_up(active_tester->pins[i].gpio); // Pull up
		if(type==PULL_DOWN) gpio_pull_down(active_tester->pins[i].gpio); // Pull down
	}
	// Reset the pull test control pins
	for(uint8_t i=0;i<active_tester->n_pulls;i++) { // PULLS
		gpio_init(active_tester->pulls[i].control); // Reset to input
		gpio_set_drive_strength(active_tester->pulls[i].control, GPIO_DRIVE_STRENGTH_12MA);
		gpio_disable_pulls(active_tester->pulls[i].control); // Remove any pulls
	}
}

// Returns name of the pull-up/down based on the resistances detect being within bounds of a pull
static const char* pullname(uint8_t id, uint32_t pd, uint32_t pu) {

	for(uint8_t i = 0; i < active_tester->n_pullnames; i++) {
		if((active_tester->pullnames[i].id == id) &&
			(pd >= active_tester->pullnames[i].pdmin) &&
			(pd <= active_tester->pullnames[i].pdmax) &&
			(pu >= active_tester->pullnames[i].pumin) &&
			(pu <= active_tester->pullnames[i].pumax)) {
				return active_tester->pullnames[i].name;
		}
	}

	return "Unknown";
}

// Read all pin states (except the one named "setpin" if passed)
static void readpins(char * setpin) {
	uint8_t ret; // Pin status
	uint8_t lowpins = 0; // Count of how many pins are low
	char separator = ':';
	uint16_t len = 0;

	if(setpin!=NULL) {
		snprintf(buf2, sizeof(buf2), ":%s", setpin);
		separator = ',';
	} else {
		buf2[0] = '\0';
	}

	for(uint8_t i=0;i<active_tester->n_pins;i++) { // PINS
		ret = gpio_get(active_tester->pins[i].gpio);
		snprintf(buf, sizeof(buf), "%d", ret);
		if(!ret) {
			if(setpin==NULL||strcmp(setpin, active_tester->pins[i].name)!=0) {
				len = strnlen(buf2, sizeof(buf2));
				snprintf(buf2+len, sizeof(buf2)-len, "%c%s", separator, active_tester->pins[i].name);
			}
			lowpins++;
			separator=',';
		}
		usbprint(buf);
	}
	snprintf(buf, sizeof(buf), ":%d%s", lowpins, buf2);
	usbprint(buf);
}

// Returns GPIO pin number from a pin name (e.g. "VBUS" -> 12)
uint8_t getgpiobyname(char *name) {
	for(uint8_t i=0;i<active_tester->n_pins;i++) {
		if(strcmp(name, active_tester->pins[i].name)==0) return active_tester->pins[i].gpio;
	}
	return ERR_PIN_NOT_FOUND;
}

// Command Line Interface - reads and actions commands from USB CDC serial
static void cli(void) {
	uint8_t got_cmd = false;
	uint8_t tmp0; 
	int32_t ret;
	uint16_t result;
	uint16_t result3;
	uint32_t result2;
	uint32_t pu;
	uint32_t pd;

	// Read any available data until we see a newline
	while(tud_cdc_n_available(0)&&!got_cmd) {
		// If we've reached the end of the buffer reset it
		if(cli_buffer_used==(CLI_BUFFER_SIZE-1)) cli_buffer_used=0;

		ret = tud_cdc_n_read_char(0);
		if(ret!=-1) {
			if(ret=='\n' || ret=='\r') {
				got_cmd = true;
			} else {
				cli_buffer[cli_buffer_used] = ret;
				cli_buffer[++cli_buffer_used] = '\0';
			}
		}
	}

	// Only try to parse the CLI buffer if we have a string with a newline
	if(got_cmd) {
		if(strlen(cli_buffer)>0) { // Make sure there's a string to check
			if(strcmp("IDENTIFY", cli_buffer)==0) { // IDENTIFY
				snprintf(buf, sizeof(buf), "TYPE:%s\r\n", active_tester->name );
				usbprint(buf);

				snprintf(buf, sizeof(buf), "PINS:%d", active_tester->n_pins );
				usbprint(buf);
				tmp0 = 0;
				for(uint8_t i=0;i<active_tester->n_pins;i++) { // PINS
					buf[0] = (tmp0==0)?':':',';
					snprintf(buf+1, sizeof(buf)-1, "%s", active_tester->pins[i].name );
					usbprint(buf);
					tmp0++;
				}
				snprintf(buf, sizeof(buf), "\r\n" );
				usbprint(buf);

				snprintf(buf, sizeof(buf), "PULLS:%d", active_tester->n_pulls );
				usbprint(buf);
				tmp0=0;
				for(uint8_t i=0;i<active_tester->n_pulls;i++) { // PULLS
					buf[0] = (tmp0==0)?':':',';
					snprintf(buf+1, sizeof(buf)-1, "%s", active_tester->pulls[i].name);
					usbprint(buf);
					tmp0++;
				}
				snprintf(buf, sizeof(buf), "\r\n");
				usbprint(buf);

				snprintf(buf, sizeof(buf), "INTERFACES:%d", active_tester->n_interfaces );
				usbprint(buf);
				tmp0=0;
				for(uint8_t i=0;i<active_tester->n_interfaces;i++) { // INTERFACES
					buf[0] = (tmp0==0)?':':',';
					snprintf(buf+1, sizeof(buf)-1, "%s@%s", active_tester->interfaces[i].prefix, active_tester->interfaces[i].name );
					usbprint(buf);
					tmp0++;
				}

				snprintf(buf, sizeof(buf), "\r\nGIT_HASH:" CABLE_TESTER_GIT_HASH );
				usbprint(buf);

				snprintf(buf, sizeof(buf), "\r\nGIT_DATE:" CABLE_TESTER_GIT_DATE );
				usbprint(buf);

				snprintf(buf, sizeof(buf), "\r\nBUILD_DATE:" CABLE_TESTER_BUILD_DATE );
				usbprint(buf);

				snprintf(buf, sizeof(buf), "\r\nPICO-SDK:" PICO_SDK_VERSION_STRING );
				usbprint(buf);

				snprintf(buf, sizeof(buf), "\r\nSERIAL:%s", usb_serial);
				usbprint(buf);

			} else if(strcmp("GETID", cli_buffer)==0) { // GETID
				snprintf(buf, sizeof(buf), "GETID:0x%02X", config);
				usbprint(buf);
			} else if(strcmp("GETDELAY", cli_buffer)==0) { // GETDELAY
				snprintf(buf, sizeof(buf), "DELAY:%lu", delay);
				usbprint(buf);
			} else if(strncmp("SETDELAY:", cli_buffer, strlen("SETDELAY:"))==0) { // SETDELAY:<INT>
				ret = sscanf(cli_buffer+strlen("SETDELAY:"), "%lu", &result2);
				if(ret==1) { // Parsed 1 option
					delay = (result2>10000) ? 10000 : result2; // Max 10 second delay
				} else {
					snprintf(buf, sizeof(buf), "ERROR:Invalid delay");
					usbprint(buf);
				}
			} else if(strcmp("RESETN", cli_buffer)==0) { // RESETN
				resetpins(PULL_NONE);
			} else if(strcmp("RESETU", cli_buffer)==0) { // RESETU
				resetpins(PULL_UP);
			} else if(strcmp("RESETD", cli_buffer)==0) { // RESETD
				resetpins(PULL_DOWN);
			} else if(strcmp("READ", cli_buffer)==0) { // READ
				readpins(NULL);
			} else if(strncmp("LREAD:", cli_buffer, 6) == 0) { // LREAD:<NAME>
				uint8_t gpio = getgpiobyname((char*)cli_buffer + 6);
				if(gpio != ERR_PIN_NOT_FOUND) {
					resetpins(PULL_UP); // reset all pins to pull up
					gpio_set_dir(gpio, 1); // set specific pin to output
					gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_12MA);
					gpio_put(gpio, 0); // set specific pin low
					sleep_ms_safe(delay);
					readpins((char*)cli_buffer + 6);
				} else {
					snprintf(buf, sizeof(buf), "ERROR:Pin not found.");
					usbprint(buf);
				}
			} else if(strncmp("SETL:", cli_buffer, 5) == 0) { // SETL:<NAME>
				uint8_t gpio = getgpiobyname((char*)cli_buffer + 5);
				if(gpio != ERR_PIN_NOT_FOUND) {
					gpio_set_dir(gpio, 1); // set output
					gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_12MA);
					gpio_put(gpio, 0); // set low
				} else {
					snprintf(buf, sizeof(buf), "ERROR:Pin not found.");
					usbprint(buf);
				}
			} else if(strncmp("SETH:", cli_buffer, 5) == 0) { // SETH:<NAME>
				uint8_t gpio = getgpiobyname((char*)cli_buffer + 5);
				if(gpio != ERR_PIN_NOT_FOUND) {
					gpio_set_dir(gpio, 1); // set output
					gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_12MA);
					gpio_put(gpio, 1); // set high
				} else {
					snprintf(buf, sizeof(buf), "ERROR:Pin not found.");
					usbprint(buf);
				}
			} else if(strncmp("SETN:", cli_buffer, 5) == 0) { // SETN:<NAME>
				uint8_t gpio = getgpiobyname((char*)cli_buffer + 5);
				if(gpio != ERR_PIN_NOT_FOUND) {
					gpio_set_dir(gpio, 0); // set input
					gpio_disable_pulls(gpio); // disable pulls
				} else {
					snprintf(buf, sizeof(buf), "ERROR:Pin not found.");
					usbprint(buf);
				}
			} else if(strncmp("PULLN:", cli_buffer, 6) == 0) { // PULLN:<NAME>
				uint8_t gpio = getgpiobyname((char*)cli_buffer + 6);
				if(gpio != ERR_PIN_NOT_FOUND) {
					gpio_disable_pulls(gpio); // disable pulls
				} else {
					snprintf(buf, sizeof(buf), "ERROR:Pin not found.");
					usbprint(buf);
				}
			} else if(strncmp("PULLU:", cli_buffer, 6) == 0) { // PULLU:<NAME>
				uint8_t gpio = getgpiobyname((char*)cli_buffer + 6);
				if(gpio != ERR_PIN_NOT_FOUND) {
					gpio_pull_up(gpio);
				} else {
					snprintf(buf, sizeof(buf), "ERROR:Pin not found.");
					usbprint(buf);
				}
			} else if(strncmp("PULLD:", cli_buffer, 6) == 0) { // PULLD:<NAME>
				uint8_t gpio = getgpiobyname((char*)cli_buffer + 6);
				if(gpio != ERR_PIN_NOT_FOUND) {
					gpio_pull_down(gpio); // pull down
				} else {
					snprintf(buf, sizeof(buf), "ERROR:Pin not found.");
					usbprint(buf);
				}
			} else if(strcmp("GETPULLS", cli_buffer)==0) { // GETPULLS
				for(uint8_t i=0;i<active_tester->n_pulls;i++) { // PULLS
					// Reset all pins
					resetpins(PULL_NONE);
					uint32_t r_ref = active_tester->pulls[i].r; // Resistance on-board tester

					// Setup ADC
					tmp0 = getgpiobyname((char*)active_tester->pulls[i].name);
					if(tmp0>=ADC_BASE_PIN && tmp0<(ADC_BASE_PIN+NUM_ADC_CHANNELS-1)) {
						adc_gpio_init(tmp0);
						adc_select_input(tmp0-ADC_BASE_PIN);
					} else {
						snprintf(buf, sizeof(buf), "ERROR: ADC not available on pin %s", (char*)active_tester->pulls[i].name);
						usbprint(buf);
						break;
					}

					//// Test Pull Down

					// Set GND pin low
					tmp0 = getgpiobyname((char*)active_tester->pull_low);
					if(tmp0<NUM_BANK0_GPIOS) {
						gpio_set_dir(tmp0, 1); // set output
						gpio_put(tmp0, 0); // set low
					} else {
						snprintf(buf, sizeof(buf), "ERROR: GND pin %s not found", active_tester->pull_low);
						usbprint(buf);
						break;
					}

					// Set Control pin high
					tmp0 = active_tester->pulls[i].control;
					if(tmp0<NUM_BANK0_GPIOS) {
						gpio_set_dir(tmp0, 1); // set output
						gpio_put(tmp0, 1); // set high
					} else {
						snprintf(buf, sizeof(buf), "ERROR: Control pin %u not valid", tmp0);
						usbprint(buf);
						break;
					}

					sleep_ms_safe(delay);

					result = adc_read();

					if(result>=4096) {
						pd = 0xFFFFFFFF;
					} else {
						pd = (r_ref * result ) / (4096 - result);
					}

					// Reset GND pin
					tmp0 = getgpiobyname((char*)active_tester->pull_low);
					if(tmp0<NUM_BANK0_GPIOS) {
						gpio_set_dir(tmp0, 0); // set input
					}

					//// Test Pull Up

					// Set VCC
					tmp0 = getgpiobyname((char*)active_tester->pull_high);
					if(tmp0<NUM_BANK0_GPIOS) {
						gpio_set_dir(tmp0, 1); // set output
						gpio_put(tmp0, 1); // set high
					} else {
						snprintf(buf, sizeof(buf), "ERROR: VCC pin %s not found", active_tester->pull_high);
						usbprint(buf);
						break;
					}

					// Set Control pin low
					tmp0 = active_tester->pulls[i].control;
					if(tmp0<NUM_BANK0_GPIOS) {
						//gpio_set_dir(tmp0, 1); // set output // already set above
						gpio_put(tmp0, 0); // set low
					}

					sleep_ms_safe(delay);

					result3 = adc_read();

					if(result3==0) {
						pu = 0xFFFFFFFF;
					} else {
						pu = (r_ref * ( 4096 - result3) ) / result3;
					}

					// Reset VBUS pin
					tmp0 = getgpiobyname((char*)active_tester->pull_high);
					if(tmp0<NUM_BANK0_GPIOS) {
						gpio_set_dir(tmp0, 0);
					}

					// Reset control pin
					gpio_init(active_tester->pulls[i].control);

					snprintf(buf, sizeof(buf), "%s:%s:%lu:%lu\r\n", (char*)active_tester->pulls[i].name, pullname(active_tester->pulls[i].id, pd, pu), pd, pu );
					usbprint(buf);
				}
			} else if(strcmp("UPTIME", cli_buffer)==0) { // UPTIME
				snprintf(buf, sizeof(buf), "%llu", time_us_64());
				usbprint(buf);
			} else { // Unknown command
				snprintf(buf, sizeof(buf), "ERROR:Unknown Command");
				usbprint(buf);
			}
	
			snprintf(buf, sizeof(buf), "\r\n%s:%s\r\n", "DONE", cli_buffer); // DONE
			usbprint(buf);
		}
		// Reset cli buffer
		cli_buffer_used=0;
		cli_buffer[0] = '\0';

	}
}

// Copy all data from I2C 24C32 EEPROM into "i2c_cache"
bool load_i2c_eeprom_hhn(void) {
	int ret;
	uint8_t mem_addr[2] = {0x00, 0x00};
	bool ok = false;

	i2c_init(I2C_PORT, I2C_BAUDRATE);
	gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);

	ret = i2c_write_blocking(I2C_PORT, EEPROM_ADDR, mem_addr, 2, true);

	if(ret == 2) {
		// Read MAGIC from EEPROM
		ret = i2c_read_blocking(I2C_PORT, EEPROM_ADDR, i2c_cache, strlen(EEPROM_MAGIC), false);

		// If the MAGIC is OK read the rest of the EEPROM
		if(ret == strlen(EEPROM_MAGIC) && memcmp(i2c_cache, EEPROM_MAGIC, strlen(EEPROM_MAGIC)) == 0) {
			ret = i2c_read_blocking(I2C_PORT, EEPROM_ADDR, i2c_cache + strlen(EEPROM_MAGIC), EEPROM_SIZE - strlen(EEPROM_MAGIC), false);

			// Check the read was complete
			if(ret==(EEPROM_SIZE-strlen(EEPROM_MAGIC))) ok = true;
		}
	}

	// Set the pins back to original config (probably not needed)
	i2c_deinit(I2C_PORT);
	gpio_init(I2C_SDA_PIN);
	gpio_init(I2C_SCL_PIN);
	gpio_set_dir(I2C_SDA_PIN, 0);
	gpio_set_dir(I2C_SCL_PIN, 0);
	gpio_disable_pulls(I2C_SDA_PIN);
	gpio_disable_pulls(I2C_SCL_PIN);

	return ok;
}

// Parses EEPROM contents which has been copied into i2c_cache
bool parse_eeprom_config(void) {
	uint16_t offset = 0;
	size_t pins_size;
	size_t pulls_size;
	size_t interfaces_size;
	size_t pullnames_size;

	// Recheck MAGIC (checked in load_i2c_eeprom_hhn)
	if(memcmp(&i2c_cache[offset], EEPROM_MAGIC, strlen(EEPROM_MAGIC)) != 0) return false;
	offset += strlen(EEPROM_MAGIC);

	// Check version number
	if(i2c_cache[offset++] != 0x01) return false;

	// Set config id
	i2c_tester.config_id = 0b00001100;

	// Set counts (4 bytes)
	i2c_tester.n_pins = i2c_cache[offset++];
	i2c_tester.n_pulls = i2c_cache[offset++];
	i2c_tester.n_pullnames = i2c_cache[offset++];
	i2c_tester.n_interfaces = i2c_cache[offset++];

	// Get size of dynamic config data we need to allocate memory for
	// Structs are mostly just pointers into i2c_cache for the strings and counts
	pins_size = i2c_tester.n_pins * sizeof(struct Pin);
	pulls_size = i2c_tester.n_pulls * sizeof(struct Pull);
	pullnames_size = i2c_tester.n_pullnames * sizeof(struct PullName);
	interfaces_size = i2c_tester.n_interfaces * sizeof(struct Interface);

	// Allocate memory for structs
	i2c_backing_memory = malloc(pins_size + pulls_size + pullnames_size + interfaces_size);
	if(!i2c_backing_memory) return false; // Couldn't allocate the memory!

	// Create pointers into the chunk of memory for struct data
	struct Pin *dyn_pins = (struct Pin *)i2c_backing_memory;
	struct Pull *dyn_pulls = (struct Pull *)((uint8_t*)i2c_backing_memory + pins_size);
	struct PullName *dyn_pullnames = (struct PullName *)((uint8_t*)i2c_backing_memory + pins_size + pulls_size);
	struct Interface *dyn_interfaces = (struct Interface *)((uint8_t*)i2c_backing_memory + pins_size + pulls_size + pullnames_size);

	// Setup pointers to locations of pin/pull/interface data
	i2c_tester.pins = dyn_pins;
	i2c_tester.pulls = dyn_pulls;
	i2c_tester.pullnames = dyn_pullnames;
	i2c_tester.interfaces = dyn_interfaces;

	// Setup string variables (pointing to the data in i2c_cache)
	i2c_tester.name = (const char*)&i2c_cache[offset];
	offset += strlen(i2c_tester.name) + 1;

	i2c_tester.pull_high = (const char*)&i2c_cache[offset];
	offset += strlen(i2c_tester.pull_high) + 1;

	i2c_tester.pull_low = (const char*)&i2c_cache[offset];
	offset += strlen(i2c_tester.pull_low) + 1;

	// Pins - store GPIO number and pointer to name
	for(uint8_t i = 0; i < i2c_tester.n_pins; i++) {
		dyn_pins[i].gpio = i2c_cache[offset++];
		dyn_pins[i].name = (const char*)&i2c_cache[offset];
		offset += strlen(dyn_pins[i].name) + 1;
	}

	// Pulls - store control GPIO number, pull resistor value and pointer to name string
	for(uint8_t i = 0; i < i2c_tester.n_pulls; i++) {
		dyn_pulls[i].control = i2c_cache[offset++];
		dyn_pulls[i].id = i2c_cache[offset++];
		// using memcpy should avoid any issues with alignment?
		memcpy(&dyn_pulls[i].r, &i2c_cache[offset], 4);
		offset += 4;
		dyn_pulls[i].name = (const char*)&i2c_cache[offset];
		offset += strlen(dyn_pulls[i].name) + 1;
	}

	// Pullnames - store ID, min/max pull up/down resistor values and pointer to pullname string
	for(uint8_t i = 0; i < i2c_tester.n_pullnames; i++) {
		dyn_pullnames[i].id = i2c_cache[offset++];
		memcpy(&dyn_pullnames[i].pdmin, &i2c_cache[offset], 4); offset += 4;
		memcpy(&dyn_pullnames[i].pdmax, &i2c_cache[offset], 4); offset += 4;
		memcpy(&dyn_pullnames[i].pumin, &i2c_cache[offset], 4); offset += 4;
		memcpy(&dyn_pullnames[i].pumax, &i2c_cache[offset], 4); offset += 4;
		dyn_pullnames[i].name = (const char*)&i2c_cache[offset];
		offset += strlen(dyn_pullnames[i].name) + 1;
	}

	// Interfaces - store detect GPIO pins and pointers to prefix and name strings
	for(uint8_t i = 0; i < i2c_tester.n_interfaces; i++) {
		dyn_interfaces[i].detect0 = i2c_cache[offset++];
		dyn_interfaces[i].detect1 = i2c_cache[offset++];
		dyn_interfaces[i].prefix = (const char*)&i2c_cache[offset];
		offset += strlen(dyn_interfaces[i].prefix) + 1;
		dyn_interfaces[i].name = (const char*)&i2c_cache[offset];
		offset += strlen(dyn_interfaces[i].name) + 1;
	}

	return true;	
}

int main(void) {
	uint8_t tester_found = false;
	int16_t tmp = -1;

	// Setup USB serial number from EEPROMs unique ID
	pico_get_unique_board_id_string(usb_serial, sizeof(usb_serial));

	// Setup ADC
	adc_init();

	// Detect Cable Tester model from GP0/GP1/GP2 ties
	// config=0bDDD0UUU0 D=1 when pin is set low U=1 when pin is set high (LSB)

	// Init config GPIO pins
	gpio_init(0);
	gpio_init(1);
	gpio_init(2);

	// Pull them all high
	gpio_pull_up(0);
	gpio_pull_up(1);
	gpio_pull_up(2);

	// Settle
	sleep_ms(delay);

	// If the pin is still low it's tied low
	if(gpio_get(0)==0) config = config|0x01;
	config = config<<1;
	if(gpio_get(1)==0) config = config|0x01;
	config = config<<1;
	if(gpio_get(2)==0) config = config|0x01;
	config = config<<1;
	config = config<<1;

	// Pull the pins low
	gpio_pull_down(0);
	gpio_pull_down(1);
	gpio_pull_down(2);

	// Settle
	sleep_ms(delay);

	// If the pin is still high it's tied high
	if(gpio_get(0)==1) config = config|0x01;
	config = config<<1;
	if(gpio_get(1)==1) config = config|0x01;
	config = config<<1;
	if(gpio_get(2)==1) config = config|0x01;
	config = config<<1;

	// Reset pulls
	gpio_disable_pulls(0);
	gpio_disable_pulls(1);
	gpio_disable_pulls(2);

	// Was an I2C config detected?
	if(config==0b00001100) { // I2C config (GP0=HIGH, GP1=HIGH, GP2=NONE)
		if(load_i2c_eeprom_hhn()) { // Read all data from 24C32 I2C EEPROM
			if(parse_eeprom_config()) { // Parse memory into tester structs
				active_tester = &i2c_tester;
				tester_found = true;
			}
		}
	} else { // Check for a valid tester in testers.h based on pulls
		for(uint8_t i=0;i<N_TESTERS;i++) {
                	if(testers[i].config_id==config) {
				tester_found = true;
				tid = i;
				break;
                	}
			// If we found the fallback save it for later
			if(testers[i].config_id==0xFF) tmp = i;
        	}

		// If nothing matched but we've found the fallback entry use that
		if(!tester_found && tmp >=0) {
			tid = (uint8_t)tmp;
			tester_found = true;
		
		}

		// Set the USB Product name if a tester is found
		if(tester_found) {
			active_tester = &testers[tid];
		}
	} // end config pull check

	if(tester_found) {
		// Set name to what's given in the config
		snprintf(usb_product, 32, "Tester:%s", active_tester->name );

		// Disable default pull-down on interface detection pins
		for(uint8_t i = 0; i < active_tester->n_interfaces; i++) {
			gpio_disable_pulls(active_tester->interfaces[i].detect0);
			gpio_disable_pulls(active_tester->interfaces[i].detect1);
		}
	} else {
		// GP0/1/2 set in an unknown way and we don't have a fallback config to use
		snprintf(usb_product, 32, "Tester:ERROR Missing Fallback");
	}

	// Init USB device stack
	board_init();
	tud_init(BOARD_TUD_RHPORT);

	// Loop USB CDC Serial Command Line Interface
	while(true) {
		tud_task();
		if(tester_found) cli();
	}

	return 0;
}

// If serial speed is set to 1200 go into UF2 programming mode
void tud_cdc_line_coding_cb(__unused uint8_t itf, cdc_line_coding_t const* p_line_coding) {
 if(p_line_coding->bit_rate == 1200) {
  reset_usb_boot(0u, 0u);
 }
}
