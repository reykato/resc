/*
	Copyright 2016 Benjamin Vedder	benjamin@vedder.se
	Copyright 2025 Tyler Lindsay

	This file is part of the VESC firmware.

	The VESC firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The VESC firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "conf_general.h"
#ifdef HW_HAS_6EDL7141

#include "edl7141.h"
#include "ch.h"
#include "hal.h"
#include "stm32f4xx_conf.h"
#include "utils_math.h"
#include "terminal.h"
#include "commands.h"
#include <string.h>
#include <stdio.h>
#include "mc_interface.h"

// Private functions
static void spi_begin(void);
static void spi_end(void);
static void spi_delay(void);
static void terminal_read_reg(int argc, const char **argv);
static void terminal_write_reg(int argc, const char **argv);
static void terminal_print_faults(int argc, const char **argv);

// Private variables
static char m_fault_print_buffer[120];
static mutex_t m_spi_mutex;

void edl7141_init(void) {
	chMtxObjectInit(&m_spi_mutex);

	palSetPadMode(EDL7141_MISO_GPIO, EDL7141_MISO_PIN, PAL_MODE_INPUT);
	palSetPadMode(EDL7141_SCK_GPIO, EDL7141_SCK_PIN, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
	palSetPadMode(EDL7141_CS_GPIO, EDL7141_CS_PIN, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
	palSetPadMode(EDL7141_MOSI_GPIO, EDL7141_MOSI_PIN, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
	palSetPad(EDL7141_MOSI_GPIO, EDL7141_MOSI_PIN);
#ifdef EDL7141_CS_GPIO2
	palSetPadMode(EDL7141_CS_GPIO2, EDL7141_CS_PIN2, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
#endif

	chThdSleepMilliseconds(100);

	terminal_register_command_callback(
			"6edl7141_read_reg",
			"Read a register from the 6EDL7141 and print it.",
			"[reg]",
			terminal_read_reg);

	terminal_register_command_callback(
			"6edl7141_write_reg",
			"Write to a 6EDL7141 register.",
			"[reg] [hexvalue]",
			terminal_write_reg);

	terminal_register_command_callback(
			"6edl7141_print_faults",
			"Print all current 6EDL7141 faults.",
			0,
			terminal_print_faults);
}

/**
 * Read the fault codes of the 6EDL7141.
 *
 * @return
 * The fault codes, where each bit represents a specific fault:
 *
 * b12: OTP_FLT   - OTP (One Time Programmable) memory fault status
 *                  0: No fault has occurred
 *                  1: Fault has occurred
 *
 * b11: WD_FLT    - Watchdog fault status
 *                  0: No fault has occurred
 *                  1: Fault has occurred
 *
 * b10: RLOCK_FLT - Locked rotor fault status (using hall sensors)
 *                  0: No fault has occurred
 *                  1: Fault has occurred
 *
 * b9:  OTW_FLT   - Over-temperature warning status
 *                  0: No warning signal has occurred
 *                  1: Warning signal has occurred
 *
 * b8:  OTS_FLT   - Over-temperature shutdown fault status
 *                  0: No fault has occurred
 *                  1: Fault has occurred
 *
 * b7:  BK_OCP_FLT - Buck over-current protection fault status
 *                  0: No fault has occurred
 *                  1: Fault has occurred
 *
 * b6:  DVDD_OV_FLT - DVDD over-voltage lockout fault status
 *                  0: No fault has occurred
 *                  1: Fault has occurred
 *
 * b5:  DVDD_UV_FLT - DVDD under-voltage lockout fault status
 *                  0: No fault has occurred
 *                  1: Fault has occurred
 *
 * b4:  DVDD_OCP_FLT - DVDD linear regulator over-current protection status
 *                  0: No fault has occurred
 *                  1: Fault has occurred
 *
 * b3:  CP_FLT      - Charge pump fault status (high & low side combined)
 *                  0: No fault has occurred
 *                  1: Fault has occurred
 *
 * b2..0: CS_OCP_FLT - Current sense amplifier over-current protection fault
 *                  Bitwise per phase:
 *                      b0: Phase C  0=No fault, 1=Fault
 *                      b1: Phase B  0=No fault, 1=Fault
 *                      b2: Phase A  0=No fault, 1=Fault
 *
 * 15:13: Reserved (always reads 0)
 */
