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

//-----------------------------------------------------------------------------//
//                               Unix Specific
//-----------------------------------------------------------------------------//
uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}
//-----------------------------------------------------------------------------//
//-----------------------------------------------------------------------------//

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

// Trap instructions
enum {
    TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
    TRAP_OUT = 0x21,   /* output a character */
    TRAP_PUTS = 0x22,  /* output a word string */
    TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24, /* output a byte string */
    TRAP_HALT = 0x25   /* halt the program */
};

// Memory Mapped Registers
/*
    These are pre-defined special registers. For LC-3, two memory map registers are
    required.
 */
enum {
    MR_KBSR = 0xFE00, /* keyboard status. Whether a key was pressed or not */
    MR_KBDR = 0xFE02  /* keyboard data. Which key was pressed */
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

// Read from memory
/*
    KBSR and KBDR allow you to 'poll' the state of the device
    and continute execution, so that programs remain responsive while
    awaiting for input from the user.
 */
uint16_t mem_read(uint16_t r) {
    if(r == MR_KBSR) {
        if(check_key()) {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        } else {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[r];
}

// Write to memory
void mem_write(uint16_t r, uint16_t v) {
    memory[r] = v;
}


// LC-3 VMs are big endian.
/*
    most of our systems are little endian
 */
uint16_t swap16(uint16_t x) {
    return (x << 8) | (x >> 8);
}

// Reading an image file
/*
    programs are nothing but a set of instructions converted from
    the assembly programs to machine code (byte code)
    and putting them onto a file.
 
    These instructions can then be put into memory for execution by loading the
    file onto the VM. This can then be loaded just by
    copying the contents right into an address in the memory.
 
    The first 16 bits of any such program is called the origin and
    specifies the address in the memory where the program should start.
 */
void read_image_file(FILE* file) {
    /* the origin tells us where in memory to place the image */
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);
    
    /* since we know the maximum file size so we only need one fread */
    uint16_t max_read = UINT16_MAX - origin;
    uint16_t* p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    /* swap to little endian */
    while (read-- > 0)
    {
        *p = swap16(*p);
        ++p;
    }
}

// Read file from a string path
int read_image(const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if (!file) {
        return 0;
    }
    
    read_image_file(file);
    fclose(file);
    return 1;
}

int main(int argc, const char * argv[]) {
    
    /*
        load arguments
     */
    if(argc < 2) {
        printf("main [image-file1] .. \n");
        exit(2);
    }
    
    for(int j=1; j<argc; j++) {
        if(!read_image(argv[j])) {
            printf("failed to load image %s \n", argv[j]);
            exit(1);
        }
    }
    
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();
    
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
            {
                /* mode flag */
                uint16_t flag = (instr >> 11) & 0x1;
                reg[R_R7] = reg[R_PC];
                
                if(flag) {
                    /* PC offset */
                    uint16_t offset = sign_extend(instr & 0x7FF, 11);
                    reg[R_PC] += offset;
                } else {
                    /* base register (BaseR) */
                    uint16_t r0 = (instr >> 6) & 0x7;
                    reg[R_PC] = reg[r0];
                }
            }
                break;
            case OP_LD:
            {
                /* destination register (DR) */
                uint16_t r0 = (instr >> 9) & 0x7;
                
                /* PC offset */
                uint16_t offset = sign_extend(instr & 0x1FF, 9);
                
                reg[r0] = mem_read(reg[R_PC] + offset);
                update_flags(r0);
            }
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
            {
                /* destination register (DR) */
                uint16_t r0 = (instr >> 9) & 0x7;
                
                /* base register (BaseR) */
                uint16_t r1 = (instr >> 6) & 0x7;
                
                /* offset */
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                
                reg[r0] = mem_read(reg[r1] + offset);
                update_flags(r0);
            }
                break;
            case OP_LEA:
            {
                /* destination register (DR) */
                uint16_t r0 = (instr >> 9) & 0x7;
                
                /* PC offset */
                uint16_t offset = sign_extend(instr & 0x1FF, 9);
                
                reg[r0] = reg[R_PC] + offset;
                update_flags(r0);
            }
                break;
            case OP_ST:
            {
                /*  first operand (SR) */
                uint16_t r0 = (instr >> 9) & 0x7;
                
                /* PC offset */
                uint16_t offset = sign_extend(instr & 0x1FF, 9);
                
                mem_write(reg[R_PC] + offset, reg[r0]);
            }
                break;
            case OP_STI:
            {
                /* first operand (SR) */
                uint16_t r0 = (instr >> 9) & 0x7;
                
                /* PC offset */
                uint16_t offset = sign_extend(instr & 0x1FF, 9);
                
                mem_write(mem_read(reg[R_PC] + offset), reg[r0]);
            }
                break;
            case OP_STR:
            {
                /* base register (BaseR) */
                uint16_t r0 = (instr >> 6) & 0x7;
                
                /* stored register (SR) */
                uint16_t r1 = (instr >> 9) & 0x7;
                
                /* offset */
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                
                mem_write(reg[r0] + offset, reg[r1]);
            }
                break;
            case OP_TRAP:
            {
                switch (instr & 0xFF) {
                    case TRAP_GETC:
                        reg[R_R0] = (uint16_t)getchar();
                        break;
                    case TRAP_IN:
                    {
                        printf("Insert a character: ");
                        char c = getchar();
                        putc(c, stdout);
                        reg[R_R0] = (uint16_t)c;
                    }
                        break;
                    case TRAP_OUT:
                    {
                        putc((char)reg[R_R0], stdout);
                        fflush(stdout);
                    }
                        break;
                    case TRAP_HALT:
                        break;
                    case TRAP_PUTS:
                    {
                        uint16_t* c = memory + reg[R_R0];
                        while(*c) {
                            putc((char)*c, stdout);
                            ++c;
                        }
                        fflush(stdout);
                    }
                        break;
                    case TRAP_PUTSP:
                    {
                        /* one char per byte (two bytes per word)
                           here we need to swap back to
                           big endian format */
                        uint16_t* c = memory + reg[R_R0];
                        while (*c)
                        {
                            char char1 = (*c) & 0xFF;
                            putc(char1, stdout);
                            char char2 = (*c) >> 8;
                            if (char2) putc(char2, stdout);
                            ++c;
                        }
                        fflush(stdout);
                    }
                        break;
                }
            }
                break;
            case OP_RES:
            case OP_RTI:
                break;
            default:
                /* Bad opcode */
                abort();
                break;
        }
    }
    
    restore_input_buffering();
}
