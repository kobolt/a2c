#include "w65c02_trace.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "w65c02.h"
#include "mem.h"



#define W65C02_TRACE_BUFFER_SIZE 256

typedef enum {
  AM_ACCU, /* A      - Accumulator */
  AM_IMPL, /* i      - Implied */
  AM_IMM,  /* #      - Immediate */
  AM_ABS,  /* a      - Absolute */
  AM_ABSI, /* (a)    - Indirect Absolute */
  AM_ABSX, /* a,x    - Absolute + X */
  AM_ABSY, /* a,y    - Absolute + Y */
  AM_ABIX, /* (a,x)  - Indirect Absolute + X */
  AM_REL,  /* r      - Relative */
  AM_ZP,   /* zp     - Zero Page */
  AM_ZPX,  /* zp,x   - Zero Page + X */
  AM_ZPY,  /* zp,y   - Zero Page + Y */
  AM_ZPYI, /* (zp),y - Zero Page Indirect Indexed */
  AM_ZPIX, /* (zp,x) - Zero Page Indexed Indirect */
  AM_ZPR,  /* zp,r   - Zero Page Relative */
  AM_ZPI,  /* (zp)   - Zero Page Indirect */
} w65c02_address_mode_t;

typedef struct w65c02_trace_s {
  w65c02_t cpu;
  uint8_t mc[3];
} w65c02_trace_t;



static w65c02_address_mode_t opcode_address_mode[UINT8_MAX + 1] = {
  AM_IMPL, AM_ZPIX, AM_IMM,  AM_IMPL, AM_ZP,   AM_ZP,   AM_ZP,   AM_ZP,
  AM_IMPL, AM_IMM,  AM_ACCU, AM_IMPL, AM_ABS,  AM_ABS,  AM_ABS,  AM_ZPR,
  AM_REL,  AM_ZPYI, AM_ZPI,  AM_IMPL, AM_ZP,   AM_ZPX,  AM_ZPX,  AM_ZP,
  AM_IMPL, AM_ABSY, AM_IMPL, AM_IMPL, AM_ABS,  AM_ABSX, AM_ABSX, AM_ZPR,
  AM_ABS,  AM_ZPIX, AM_IMM,  AM_IMPL, AM_ZP,   AM_ZP,   AM_ZP,   AM_ZP,
  AM_IMPL, AM_IMM,  AM_ACCU, AM_IMPL, AM_ABS,  AM_ABS,  AM_ABS,  AM_ZPR,
  AM_REL,  AM_ZPYI, AM_ZPI,  AM_IMPL, AM_ZPX,  AM_ZPX,  AM_ZPX,  AM_ZP,
  AM_IMPL, AM_ABSY, AM_IMPL, AM_IMPL, AM_ABSX, AM_ABSX, AM_ABSX, AM_ZPR,
  AM_IMPL, AM_ZPIX, AM_IMM,  AM_IMPL, AM_ZP,   AM_ZP,   AM_ZP,   AM_ZP,
  AM_IMPL, AM_IMM,  AM_ACCU, AM_IMPL, AM_ABS,  AM_ABS,  AM_ABS,  AM_ZPR,
  AM_REL,  AM_ZPYI, AM_ZPI,  AM_IMPL, AM_ZPX,  AM_ZPX,  AM_ZPX,  AM_ZP,
  AM_IMPL, AM_ABSY, AM_IMPL, AM_IMPL, AM_ABS,  AM_ABSX, AM_ABSX, AM_ZPR,
  AM_IMPL, AM_ZPIX, AM_IMM,  AM_IMPL, AM_ZP,   AM_ZP,   AM_ZP,   AM_ZP,
  AM_IMPL, AM_IMM,  AM_ACCU, AM_IMPL, AM_ABSI, AM_ABS,  AM_ABS,  AM_ZPR,
  AM_REL,  AM_ZPYI, AM_ZPI,  AM_IMPL, AM_ZPX,  AM_ZPX,  AM_ZPX,  AM_ZP,
  AM_IMPL, AM_ABSY, AM_IMPL, AM_IMPL, AM_ABIX, AM_ABSX, AM_ABSX, AM_ZPR,
  AM_REL,  AM_ZPIX, AM_IMM,  AM_IMPL, AM_ZP,   AM_ZP,   AM_ZP,   AM_ZP,
  AM_IMPL, AM_IMM,  AM_IMPL, AM_IMPL, AM_ABS,  AM_ABS,  AM_ABS,  AM_ZPR,
  AM_REL,  AM_ZPYI, AM_ZPI,  AM_IMPL, AM_ZPX,  AM_ZPX,  AM_ZPY,  AM_ZP,
  AM_IMPL, AM_ABSY, AM_IMPL, AM_IMPL, AM_ABS,  AM_ABSX, AM_ABSX, AM_ZPR,
  AM_IMM,  AM_ZPIX, AM_IMM,  AM_IMPL, AM_ZP,   AM_ZP,   AM_ZP,   AM_ZP,
  AM_IMPL, AM_IMM,  AM_IMPL, AM_IMPL, AM_ABS,  AM_ABS,  AM_ABS,  AM_ZPR,
  AM_REL,  AM_ZPYI, AM_ZPI,  AM_IMPL, AM_ZPX,  AM_ZPX,  AM_ZPY,  AM_ZP,
  AM_IMPL, AM_ABSY, AM_IMPL, AM_IMPL, AM_ABSX, AM_ABSX, AM_ABSY, AM_ZPR,
  AM_IMM,  AM_ZPIX, AM_IMM,  AM_IMPL, AM_ZP,   AM_ZP,   AM_ZP,   AM_ZP,
  AM_IMPL, AM_IMM,  AM_IMPL, AM_IMPL, AM_ABS,  AM_ABS,  AM_ABS,  AM_ZPR,
  AM_REL,  AM_ZPYI, AM_ZPI,  AM_IMPL, AM_ZPX,  AM_ZPX,  AM_ZPX,  AM_ZP,
  AM_IMPL, AM_ABSY, AM_IMPL, AM_IMPL, AM_ABS,  AM_ABSX, AM_ABSX, AM_ZPR,
  AM_IMM,  AM_ZPIX, AM_IMM,  AM_IMPL, AM_ZP,   AM_ZP,   AM_ZP,   AM_ZP,
  AM_IMPL, AM_IMM,  AM_IMPL, AM_IMPL, AM_ABS,  AM_ABS,  AM_ABS,  AM_ZPR,
  AM_REL,  AM_ZPYI, AM_ZPI,  AM_IMPL, AM_ZPX,  AM_ZPX,  AM_ZPX,  AM_ZP,
  AM_IMPL, AM_ABSY, AM_IMPL, AM_IMPL, AM_ABS,  AM_ABSX, AM_ABSX, AM_ZPR,
};