int edl7141_read_faults(void) {
	return edl7141_read_reg(0);
}

/**
 * Reset all latched and non-latched faults.
 */
void edl7141_reset_faults(void) {
	int data = 3;
	edl7141_write_reg(10, data);
}

char* edl7141_faults_to_string(int faults) {
    if (faults == 0) {
        strcpy(m_fault_print_buffer, "No 6EDL7141 faults");
    } else {
        strcpy(m_fault_print_buffer, "|");

        if (faults & EDL7141_FAULT_CS_OCP_A) {
            strcat(m_fault_print_buffer, " CS_OCP_A |");
        }
        if (faults & EDL7141_FAULT_CS_OCP_B) {
            strcat(m_fault_print_buffer, " CS_OCP_B |");
        }
        if (faults & EDL7141_FAULT_CS_OCP_C) {
            strcat(m_fault_print_buffer, " CS_OCP_C |");
        }
        if (faults & EDL7141_FAULT_CP) {
            strcat(m_fault_print_buffer, " CP |");
        }
        if (faults & EDL7141_FAULT_DVDD_OCP) {
            strcat(m_fault_print_buffer, " DVDD_OCP |");
        }
        if (faults & EDL7141_FAULT_DVDD_UV) {
            strcat(m_fault_print_buffer, " DVDD_UV |");
        }
        if (faults & EDL7141_FAULT_DVDD_OV) {
            strcat(m_fault_print_buffer, " DVDD_OV |");
        }
        if (faults & EDL7141_FAULT_BK_OCP) {
            strcat(m_fault_print_buffer, " BK_OCP |");
        }
        if (faults & EDL7141_FAULT_OTS) {
            strcat(m_fault_print_buffer, " OTS |");
        }
        if (faults & EDL7141_FAULT_OTW) {
            strcat(m_fault_print_buffer, " OTW |");
        }
        if (faults & EDL7141_FAULT_RLOCK) {
            strcat(m_fault_print_buffer, " RLOCK |");
        }
        if (faults & EDL7141_FAULT_WD) {
            strcat(m_fault_print_buffer, " WD |");
        }
        if (faults & EDL7141_FAULT_OTP) {
            strcat(m_fault_print_buffer, " OTP |");
        }
    }

    return m_fault_print_buffer;
}

unsigned int edl7141_read_reg(int reg) {
	uint8_t cmd = (uint8_t)reg & 0x7F;
	uint16_t response = 0;

	chMtxLock(&m_spi_mutex);

	spi_begin();

	// Send the 8-bit command
	for (int i = 0; i < 8; i++) {
		if (cmd & 0x80) {
			palSetPad(EDL7141_MOSI_GPIO, EDL7141_MOSI_PIN);
		} else {
			palClearPad(EDL7141_MOSI_GPIO, EDL7141_MOSI_PIN);
		}
		cmd <<= 1;

		palSetPad(EDL7141_SCK_GPIO, EDL7141_SCK_PIN);
		spi_delay();
		palClearPad(EDL7141_SCK_GPIO, EDL7141_SCK_PIN);
		spi_delay();
	}

	// Receive the 16-bit response
	for (int i = 0; i < 16; i++) {
		palSetPad(EDL7141_SCK_GPIO, EDL7141_SCK_PIN);
		spi_delay();
		palClearPad(EDL7141_SCK_GPIO, EDL7141_SCK_PIN); // Falling edge: sample data here
		spi_delay();

		response <<= 1;
		if (palReadPad(EDL7141_MISO_GPIO, EDL7141_MISO_PIN)) {
			response |= 1;
		}
	}

	spi_end();

	chMtxUnlock(&m_spi_mutex);

	return response;
}

