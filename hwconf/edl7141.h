/*
	Copyright 2017 Benjamin Vedder	benjamin@vedder.se
	Copyright 2025 Tyler Lindsay

	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef HWCONF_EDL7141_H_
#define HWCONF_EDL7141_H_

#include "datatypes.h"

// Functions
void edl7141_init(void);
int edl7141_read_faults(void);
void edl7141_reset_faults(void);
char* edl7141_faults_to_string(int faults);
unsigned int edl7141_read_reg(int reg);
void edl7141_write_reg(int reg, int contents);

#define HW_RESET_DRV_FAULTS()		edl7141_reset_faults()

// 6EDL7141 Fault Defines
#define EDL7141_FAULT_CS_OCP_A      (1 << 0)  // Current sense OCP, Phase C bit 0
#define EDL7141_FAULT_CS_OCP_B      (1 << 1)  // Current sense OCP, Phase B bit 1
#define EDL7141_FAULT_CS_OCP_C      (1 << 2)  // Current sense OCP, Phase A bit 2
#define EDL7141_FAULT_CP            (1 << 3)  // Charge pump fault
#define EDL7141_FAULT_DVDD_OCP      (1 << 4)  // DVDD OCP fault
#define EDL7141_FAULT_DVDD_UV       (1 << 5)  // DVDD under-voltage
#define EDL7141_FAULT_DVDD_OV       (1 << 6)  // DVDD over-voltage
#define EDL7141_FAULT_BK_OCP        (1 << 7)  // Buck OCP fault
#define EDL7141_FAULT_OTS           (1 << 8)  // Over-temperature shutdown
#define EDL7141_FAULT_OTW           (1 << 9)  // Over-temperature warning
#define EDL7141_FAULT_RLOCK         (1 << 10) // Locked rotor fault
#define EDL7141_FAULT_WD            (1 << 11) // Watchdog fault
#define EDL7141_FAULT_OTP           (1 << 12) // OTP memory fault

#endif /* HWCONF_EDL7141_H_ */