static char *opcode_mnemonic[UINT8_MAX + 1] = {
  "BRK", "ORA", "NOP", "NOP", "TSB", "ORA", "ASL", "RMB0", /* 0x00 -> 0x07 */
  "PHP", "ORA", "ASL", "NOP", "TSB", "ORA", "ASL", "BBR0", /* 0x08 -> 0x0F */
  "BPL", "ORA", "ORA", "NOP", "TRB", "ORA", "ASL", "RMB1", /* 0x10 -> 0x17 */
  "CLC", "ORA", "INC", "NOP", "TRB", "ORA", "ASL", "BBR1", /* 0x18 -> 0x1F */
  "JSR", "AND", "NOP", "NOP", "BIT", "AND", "ROL", "RMB2", /* 0x20 -> 0x27 */
  "PLP", "AND", "ROL", "NOP", "BIT", "AND", "ROL", "BBR2", /* 0x28 -> 0x2F */
  "BMI", "AND", "AND", "NOP", "BIT", "AND", "ROL", "RMB3", /* 0x30 -> 0x37 */
  "SEC", "AND", "DEC", "NOP", "BIT", "AND", "ROL", "BBR3", /* 0x38 -> 0x3F */
  "RTI", "EOR", "NOP", "NOP", "NOP", "EOR", "LSR", "RMB4", /* 0x40 -> 0x47 */
  "PHA", "EOR", "LSR", "NOP", "JMP", "EOR", "LSR", "BBR4", /* 0x48 -> 0x4F */
  "BVC", "EOR", "EOR", "NOP", "NOP", "EOR", "LSR", "RMB5", /* 0x50 -> 0x57 */
  "CLI", "EOR", "PHY", "NOP", "NOP", "EOR", "LSR", "BBR5", /* 0x58 -> 0x5F */
  "RTS", "ADC", "NOP", "NOP", "STZ", "ADC", "ROR", "RMB6", /* 0x60 -> 0x67 */
  "PLA", "ADC", "ROR", "NOP", "JMP", "ADC", "ROR", "BBR6", /* 0x68 -> 0x6F */
  "BVS", "ADC", "ADC", "NOP", "STZ", "ADC", "ROR", "RMB7", /* 0x70 -> 0x77 */
  "SEI", "ADC", "PLY", "NOP", "JMP", "ADC", "ROR", "BBR7", /* 0x78 -> 0x7F */
  "BRA", "STA", "NOP", "NOP", "STY", "STA", "STX", "SMB0", /* 0x80 -> 0x87 */
  "DEY", "BIT", "TXA", "NOP", "STY", "STA", "STX", "BBS0", /* 0x88 -> 0x8F */
  "BCC", "STA", "STA", "NOP", "STY", "STA", "STX", "SMB1", /* 0x90 -> 0x97 */
  "TYA", "STA", "TXS", "NOP", "STZ", "STA", "STZ", "BBS1", /* 0x98 -> 0x9F */
  "LDY", "LDA", "LDX", "NOP", "LDY", "LDA", "LDX", "SMB2", /* 0xA0 -> 0xA7 */
  "TAY", "LDA", "TAX", "NOP", "LDY", "LDA", "LDX", "BBS2", /* 0xA8 -> 0xAF */
  "BCS", "LDA", "LDA", "NOP", "LDY", "LDA", "LDX", "SMB3", /* 0xB0 -> 0xB7 */
  "CLV", "LDA", "TSX", "NOP", "LDY", "LDA", "LDX", "BBS3", /* 0xB8 -> 0xBF */
  "CPY", "CMP", "NOP", "NOP", "CPY", "CMP", "DEC", "SMB4", /* 0xC0 -> 0xC7 */
  "INY", "CMP", "DEX", "WAI", "CPY", "CMP", "DEC", "BBS4", /* 0xC8 -> 0xCF */
  "BNE", "CMP", "CMP", "NOP", "NOP", "CMP", "DEC", "SMB5", /* 0xD0 -> 0xD7 */
  "CLD", "CMP", "PHX", "STP", "NOP", "CMP", "DEC", "BBS5", /* 0xD8 -> 0xDF */
  "CPX", "SBC", "NOP", "NOP", "CPX", "SBC", "INC", "SMB6", /* 0xE0 -> 0xE7 */
  "INX", "SBC", "NOP", "NOP", "CPX", "SBC", "INC", "BBS6", /* 0xE8 -> 0xEF */
  "BEQ", "SBC", "SBC", "NOP", "NOP", "SBC", "INC", "SMB7", /* 0xF0 -> 0xF7 */
  "SED", "SBC", "PLX", "NOP", "NOP", "SBC", "INC", "BBS7", /* 0xF8 -> 0xFF */
};



