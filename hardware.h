#ifndef __HARDWARE_H__
#define __HARDWARE_H__

/*
 * "Copyright (c) 2012-2016 by Fritz Sieker."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written
 * agreement is hereby granted, provided that the above copyright notice
 * and the following two paragraphs appear in all copies of this software,
 * that the files COPYING and NO_WARRANTY are included verbatim with
 * any distribution, and that the contents of the file README are included
 * verbatim as part of a file named README with any distribution.
 *
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT,
 * INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE AUTHOR
 * HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
 * BASIS, AND THE AUTHOR NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT,
 * UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 */

/** @file hardware.h
 *  @brief defines interface to code simulating hardware (do not modify)
 *  @details This defines the interface to the code simulating the hardware
 *  contained in hardware.c. There are a variety of routines corresponding
 *  to actions the control logic of the LC3 can perform. For example, it
 *  can gate various values onto the bus and transfer values to registers
 *  from the bus. It is expected that the student will encapsulate many
 *  of these routines to give a cleaner interface to the hardware. This
 *  code will be in logic.c. With the exception of the bus, all hardware
 *  data structures are hidden behind accessor functions.
 *  <p>
 *  Not all of the interface mirrors the LC3 hardware. The differences were
 *  designed to simplify the code.
 *  <p>
 *  @author Fritz Sieker
 */

#include "lc3.h"
#include "logic.h"

/** This controls whether variable is extern or not */
#ifndef HARDWARE_VAR
#define HARDWARE_VAR extern
#endif

/** The LC3 bus is modeled as a pointer. Reading from the bus (dereferencing
 *  the pointer) is getting the value from whatever source is currently
 *  "driving" the bus. Similarly, writing (gating) the bus invloves setting the
 *  bus value to the address of the value that is "on" the bus.
 *  The bus is a global that can be accessed outside of this code.
 */
HARDWARE_VAR LC3_WORD* lc3_BUS;

/** Reset all the hardware variables in the machine to a known state. */
void hardware_reset (void);

/** The memory address register (MAR) is loaded from the bus. */
void hardware_load_MAR (void);

/** The memory data register (MDR) is loaded from the bus. */
void hardware_load_MDR (void);

/** The value of the memory data register (MDR) is put "on" the bus. */
void hardware_gate_MDR (void);

/** The processor status register (PSR) is loaded from the bus. */
void hardware_load_PSR (void);

/** The value of the processor status register (PSR) is put "on" the bus. */
void hardware_gate_PSR (void);

/** The value of the program counter (PC) is put "on" the bus. */
void hardware_gate_PC (void);

/** The program counter (PC) is set to the parameter.
 *  @param addr - new value for the PC
 */
void hardware_set_PC (LC3_WORD addr);

/** Returns the current value of the PC.
 *
 */
LC3_WORD hardware_get_PC (void);

/** The instruction register (IR) is loaded from the bus. */
void hardware_load_IR (void);

/** Return the current value of the instruction register (IR). */
LC3_WORD hardware_get_IR(void);

/** The value of a register (0-7) is put "on" the bus.
 *  @param regNum - register to get value from
 */
void hardware_gate_REG (int regNum);

/** Copy the value "on" the LC3 bus into the designated register (0-7).
 *  @param regNum - register to load
 */
void hardware_load_REG (int regNum);

/** Return the value of a register (0-7).
 *  @param regNum - register to get value from
 *  @return - value contained in designated register
 */
LC3_WORD hardware_get_REG (int regNum); /**< get value from register R0-R7 */

/** This is the low level memory access function. It controls whether a read
 *  or write is performed. Is <b>assumes</b> that the MAR and possibly the
 *  MDR have been set appropriately by calls to other functions.
 *  @param rw - if non zero, a write is performed by trasfering the contents
 *  of the MDR to memory the address contained in the MAR. If it is 0, a read is
 *  performed and the contents of memory at the address contained in the MAR
 *  are copied to the MDR.
 */
void hardware_memory_enable (int rw);

/** Return the mode of the processor
 *  @return 0 for supervisor mode, 1 for user mode
 */
int hardware_get_mode(void);

/** Toggle between user and system mode. For future use.
 *  @param mode - 0 is system mode, 1 is user mode
 */
void hardware_set_mode (int mode);

/** Return the current value of the condition code. */
int hardware_get_CC (void);

/** Set the condition code
 *  @param val - new value for condition code
 */
void hardware_set_CC (int val);

/** This is the basis for doing single stepping durring debugging. It
 *  encapsulates the basic instruction cycle by making calls to three
 *  external routines, normally written by students as part of the
 *  simulator assignment. The routines are:
 *  <ol>
 *  <li>logic_fetch_instruction()</li>
 *  <li>logic_decode_instruction()</li>
 *  <li>logic_execute_instruction()</li>
 *  </ol>
 *  @param inst - storage for info of the instruction
 *  @return 0 on success, non-zero on failure
 */
int hardware_step (instruction_t* inst);

#endif /* ifdef __HARDWARE_H__ */
