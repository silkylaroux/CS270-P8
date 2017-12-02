/** @file logic.c
 *  @brief Implementation of the logic.h interface
 *  @details This is a skeleton implementation of the interface that you will write
 *  for the assignment. The skeleton provides functions to fetch, decode, and execute 
 *  instructions as well as the ADD immediate and ST instructions.  This allows you
 *  to run the simulator with a simple test program before starting to verify your
 *  setup. You must complete the remainder of the instructions.
 *  <p>
 *  @author <b>your name here</b>
 */

#include "lc3.h"
#include "hardware.h"
#include "logic.h"

static int not_implemented() {
  return (! OK);
}

/* Here's the ADD immediate and ST instructions to get started,
 * along with a helper function to determine condition codes.
 * You need to complete the ADD instruction to handle SR2. 
 */ 

static int logic_NZP(LC3_WORD value) {
    /* determine the appropriate 3 bit condition code for value */
    int nzp = 0;
    if ((value & 0x8000) != 0)
        nzp = 4; /* negative */
    else if (value == 0)
        nzp = 2; /* zero */
    else
        nzp = 1; /* positive */
    return nzp;
}

static int execute_ADD (instruction_t* inst) {
  /* clock cycle 4 */
  LC3_WORD ALU = hardware_get_REG(inst->SR1) ;
  if (inst->bit5 == 0)               /* ADD SR2 */
    ALU += inst->SR2 ;    
  else                               /* ADD immediate */
    ALU += inst->imm5;
  lc3_BUS = &ALU;                    /* put ALU on BUS  */
  hardware_load_REG(inst->DR);       /* load DR from BUS  */
  hardware_set_CC(logic_NZP(ALU));   /* set condition code */
  return OK;
}

static int execute_ST (instruction_t* inst) {
  /* clock cycle 4 */
  LC3_WORD MARMUX;
  MARMUX = hardware_get_PC() + inst->PCoffset9;
  lc3_BUS = &MARMUX;                 /* put addr on BUS   */
  hardware_load_MAR();               /* load MAR from BUS */
  /* clock cycle 5 */
  MARMUX = hardware_get_REG(inst->SR);
  lc3_BUS = &MARMUX;                 /* put value on BUS  */
  hardware_load_MDR();               /* load MDR from BUS */
  /* clock cycle 6 */
  hardware_memory_enable(1);         /* write to memory   */
  return OK;
}


/** @todo many more instructions to add here */ 

static int execute_AND (instruction_t* inst) {
  /* clock cycle 4*/
  LC3_WORD ALU = hardware_get_REG(inst->SR1) ;
  if (inst->bit5 == 0)               /* AND SR2 */
    ALU &= inst->SR2 ;   
  else                               /* AND immediate */
    ALU &= inst->imm5;
  lc3_BUS = &ALU;                    /* put ALU on BUS  */
  hardware_load_REG(inst->DR);       /* load DR from BUS  */
  hardware_set_CC(logic_NZP(ALU));   /* set condition code */
  return OK;
}

static int execute_NOT (instruction_t* inst) {
    /*clock cycle 4*/
    LC3_WORD ALU = hardware_get_REG(inst->SR1) ;
    ALU = ~ALU ;                     /* NOT SR1 */

    lc3_BUS = &ALU;
    hardware_load_REG(inst->DR);     /* load DR from BUS */
    hardware_set_CC(logic_NZP(ALU));
    return OK;
}



/* Instruction fetch, decode, and execution functions already provided. 
 *
 * logic_fetch_instruction completes the first 3 clock cycles of each
 * instruction that load the IR from PC address and increments PC.
 *
 * logic_decode_instruction verifies instruction and extracts components 
 * from the IR used by the instructions and performs appropriate sign extensions.
 *
 * logic_execute_instruction calls the appropriate routine to complete
 * the remaining cycles for the specified opcode.  
 */