static w65c02_trace_t w65c02_trace_buffer[W65C02_TRACE_BUFFER_SIZE];
static int w65c02_trace_index = 0;



static void w65c02_disassemble(FILE *fh, uint16_t pc, uint8_t mc[3])
{
  uint16_t address;
  int8_t relative;

  switch (opcode_address_mode[mc[0]]) {
  case AM_ACCU:
  case AM_IMPL:
    fprintf(fh, "%02x         ", mc[0]);
    break;

  case AM_IMM:
  case AM_REL:
  case AM_ZP:
  case AM_ZPX:
  case AM_ZPY:
  case AM_ZPYI:
  case AM_ZPIX:
  case AM_ZPI:
    fprintf(fh, "%02x %02x      ", mc[0], mc[1]);
    break;

  case AM_ABS:
  case AM_ABSI:
  case AM_ABSX:
  case AM_ABSY:
  case AM_ABIX:
  case AM_ZPR:
    fprintf(fh, "%02x %02x %02x   ", mc[0], mc[1], mc[2]);
    break;
  }

  fprintf(fh, "%s ", opcode_mnemonic[mc[0]]);

  switch (opcode_address_mode[mc[0]]) {
  case AM_ACCU:
    fprintf(fh, "A         ");
    break;

  case AM_IMPL:
    fprintf(fh, "          ");
    break;

  case AM_IMM:
    fprintf(fh, "#$%02x      ", mc[1]);
    break;

  case AM_ABS:
    fprintf(fh, "$%02x%02x     ", mc[2], mc[1]);
    break;

  case AM_ABSI:
    fprintf(fh, "($%02x%02x)   ", mc[2], mc[1]);
    break;

  case AM_ABSX:
    fprintf(fh, "$%02x%02x,X   ", mc[2], mc[1]);
    break;

  case AM_ABSY:
    fprintf(fh, "$%02x%02x,Y   ", mc[2], mc[1]);
    break;

  case AM_ABIX:
    fprintf(fh, "($%02x%02x,X) ", mc[2], mc[1]);
    break;

  case AM_REL:
    address = pc + 2;
    relative = mc[1];
    address += relative;
    fprintf(fh, "$%04x     ", address);
    break;

  case AM_ZP:
    fprintf(fh, "$%02x       ", mc[1]);
    break;

  case AM_ZPX:
    fprintf(fh, "$%02x,X     ", mc[1]);
    break;

  case AM_ZPY:
    fprintf(fh, "$%02x,Y     ", mc[1]);
    break;

  case AM_ZPYI:
    fprintf(fh, "($%02x),Y   ", mc[1]);
    break;

  case AM_ZPIX:
    fprintf(fh, "($%02x,X)   ", mc[1]);
    break;

  case AM_ZPR:
    address = pc + 2;
    relative = mc[2];
    address += relative;
    fprintf(fh, "$%02x,$%04x ", mc[1], address);
    break;

  case AM_ZPI:
    fprintf(fh, "($%02x)     ", mc[1]);
    break;
  }

  if (((mc[0] & 0xF) == 0xF) || ((mc[0] & 0xF) == 0x7)) {
    fprintf(fh, "  ");
  } else {
    fprintf(fh, "   ");
  }
}



