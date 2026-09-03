# Cable Tester

The firmware provides a USB CDC Serial interface which when paired with control software (or a serial terminal program) running on a host computer can be used to map pin connections allowing cables to be detected and verified. It also supports limited testing of active modules and adaptor boards such as our USB Power Switch, USB-C/PWR Splitter, etc.

The firmware can currently be built for the Raspberry Pi Pico/Pico2 and Waveshare Core2350B. Once installed, it's designed to support multiple tester PCBs. The specific tester board type (USB-C/USB Type-A/etc.) is either detected by the firmware using GP0/GP1/GP2 tied high/low/none or via an external I2C EEPROM. This allows an MCU board (Pico/Pico2/Core2350B) to be switched between multiple testers without requiring any additional reflashing/configuration/etc. of the firmware.

> **Note:** Devices under test must be unpowered and consideration must be taken before connecting devices with active components.

## Building and Uploading

Install the compiler and associated tools:

```
sudo apt install cmake python3 build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```

and then you can compile the firmware using

```./make.sh```

You will then find the firmware for each board (Pico/Pico2/Core2350B) within the build directory called `cable_tester.uf2`, which you will need to copy to the Pico/Pico2/Core2350B when in bootloader mode (hold BOOT whilst plugging in).

## How it works

### Continuity testing (pin mapping)

When paired with controller software either standalone or by using any combination of testers together (i.e. USB-C, DB25, etc) it can determine how the cable is wired using a simple "pull, set and scan" method.

1. The firmware configures all testing GPIO pins as inputs and pulls them **high**.
2. The control software then drives one pin **low**.
3. All other pins (over all testers) are then scanned. Any pin which reads **low** is considered to be connected.
4. The pin under test is then reset and the control software moves onto the next pin until all pins (over all testers) have been tested.

This allows a map of all pin connections to be created.

### Pull-up and Pull-down Resistance Checks

To support extended testing of USB-C (and maybe others in the future) cables/adaptors with pull-up/down resistors can be checked by creating a voltage divider using a known reference resistance connected between a control pin and the pin connected to the device's pull.

1. All pins are reset to inputs with no pull-up/down.
2. The specified power pins are driven high/low as appropriate.
3. The test control pin (connected to the pin under test via a known resistor) is driven low/high creating a voltage divider.
4. The voltage on the pin under test is then read by the RP2 ADC, and the resistance is calculated.
5. Using a lookup table of expected pull-up/down resistances, either a name is returned or "Unknown", both along with the actual measured resistances.

## USB CDC Serial API

Communication with the cable tester firmware is via a USB CDC serial port. You can control it manually using a serial terminal program or via a controller script.

### Example command usage

When an error occurs the text "ERROR:" followed by an error message is sent. After all commands it also sends "DONE:" followed by the command.

**Example error response**

```
ERROR:Unknown Command
DONE:GarBaGe
```

```
IDENTIFY
```

Returns information about the tester and its capabilities.

**Example response**

```
TYPE:MicroB2 + Type-A3
PINS:15:uB_VBUS,uB_D+,uB_D-,uB_ID,uB_GND,uB_SHIELD,A_VBUS,A_D-,A_D+,A_GND,A_RX-,A_RX+,A_DRAIN,A_TX-,A_TX+
PULLS:0
INTERFACES:2:uB@USB2 MicroB,A@USB3 Type-A
GIT_HASH:d8de823-dirty
GIT_DATE:2026-08-26 19:25:39 +0000
BUILD_DATE:2026-08-28
PICO-SDK:2.3.0
SERIAL:E660D4A0A765562F
DONE:IDENTIFY
```

* **TYPE:** The name of the currently detected hardware tester profile.
* **PINS:** The total count of pins, followed by a comma-separated list of their names.
* **PULLS:** The total count of ADC-capable pull-test pins, followed by their names.
* **INTERFACES:** The total number of interfaces, their prefix and name.
* **GIT_HASH:**  Git hash of the built source (dirty or clean).
* **GIT_DATE:** Date of the git hash.
* **BUILD_DATE:** Date the firmware was compiled.
* **PICO-SDK:** Which version of Pico-SDK was used for the build.
* **SERIAL:** Unique hardware ID of the RP2040/RP2350 flash chip.

```
LREAD:<NAME>
```

Resets all pins to input with a pull-up and drives pin **NAME** **low**, waits for "delay" milliseconds and then reads the state of all pins.

**Example response**

```
011110111011111:3:uB_VBUS,uB_SHIELD,A_GND
DONE:LREAD:uB_VBUS
```

The first block of 0/1 is the current state of all pins in the order returned by the PINS parameter of the IDENTIFY command (0 for **low** and 1 for **high**). It then gives a count of the number of pins which read **low** followed by a comma separated list of their names.

```
READ
```

**Example response**

```
000000000000000:15:uB_VBUS,uB_D+,uB_D-,uB_ID,uB_GND,uB_SHIELD,A_VBUS,A_D-,A_D+,A_GND,A_RX-,A_RX+,A_DRAIN,A_TX-,A_TX+
DONE:READ
```

The first block of 0/1 is the current state of all pins in the order returned by the PINS parameter of the IDENTIFY command (0 for **low** and 1 for **high**), it then gives a count of the number of pins which read **low** followed by a comma separated list of their names. This command is normally used on a secondary tester where a pin has been set **low** on another tester to check which pins are connected (before testing you must use RESETU on all other testers to ensure the pull-ups are set and it's ready for testing with LREAD on the tester with the pin you're currently checking).

```
GETPULLS
```

Tests each PULLUP defined for the tester and returns the name (or Unknown if out of range), pull-down value and pull-up value in ohms.

**Example response**

```
CC2:Pu10k:1067894:9980
CC1:Pu10k:1851818:9980

DONE:GETPULLS
```

CC2 is the name of the pin with the pull-up/down and a Pull-up 10k resistor has been detected (1067894 ohms is the detected pull-down resistance and 9980 is the pull-up resistance).
CC1 has similarly been detected with a pull-up of 10k (pull-down detected 1851818 ohms and the pull-up 9980 ohms).

**Example response**

```
CC2:Pd5k1:5164:2265555
CC1:Pd5k1:5159:2145789

DONE:GETPULLS
```

CC2 is the name of the pin being tested, a Pull-down with 5.1K has been detected (5164 ohms pull-down and 2265555 ohms pull-up).
CC1 has similarly been detected with a pull-down of 5.1K (5159 ohms pull-down and 2145789 ohms pull-up).


```
GETID
```

Returns the configuration ID number set via GP0/1/2 in hex.

**Example response**

```
GETID:0x08
DONE:GETID
```

```
GETDELAY
```

Returns current delay in milliseconds.

**Example response**

```
DELAY:50
DONE:GETDELAY
```

```
SETDELAY:<INT>
```

Sets delay in milliseconds (maximum 10000, aka 10 seconds).

```
RESETN
```

Resets all pins to inputs with **no** pulls.

```
RESETU
```

Resets all pins to inputs with **pull-ups** enabled.

```
RESETD
```

Resets all pins to inputs with **pull-downs** enabled.

```
SETL:<NAME>
```

Sets a specific pin to an output and drives it **low**.

```
SETH:<NAME>
```

Sets a specific pin to an output and drives it **high**.

```
SETN:<NAME>
```

Sets a specific pin to an input with pulls disabled.

```
PULLN:<NAME>
```

Disables pulls on a specific pin.

```
PULLU:<NAME>
```

Enables the internal pull-up on a specific pin.

```
PULLD:<NAME>
```

Enables the internal pull-down on a specific pin.

```
UPTIME
```

Returns the tester's uptime in microseconds.