void logic_fetch_instruction (instruction_t* inst) {
  /* clock cycle 1 */
  hardware_gate_PC();             /* put PC onto BUS   */
  hardware_load_MAR();            /* load MAR from BUS */
  hardware_set_PC(*lc3_BUS+1);    /* increment PC      */
  inst->addr = *lc3_BUS;          /* save PC for inst  */
  /* clock cycle 2 */
  hardware_memory_enable(0);      /* read memory       */
  /* clock cycle 3 */
  hardware_gate_MDR();            /* put MDR on BUS    */
  inst->bits = *lc3_BUS;          /* load IR from BUS  */

}

int logic_decode_instruction (instruction_t* inst) {
  int valid   = OK; /* valid instruction */
  int instVal = inst->bits;

  inst->opcode              = (instVal >> 12) & 0xF;
  inst->DR                  = (instVal >>  9) & 0x7;
  inst->SR1                 = (instVal >>  6) & 0x7;
  inst->SR2                 = (instVal >>  0) & 0x7;
  inst->bit5                = (instVal >>  5) & 0x1;
  inst->bit11               = (instVal >> 11) & 0x1;
  inst->trapvect8           = (instVal >>  0) & 0xFF;
  inst->imm5                = ((instVal & 0x1F)  << (32 -  5)) >> (32 -  5);
  inst->offset6             = ((instVal & 0x3F)  << (32 -  6)) >> (32 -  6);
  inst->PCoffset9           = ((instVal & 0x1FF) << (32 -  9)) >> (32 -  9);
  inst->PCoffset11          = ((instVal & 0x7FF) << (32 - 11)) >> (32 - 11);

  /* check for invalid instructions (i.e. fields which must be all 0's or 1's */

  switch (inst->opcode) {
    case OP_BR:
      if (inst->nzp == 0)
        valid = !OK;
      break;
    
    case OP_ADD:
    case OP_AND: 
      if ((inst->bit5 == 0) && (((instVal >> 3) & 0x3) != 0))
        valid = !OK;
      break;

    case OP_JSR_JSRR:
      if ((inst->bit11 == 0) && ((inst->DR != 0) || (inst->offset6 != 0)))
        valid = !OK;
      break;

    case OP_RTI:
      if ((instVal & 0xFFF) != 0)
        valid = !OK;
      break;

    case OP_NOT:
      if (inst->offset6 != 0xFFFF)
        valid = !OK;
      break;

    case OP_JMP_RET:
      if ((inst->DR != 0) || (inst->offset6 != 0))
        valid = !OK;
      break;

    case OP_RESERVED:

#if defined STACK_OPS
      if (! (   (inst->BaseR == 6)
             && (   (inst->offset6 == 0x1    /* +1 */)
                 || (inst->offset6 == 0xFFFF /* -1 */))))
        valid = !OK;
      break;
#else 
      valid = !OK; // RESERVED not used
      break;
#endif

    case OP_TRAP:
      if (((instVal >> 8) & 0xF) != 0)
        valid = !OK;
      break;

    default: /* LEA, LD, LDI, LDR, ST, STI STR, ... have no additional checks */
      break;
  }

  return valid;
}

int logic_execute_instruction (instruction_t* inst) {
  switch (inst->opcode) {
    case OP_BR:       return not_implemented();
    case OP_ADD:      return execute_ADD(inst);
    case OP_LD:       return not_implemented();
    case OP_ST:       return execute_ST(inst);
    case OP_JSR_JSRR: return not_implemented();
    case OP_AND:      return not_implemented();
    case OP_LDR:      return not_implemented();
    case OP_STR:      return not_implemented();
    case OP_RTI:      return not_implemented();
    case OP_NOT:      return not_implemented();
    case OP_LDI:      return not_implemented();
    case OP_STI:      return not_implemented();
    case OP_JMP_RET:  return not_implemented();
    case OP_RESERVED: return not_implemented();
    case OP_LEA:      return not_implemented();
    case OP_TRAP:     return not_implemented();
    default:          return not_implemented();
  }

  return (! OK);
}