void edl7141_write_reg(int reg, int contents) {
	uint8_t cmd = (uint8_t)reg | 0x80;
	uint16_t data = (uint16_t)contents;

	chMtxLock(&m_spi_mutex);
	spi_begin();

	// Send the 8-bit command
	for (int i = 0; i < 8; i++) {
		if (cmd & 0x80) {
			palSetPad(EDL7141_MOSI_GPIO, EDL7141_MOSI_PIN);
		} else {
			palClearPad(EDL7141_MOSI_GPIO, EDL7141_MOSI_PIN);
		}
		cmd <<= 1;

		palSetPad(EDL7141_SCK_GPIO, EDL7141_SCK_PIN);
		spi_delay();
		palClearPad(EDL7141_SCK_GPIO, EDL7141_SCK_PIN);
		spi_delay();
	}

	// Send the 16-bit register data
	for (int i = 0; i < 16; i++) {
		if (data & 0x80) {
			palSetPad(EDL7141_MOSI_GPIO, EDL7141_MOSI_PIN);
		} else {
			palClearPad(EDL7141_MOSI_GPIO, EDL7141_MOSI_PIN);
		}
		data <<= 1;

		palSetPad(EDL7141_SCK_GPIO, EDL7141_SCK_PIN);
		spi_delay();
		palClearPad(EDL7141_SCK_GPIO, EDL7141_SCK_PIN);
		spi_delay();
	}

	spi_end();
	chMtxUnlock(&m_spi_mutex);
}

static void spi_begin(void) {
#ifdef EDL7141_CS_GPIO2
	if (mc_interface_motor_now() == 2) {
		palClearPad(EDL7141_CS_GPIO2, EDL7141_CS_PIN2);
	} else {
		palClearPad(EDL7141_CS_GPIO, EDL7141_CS_PIN);
	}
#else
	palClearPad(EDL7141_CS_GPIO, EDL7141_CS_PIN);
#endif
}

static void spi_end(void) {
#ifdef EDL7141_CS_GPIO2
	if (mc_interface_motor_now() == 2) {
		palSetPad(EDL7141_CS_GPIO2, EDL7141_CS_PIN2);
	} else {
		palSetPad(EDL7141_CS_GPIO, EDL7141_CS_PIN);
	}
#else
	palSetPad(EDL7141_CS_GPIO, EDL7141_CS_PIN);
#endif
}

static void spi_delay(void) {
	for (volatile int i = 0;i < 10;i++) {
		__NOP();
	}
}

static void terminal_read_reg(int argc, const char **argv) {
	if (argc == 2) {
		int reg = -1;
		sscanf(argv[1], "%d", &reg);

		if (reg >= 0) {
			unsigned int res = edl7141_read_reg(reg);
			char bl[9];
			char bh[9];

			utils_byte_to_binary((res >> 8) & 0xFF, bh);
			utils_byte_to_binary(res & 0xFF, bl);

			commands_printf("Reg 0x%02x: %s %s (0x%04x)\n", reg, bh, bl, res);
		} else {
			commands_printf("Invalid argument(s).\n");
		}
	} else {
		commands_printf("This command requires one argument.\n");
	}
}

static void terminal_write_reg(int argc, const char **argv) {
	if (argc == 3) {
		int reg = -1;
		int val = -1;
		sscanf(argv[1], "%d", &reg);
		sscanf(argv[2], "%x", &val);

		if (reg >= 0 && val >= 0) {
			edl7141_write_reg(reg, val);
			unsigned int res = edl7141_read_reg(reg);
			char bl[9];
			char bh[9];

			utils_byte_to_binary((res >> 8) & 0xFF, bh);
			utils_byte_to_binary(res & 0xFF, bl);

			commands_printf("New reg value 0x%02x: %s %s (0x%04x)\n", reg, bh, bl, res);
		} else {
			commands_printf("Invalid argument(s).\n");
		}
	} else {
		commands_printf("This command requires two arguments.\n");
	}
}

static void terminal_print_faults(int argc, const char **argv) {
	(void)argc;
	(void)argv;
	commands_printf(edl7141_faults_to_string(edl7141_read_faults()));
}

#endif