static void w65c02_register_dump(FILE *fh, w65c02_t *cpu, uint8_t mc[3])
{
  fprintf(fh, "PC:%04x   ", cpu->pc);
  w65c02_disassemble(fh, cpu->pc, mc);
  fprintf(fh, "A:%02x ", cpu->a);
  fprintf(fh, "X:%02x ", cpu->x);
  fprintf(fh, "Y:%02x ", cpu->y);
  fprintf(fh, "S:%02x ", cpu->s);
  fprintf(fh, "P:");
  fprintf(fh, "%c", (cpu->status.n) ? 'N' : '.');
  fprintf(fh, "%c", (cpu->status.v) ? 'V' : '.');
  fprintf(fh, "-");
  fprintf(fh, "%c", (cpu->status.b) ? 'B' : '.');
  fprintf(fh, "%c", (cpu->status.d) ? 'D' : '.');
  fprintf(fh, "%c", (cpu->status.i) ? 'I' : '.');
  fprintf(fh, "%c", (cpu->status.z) ? 'Z' : '.');
  fprintf(fh, "%c", (cpu->status.c) ? 'C' : '.');
  fprintf(fh, "\n");
}



void w65c02_trace_init(void)
{
  memset(w65c02_trace_buffer, 0,
    W65C02_TRACE_BUFFER_SIZE * sizeof(w65c02_trace_t));
}



void w65c02_trace_dump(FILE *fh)
{
  int i;

  for (i = 0; i < W65C02_TRACE_BUFFER_SIZE; i++) {
    w65c02_trace_index++;
    if (w65c02_trace_index >= W65C02_TRACE_BUFFER_SIZE) {
      w65c02_trace_index = 0;
    }
    w65c02_register_dump(fh, &w65c02_trace_buffer[w65c02_trace_index].cpu,
                              w65c02_trace_buffer[w65c02_trace_index].mc);
  }
}



void w65c02_trace_add(w65c02_t *cpu, mem_t *mem)
{
  uint8_t mc[3];

  w65c02_trace_index++;
  if (w65c02_trace_index >= W65C02_TRACE_BUFFER_SIZE) {
    w65c02_trace_index = 0;
  }

  memcpy(&w65c02_trace_buffer[w65c02_trace_index].cpu,
    cpu, sizeof(w65c02_t));
  mc[0] = mem_read(mem, cpu->pc);
  mc[1] = mem_read(mem, cpu->pc + 1);
  mc[2] = mem_read(mem, cpu->pc + 2);
  memcpy(&w65c02_trace_buffer[w65c02_trace_index].mc,
    mc, sizeof(uint8_t) * 3);
}



