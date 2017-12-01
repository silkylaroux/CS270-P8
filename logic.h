#ifndef __LOGIC_H__
#define __LOGIC_H__

/* Copyright (c) 2012-2015 by Fritz Sieker.
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

#include "lc3.h"
//#include "hardware.h"

/** @file logic.h
 *  @brief interface to the control logic of the simulator (do not modify)
 *  @details This is the interface to the code  (in logic.c) that simulates the
 *  control logic of the LC3 machine. Students
 *  will typically write the implementation as part of an assignment to
 *  complete an LC3 simulator. The implementation may depend on a previous
 *  assignment where students wrote methods for bit extraction/mainpulation.
 *
 *  @author Fritz Sieker
 */

#define OK 0 /**< value denoting successful operation */

/** This structure holds all the fields that occur in an instruction.
 *  Conceptually, all of these fields are computed in parallel in the
 *  combinational logic of the LC3 and the control logic selects the
 *  values to use depending on the actual instruction. In hardware, there
 *  is "no" cost to computing things you might not use. In the implementation,
 *  you may choose to precompute all the fields, or simply get values from
 *  the instruction itself as they are needed. This structure is used as
 *  the parameter for logic_fetch_instruction(), logic_decode_instruction(),
 *  and logic_execute_instruction(), and the specific instructions .
 */

#define SR         DR
#define BaseR      SR1
#define nzp        DR

typedef struct inst_fields {
  LC3_WORD addr;         /**< address of the instruction                   */
  LC3_WORD bits;         /**< 16 bit instruction                           */
  opcode_t opcode;       /**< opcode of instruction                        */
  int      DR;           /**< destination register                         */
  int      SR1;          /**< source register 1                            */
  int      SR2;          /**< source register 2                            */
  int      bit5;         /**< distinguishes immediate from register        */
  int      bit11;        /**< distinguishes JSR/JSRR                       */
  LC3_WORD trapvect8;    /**< value for TRAP                               */
  LC3_WORD imm5;         /**< 5 bit immediate value (signed)               */
  LC3_WORD offset6;      /**< 6 bit register offset (signed)               */
  LC3_WORD PCoffset9;    /**< 9 bit offset from program counter (signed)   */
  LC3_WORD PCoffset11;   /**< 11 bit offset from program counter (signed)  */
} instruction_t;

/** This method encapsulates the details of an instruction fetch. Upon return
 *  the IR contains the instruction and the PC has been incremented, and the
 *  addr and bits fields of the structure have been set appropriately. The addr
 *  field in the structure is the address of the instruction, not the
 *  incremented PC. It must deal with getting the MAR set correctly,
 *  peforming a read, and setting the IR. This involves accessing the resources
 *  described in hardware.h.
 */
void logic_fetch_instruction (instruction_t* inst);

/** This function decodes the instruction. This involves setting fields of the
 *  structure that is the parameter of the method. At a minimum, it must set
 *  the opcode field. Setting the other fields is optional,
 *  but will simplify the code for executing the instruction. The return value
 *  follows the common C pattern of 0 meaning success (i.e. instruction is
 *  valid), and any non-zero value indicating the type of error. For this
 *  implementation, only zero and non-zero are tested.
 *  @param inst - a pointer to the instruction structure.
 */
int logic_decode_instruction (instruction_t* inst);

/** This function executes the instruction. At completion, the state of the
 *  LC3 hardware reflects the result of the instruction. The return value
 *  follows the common C pattern of 0 meaning success (i.e. instruction is
 *  valid), and any non-zero value indicating the type of error. For this
 *  implementation, only zero and non-zero are tested.
 */
int logic_execute_instruction (instruction_t* inst);


#endif

