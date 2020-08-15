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
                break;
            case OP_AND:
                break;
            case OP_NOT:
                break;
            case OP_BR:
                break;
            case OP_JMP:
                break;
            case OP_JSR:
                break;
            case OP_LD:
                break;
            case OP_LDI:
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
