//
//  LC3
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
/* unix */
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

// Memory
/* 2^16 = 65536 memory allocation */
uint16_t memory[UINT16_MAX];

// Register
/*
    - 8 general purpose registers (R0-R7): used to perform program calculations
    - 1 program counter register (PC): store address of next operation to be performed
    - 1 condition flag register (COND): tells information about the previous operation performed
 */
enum {
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC, /* program counter register */
    R_COND, /* condition flag register */
    R_COUNT
};

uint16_t reg[R_COUNT];

// Instruction Set
/*
    LC-3 has 16 opcodes
 */
enum {
    OP_BR = 0, /* branch */
    OP_ADD, /* add */
    OP_LD,  /* load */
    OP_ST,  /* store */
    OP_JSR, /* jump register */
    OP_AND, /* bitwise and */
    OP_LDR, /* load register */
    OP_STR, /* store register */
    OP_RTI, /* unused */
    OP_NOT, /* bitwise not */
    OP_LDI, /* load indirect */
    OP_STI, /* store indirect */
    OP_JMP, /* jump */
    OP_RES, /* reserved (unused) */
    OP_LEA, /* load effective address */
    OP_TRAP /* execute trap */
};

// Condition flags
/*
    This LC-3 will be having three condition flags which is stored in R_COND.
    This value tells us about the result of the most recent calculation and is
    useful to check logical conditions like if(x > 0) { .. }
 */
enum {
    FL_POS = 1 << 0,    // positive
    FL_ZRO = 1 << 1,    // zero
    FL_NEG = 1 << 2,     // negative
};

// Sign Extension
uint16_t sign_extend(uint16_t x, int bit_count) {
    if((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

// Condition Flag updation.
/*
    Anytime a value is written in a register, this
    flag needs to be updated to indicate its sign.
 */
void update_flags(uint16_t r) {
    if(reg[r] == 0) {
        reg[R_COND] = FL_ZRO;
    } else if(reg[r] >> 15) {
        /* 1 in the leftmost bit means it's a negative */
        reg[R_COND] = FL_NEG;
    } else {
        reg[R_COND] = FL_POS;
    }
}

uint16_t mem_read(uint16_t r) {
    return 0xF;
}

int main(int argc, const char * argv[]) {
    
    /*
        load arguments
     */
    if(argc < 2) {
        printf("main [image-file1] .. \n");
        exit(2);
    }
    
    /*
        set the starting position.
     */
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;
    
    int running = 1;
    while(running) {
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;
        
        switch (op) {
            case OP_ADD:
                {
                    /* destination register (DR) */
                    uint16_t r0 = (instr >> 9) & 0x7;
                    
                    /* first operand (SR1) */
                    uint16_t r1 = (instr >> 6) & 0x7;
                    
                    /* whether immediate mode or register mode */
                    uint16_t imm_flag = (instr >> 5) & 0x1;
                    
                    if(imm_flag) {
                        /* imm5, immediate value passed in ADD for immediate mode */
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                        reg[r0] = reg[r1] + imm5;
                    } else {
                        /* second operand (SR2) incase of register mode */
                        uint16_t r2 = instr & 0x7;
                        reg[r0] = reg[r1] + reg[r2];
                    }
                    
                    update_flags(r0);
                }
                break;
            case OP_AND:
            {
                /* destination register (DR) */
                uint16_t r0 = (instr >> 9) & 0x7;
                
                /* first operand (SR1) */
                uint16_t r1 = (instr >> 6) & 0x7;
                
                /* whether immediate mode or register mode */
                uint16_t imm_flag = (instr >> 5) & 0x1;
                
                if(imm_flag) {
                    /* imm5, immediate value passed in AND for immediate mode */
                    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                    reg[r0] = reg[r1] & imm5;
                } else {
                    /* second operand (SR2) incase of register mode */
                    uint16_t r2 = instr & 0x7;
                    reg[r0] = reg[r1] & reg[r2];
                }
                
                update_flags(reg[r0]);
            }
                break;
            case OP_NOT:
            {
                /* destination register (DR) */
                uint16_t r0 = (instr >> 9) & 0x7;
                
                /* operand (SR) */
                uint16_t r1 = (instr >> 6) & 0x7;
                
                reg[r0] = ~reg[r1];
                update_flags(r0);
            }
                break;
            case OP_BR:
            {
                /* PC offset */
                uint16_t offset = sign_extend(instr & 0x1FF, 9);
                
                /* conditional flag */
                uint16_t cond = (instr >> 9) & 0x7;
                
                if(cond & reg[R_COND]) {
                    reg[R_PC] += offset;
                }
            }
                break;
            case OP_JMP:
            {
                /* base register (BaseR) */
                uint16_t r0 = (instr >> 6) & 0x7;
                reg[R_PC] = reg[r0];
            }
                break;
            case OP_JSR:
                break;
            case OP_LD:
                break;
            case OP_LDI:
                {
                    /* destination register (DR) */
                    uint16_t r0 = (instr >> 9) & 0x7;
                    
                    /* PC offset 9 */
                    uint16_t pcoffset = sign_extend(instr & 0x1FF, 9);
                    
                    /* Add the offset to the current PC and look at the memory location to get the final address */
                    reg[r0] = mem_read(mem_read(reg[R_PC] + pcoffset));
                    update_flags(r0);
                }
                break;
            case OP_LDR:
                break;
            case OP_LEA:
                break;
            case OP_ST:
                break;
            case OP_STI:
                break;
            case OP_STR:
                break;
            case OP_TRAP:
                break;
            case OP_RES:
            case OP_RTI:
            default:
                break;
        }
    }
    
    return 0;
}
