#include "w65c02.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "mem.h"
#include "panic.h"



static inline uint8_t dec_to_bin(uint8_t value)
{
  return (value % 0x10) + ((value / 0x10) * 10);
}

static inline uint8_t bin_to_dec(uint8_t value)
{
  return (value % 10) + ((value / 10) * 0x10);
}



static inline void flag_zero_other(w65c02_t *cpu, uint8_t value)
{
  if (value == 0) {
    cpu->status.z = 1;
  } else {
    cpu->status.z = 0;
  }
}

static inline void flag_negative_other(w65c02_t *cpu, uint8_t value)
{
  if ((value >> 7) == 1) {
    cpu->status.n = 1;
  } else {
    cpu->status.n = 0;
  }
}

static inline void flag_zero_compare(w65c02_t *cpu, uint8_t a, uint8_t b)
{
  if (a == b) {
    cpu->status.z = 1;
  } else {
    cpu->status.z = 0;
  }
}

static inline void flag_negative_compare(w65c02_t *cpu, uint8_t a, uint8_t b)
{
  if (((uint8_t)(a - b) >> 7) == 1) {
    cpu->status.n = 1;
  } else {
    cpu->status.n = 0;
  }
}

static inline void flag_carry_compare(w65c02_t *cpu, uint8_t a, uint8_t b)
{
  if (a >= b) {
    cpu->status.c = 1;
  } else {
    cpu->status.c = 0;
  }
}

static inline bool flag_carry_add(w65c02_t *cpu, uint8_t value)
{
  if (cpu->status.d == 1) {
    if (cpu->a + value + cpu->status.c > 99) {
      return 1;
    } else {
      return 0;
    }
  } else {
    if (cpu->a + value + cpu->status.c > 0xFF) {
      return 1;
    } else {
      return 0;
    }
  }
}

static inline bool flag_carry_sub(w65c02_t *cpu, uint8_t value)
{
  bool borrow;
  if (cpu->status.c == 0) {
    borrow = 1;
  } else {
    borrow = 0;
  }
  if (cpu->a - value - borrow < 0) {
    return 0;
  } else {
    return 1;
  }
}

static inline void flag_overflow_add(w65c02_t *cpu, uint8_t a, uint8_t b)
{
  if (a >> 7 == b >> 7) {
    if (cpu->a >> 7 != a >> 7) {
      cpu->status.v = 1;
    } else {
      cpu->status.v = 0;
    }
  } else {
    cpu->status.v = 0;
  }
}

static inline void flag_overflow_sub(w65c02_t *cpu, uint8_t a, uint8_t b)
{
  if (a >> 7 != b >> 7) {
    if (cpu->a >> 7 != a >> 7) {
      cpu->status.v = 1;
    } else {
      cpu->status.v = 0;
    }
  } else {
    cpu->status.v = 0;
  }
}

static inline void flag_overflow_bit(w65c02_t *cpu, uint8_t value)
{
  if (((value >> 6) & 0x1) == 1) {
    cpu->status.v = 1;
  } else {
    cpu->status.v = 0;
  }
}



#define OP_PROLOGUE_ABS \
  uint16_t absolute; \
  absolute  = mem_read(mem, cpu->pc++); \
  absolute += mem_read(mem, cpu->pc++) * 256;

#define OP_PROLOGUE_ABSX \
  uint16_t absolute; \
  absolute  = mem_read(mem, cpu->pc++); \
  absolute += mem_read(mem, cpu->pc++) * 256; \
  absolute += cpu->x;

#define OP_PROLOGUE_ABSY \
  uint16_t absolute; \
  absolute  = mem_read(mem, cpu->pc++); \
  absolute += mem_read(mem, cpu->pc++) * 256; \
  absolute += cpu->y; \

#define OP_PROLOGUE_ZP \
  uint8_t zeropage; \
  zeropage = mem_read(mem, cpu->pc++); \

#define OP_PROLOGUE_ZPX \
  uint8_t zeropage; \
  zeropage  = mem_read(mem, cpu->pc++); \
  zeropage += cpu->x;

#define OP_PROLOGUE_ZPY \
  uint8_t zeropage; \
  zeropage  = mem_read(mem, cpu->pc++); \
  zeropage += cpu->y;

#define OP_PROLOGUE_ZPYI \
  uint8_t zeropage; \
  uint16_t absolute; \
  zeropage  = mem_read(mem, cpu->pc++); \
  absolute  = mem_read(mem, zeropage); \
  zeropage += 1; \
  absolute += mem_read(mem, zeropage) * 256; \
  absolute += cpu->y;

#define OP_PROLOGUE_ZPI \
  uint8_t zeropage; \
  uint16_t absolute; \
  zeropage  = mem_read(mem, cpu->pc++); \
  absolute  = mem_read(mem, zeropage); \
  zeropage += 1; \
  absolute += mem_read(mem, zeropage) * 256;

#define OP_PROLOGUE_ZPIX \
  uint8_t zeropage; \
  uint16_t absolute; \
  zeropage  = mem_read(mem, cpu->pc++); \
  zeropage += cpu->x; \
  absolute  = mem_read(mem, zeropage); \
  zeropage += 1; \
  absolute += mem_read(mem, zeropage) * 256;

#define OP_PROLOGUE_ABSX_BOUNDARY_CHECK \
  uint16_t absolute; \
  absolute  = mem_read(mem, cpu->pc++); \
  absolute += mem_read(mem, cpu->pc++) * 256; \
  if ((absolute & 0xFF00) != ((absolute + cpu->x) & 0xFF00)) cpu->cycles++; \
  absolute += cpu->x;

#define OP_PROLOGUE_ABSY_BOUNDARY_CHECK \
  uint16_t absolute; \
  absolute  = mem_read(mem, cpu->pc++); \
  absolute += mem_read(mem, cpu->pc++) * 256; \
  if ((absolute & 0xFF00) != ((absolute + cpu->y) & 0xFF00)) cpu->cycles++; \
  absolute += cpu->y; \

#define OP_PROLOGUE_ZPYI_BOUNDARY_CHECK \
  uint8_t zeropage; \
  uint16_t absolute; \
  zeropage  = mem_read(mem, cpu->pc++); \
  absolute  = mem_read(mem, zeropage); \
  zeropage += 1; \
  absolute += mem_read(mem, zeropage) * 256; \
  if ((absolute & 0xFF00) != ((absolute + cpu->y) & 0xFF00)) cpu->cycles++; \
  absolute += cpu->y;



static inline void w65c02_logic_adc(w65c02_t *cpu, uint8_t value)
{
  uint8_t initial;
  bool bit;
  int ah;
  int al;
  int as;
  if (cpu->status.d == 1) {
    ah = cpu->a;
    al = (ah & 0x0F) + (value & 0x0F) + (cpu->status.c);
    if (al >= 0x0A) {
      al = ((al + 0x06) & 0x0F) + 0x10;
    }
    as = (int8_t)(ah & 0xF0) + (int8_t)(value & 0xF0) + al;
    ah = (ah & 0xF0) + (value & 0xF0) + al;
    if (as < -128 || as > 127) {
      cpu->status.v = 1;
    } else {
      cpu->status.v = 0;
    }
    if (ah >= 0xA0) {
      ah += 0x60;
    }
    cpu->a = ah & 0xFF;
    if (ah >= 0x100) {
      cpu->status.c = 1;
    } else {
      cpu->status.c = 0;
    }
    flag_negative_other(cpu, cpu->a);
    flag_zero_other(cpu, cpu->a);
    cpu->cycles++;
  } else {
    initial = cpu->a;
    bit = flag_carry_add(cpu, value);
    cpu->a += value;
    cpu->a += cpu->status.c;
    flag_overflow_add(cpu, initial, value);
    flag_negative_other(cpu, cpu->a);
    flag_zero_other(cpu, cpu->a);
    cpu->status.c = bit;
  }
}

static inline void w65c02_logic_sbc(w65c02_t *cpu, uint8_t value)
{
  uint8_t initial;
  bool bit;
  int ah;
  int al;
  initial = cpu->a;
  bit = flag_carry_sub(cpu, value);
  cpu->a -= value;
  if (cpu->status.c == 0) {
    cpu->a -= 1;
  }
  flag_overflow_sub(cpu, initial, value);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
  if (cpu->status.d == 1) {
    ah = initial;
    al = (ah & 0x0F) - (value & 0x0F) + (cpu->status.c - 1);
    ah = ah - value + (cpu->status.c - 1);
    if (ah < 0) {
      ah -= 0x60;
    }
    if (al < 0) {
      ah -= 0x06;
    }
    cpu->a = ah & 0xFF;
    flag_negative_other(cpu, cpu->a);
    flag_zero_other(cpu, cpu->a);
    cpu->cycles++;
  }
  cpu->status.c = bit;
}



static void op_adc_imm(w65c02_t *cpu, mem_t *mem)
{
  uint8_t value = mem_read(mem, cpu->pc++);
  w65c02_logic_adc(cpu, value);
}

static void op_adc_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  w65c02_logic_adc(cpu, value);
}

static void op_adc_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  w65c02_logic_adc(cpu, value);
}

static void op_adc_absy(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  w65c02_logic_adc(cpu, value);
}

static void op_adc_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  w65c02_logic_adc(cpu, value);
}

static void op_adc_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  w65c02_logic_adc(cpu, value);
}

static void op_adc_zpyi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  w65c02_logic_adc(cpu, value);
}

static void op_adc_zpi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPI
  uint8_t value = mem_read(mem, absolute);
  w65c02_logic_adc(cpu, value);
}

static void op_adc_zpix(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  uint8_t value = mem_read(mem, absolute);
  w65c02_logic_adc(cpu, value);
}

static void op_and_imm(w65c02_t *cpu, mem_t *mem)
{
  cpu->a &= mem_read(mem, cpu->pc++);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->a &= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  cpu->a &= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_absy(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY_BOUNDARY_CHECK
  cpu->a &= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  cpu->a &= mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  cpu->a &= mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_zpyi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI_BOUNDARY_CHECK
  cpu->a &= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_zpi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPI
  cpu->a &= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_zpix(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  cpu->a &= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_asl_accu(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  uint8_t value = cpu->a;
  bool bit = value & 0b10000000;
  value = value << 1;
  cpu->a = value;
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_asl_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b10000000;
  value = value << 1;
  mem_write(mem, absolute, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_asl_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b10000000;
  value = value << 1;
  mem_write(mem, absolute, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_asl_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b10000000;
  value = value << 1;
  mem_write(mem, zeropage, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_asl_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b10000000;
  value = value << 1;
  mem_write(mem, zeropage, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_bbr0(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if ((value & 1) == 0) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbr1(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 1) & 1) == 0) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbr2(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 2) & 1) == 0) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbr3(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 3) & 1) == 0) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbr4(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 4) & 1) == 0) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbr5(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 5) & 1) == 0) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbr6(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 6) & 1) == 0) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbr7(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 7) & 1) == 0) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbs0(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if ((value & 1) == 1) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbs1(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 1) & 1) == 1) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbs2(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 2) & 1) == 1) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbs3(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 3) & 1) == 1) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbs4(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 4) & 1) == 1) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbs5(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 5) & 1) == 1) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbs6(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (((value >> 6) & 1) == 1) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bbs7(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  int8_t relative = mem_read(mem, cpu->pc++);
  if (zeropage == 0xFF && relative == -1) {
    panic("Suspicious $FF $FF $FF machine code!\n");
  }
  if (((value >> 7) & 1) == 1) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bcc(w65c02_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->status.c == 0) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bcs(w65c02_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->status.c == 1) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_beq(w65c02_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->status.z == 1) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bit_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  flag_overflow_bit(cpu, value);
  flag_negative_other(cpu, value);
  value &= cpu->a;
  flag_zero_other(cpu, value);
}

static void op_bit_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  flag_overflow_bit(cpu, value);
  flag_negative_other(cpu, value);
  value &= cpu->a;
  flag_zero_other(cpu, value);
}

static void op_bit_imm(w65c02_t *cpu, mem_t *mem)
{
  uint8_t value = mem_read(mem, cpu->pc++);
  value &= cpu->a;
  flag_zero_other(cpu, value);
}

static void op_bit_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  flag_overflow_bit(cpu, value);
  flag_negative_other(cpu, value);
  value &= cpu->a;
  flag_zero_other(cpu, value);
}

static void op_bit_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  flag_overflow_bit(cpu, value);
  flag_negative_other(cpu, value);
  value &= cpu->a;
  flag_zero_other(cpu, value);
}

static void op_bmi(w65c02_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->status.n == 1) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bne(w65c02_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->status.z == 0) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bpl(w65c02_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->status.n == 0) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bra(w65c02_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  cpu->cycles++;
  if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
    cpu->cycles++; /* Crossed a page boundary. */
  }
  cpu->pc += relative;
}

static void op_brk(w65c02_t *cpu, mem_t *mem)
{
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, (cpu->pc + 1) / 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, (cpu->pc + 1) % 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, cpu->p | 0x10); /* Break */
  cpu->status.i = 1;
  cpu->status.d = 0;
  cpu->pc  = mem_read(mem, W65C02_VECTOR_IRQ_LOW);
  cpu->pc += mem_read(mem, W65C02_VECTOR_IRQ_HIGH) * 256;
}

static void op_bvc(w65c02_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->status.v == 0) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_bvs(w65c02_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->status.v == 1) {
    cpu->cycles++;
    if ((cpu->pc & 0xFF00) != ((cpu->pc + relative) & 0xFF00)) {
      cpu->cycles++; /* Crossed a page boundary. */
    }
    cpu->pc += relative;
  }
}

static void op_clc(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->status.c = 0;
}

static void op_cld(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->status.d = 0;
}

static void op_cli(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->status.i = 0;
}

static void op_clv(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->status.v = 0;
}

static void op_cmp_imm(w65c02_t *cpu, mem_t *mem)
{
  uint8_t value = mem_read(mem, cpu->pc++);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_absy(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_zpyi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_zpi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPI
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_zpix(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cpx_imm(w65c02_t *cpu, mem_t *mem)
{
  uint8_t value = mem_read(mem, cpu->pc++);
  flag_negative_compare(cpu, cpu->x, value);
  flag_zero_compare(cpu, cpu->x, value);
  flag_carry_compare(cpu, cpu->x, value);
}

static void op_cpx_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->x, value);
  flag_zero_compare(cpu, cpu->x, value);
  flag_carry_compare(cpu, cpu->x, value);
}

static void op_cpx_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  flag_negative_compare(cpu, cpu->x, value);
  flag_zero_compare(cpu, cpu->x, value);
  flag_carry_compare(cpu, cpu->x, value);
}

static void op_cpy_imm(w65c02_t *cpu, mem_t *mem)
{
  uint8_t value = mem_read(mem, cpu->pc++);
  flag_negative_compare(cpu, cpu->y, value);
  flag_zero_compare(cpu, cpu->y, value);
  flag_carry_compare(cpu, cpu->y, value);
}

static void op_cpy_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->y, value);
  flag_zero_compare(cpu, cpu->y, value);
  flag_carry_compare(cpu, cpu->y, value);
}

static void op_cpy_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  flag_negative_compare(cpu, cpu->y, value);
  flag_zero_compare(cpu, cpu->y, value);
  flag_carry_compare(cpu, cpu->y, value);
}

static void op_dec(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->a--;
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_dec_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  value -= 1;
  mem_write(mem, absolute, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_dec_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  uint8_t value = mem_read(mem, absolute);
  value -= 1;
  mem_write(mem, absolute, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_dec_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value -= 1;
  mem_write(mem, zeropage, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_dec_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  value -= 1;
  mem_write(mem, zeropage, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_dex(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->x--;
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_dey(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->y--;
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_eor_imm(w65c02_t *cpu, mem_t *mem)
{
  cpu->a ^= mem_read(mem, cpu->pc++);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->a ^= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  cpu->a ^= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_absy(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY_BOUNDARY_CHECK
  cpu->a ^= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  cpu->a ^= mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  cpu->a ^= mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_zpyi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI_BOUNDARY_CHECK
  cpu->a ^= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_zpi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPI
  cpu->a ^= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_zpix(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  cpu->a ^= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_inc(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->a++;
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_inc_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  value += 1;
  mem_write(mem, absolute, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_inc_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  uint8_t value = mem_read(mem, absolute);
  value += 1;
  mem_write(mem, absolute, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_inc_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value += 1;
  mem_write(mem, zeropage, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_inc_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  value += 1;
  mem_write(mem, zeropage, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_inx(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->x++;
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_iny(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->y++;
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_jmp_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->pc = absolute;
}

static void op_jmp_absi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint16_t address = mem_read(mem, absolute);
  absolute += 1;
  address += mem_read(mem, absolute) * 256;
  cpu->pc = address;
}

static void op_jmp_abix(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  absolute += cpu->x;
  uint16_t address = mem_read(mem, absolute);
  absolute += 1;
  address += mem_read(mem, absolute) * 256;
  cpu->pc = address;
}

static void op_jsr(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, (cpu->pc - 1) / 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, (cpu->pc - 1) % 256);
  cpu->pc = absolute;
}

static void op_lda_imm(w65c02_t *cpu, mem_t *mem)
{
  cpu->a = mem_read(mem, cpu->pc++);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->a = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  cpu->a = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_absy(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY_BOUNDARY_CHECK
  cpu->a = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  cpu->a = mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  cpu->a = mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_zpyi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI_BOUNDARY_CHECK
  cpu->a = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_zpi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPI
  cpu->a = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_zpix(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  cpu->a = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ldx_imm(w65c02_t *cpu, mem_t *mem)
{
  cpu->x = mem_read(mem, cpu->pc++);
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_ldx_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->x = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_ldx_absy(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY_BOUNDARY_CHECK
  cpu->x = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_ldx_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  cpu->x = mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_ldx_zpy(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPY
  cpu->x = mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_ldy_imm(w65c02_t *cpu, mem_t *mem)
{
  cpu->y = mem_read(mem, cpu->pc++);
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_ldy_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->y = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_ldy_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  cpu->y = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_ldy_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  cpu->y = mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_ldy_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  cpu->y = mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_lsr_accu(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  uint8_t value = cpu->a;
  bool bit = value & 0b00000001;
  value = value >> 1;
  cpu->a = value;
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_lsr_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b00000001;
  value = value >> 1;
  mem_write(mem, absolute, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_lsr_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b00000001;
  value = value >> 1;
  mem_write(mem, absolute, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_lsr_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b00000001;
  value = value >> 1;
  mem_write(mem, zeropage, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_lsr_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b00000001;
  value = value >> 1;
  mem_write(mem, zeropage, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_nop(w65c02_t *cpu, mem_t *mem)
{
  (void)cpu;
  (void)mem;
}

static void op_nop_imm(w65c02_t *cpu, mem_t *mem)
{
  (void)mem_read(mem, cpu->pc++);
}

static void op_nop_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  (void)mem_read(mem, absolute);
}

static void op_nop_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  (void)mem_read(mem, zeropage);
}

static void op_nop_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  (void)mem_read(mem, zeropage);
}

static void op_ora_imm(w65c02_t *cpu, mem_t *mem)
{
  cpu->a |= mem_read(mem, cpu->pc++);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->a |= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  cpu->a |= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_absy(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY_BOUNDARY_CHECK
  cpu->a |= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  cpu->a |= mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  cpu->a |= mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_zpyi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI_BOUNDARY_CHECK
  cpu->a |= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_zpi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPI
  cpu->a |= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_zpix(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  cpu->a |= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_pha(w65c02_t *cpu, mem_t *mem)
{
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, cpu->a);
}

static void op_phx(w65c02_t *cpu, mem_t *mem)
{
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, cpu->x);
}

static void op_phy(w65c02_t *cpu, mem_t *mem)
{
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, cpu->y);
}

static void op_php(w65c02_t *cpu, mem_t *mem)
{
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, cpu->p | 0x10); /* Break */
}

static void op_pla(w65c02_t *cpu, mem_t *mem)
{
  cpu->a = mem_read(mem, MEM_PAGE_STACK + (++cpu->s));
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_plx(w65c02_t *cpu, mem_t *mem)
{
  cpu->x = mem_read(mem, MEM_PAGE_STACK + (++cpu->s));
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_ply(w65c02_t *cpu, mem_t *mem)
{
  cpu->y = mem_read(mem, MEM_PAGE_STACK + (++cpu->s));
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_plp(w65c02_t *cpu, mem_t *mem)
{
  cpu->p = (mem_read(mem, MEM_PAGE_STACK + (++cpu->s)) & ~0x30) |
           (cpu->p & 0x30);
}

static void op_rmb0(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value &= ~0x01;
  mem_write(mem, zeropage, value);
}

static void op_rmb1(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value &= ~0x02;
  mem_write(mem, zeropage, value);
}

static void op_rmb2(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value &= ~0x04;
  mem_write(mem, zeropage, value);
}

static void op_rmb3(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value &= ~0x08;
  mem_write(mem, zeropage, value);
}

static void op_rmb4(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value &= ~0x10;
  mem_write(mem, zeropage, value);
}

static void op_rmb5(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value &= ~0x20;
  mem_write(mem, zeropage, value);
}

static void op_rmb6(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value &= ~0x40;
  mem_write(mem, zeropage, value);
}

static void op_rmb7(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value &= ~0x80;
  mem_write(mem, zeropage, value);
}

static void op_rol_accu(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  uint8_t value = cpu->a;
  bool bit = value & 0b10000000;
  value = value << 1;
  if (cpu->status.c == 1) {
    value |= 0b00000001;
  }
  cpu->a = value;
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_rol_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b10000000;
  value = value << 1;
  if (cpu->status.c == 1) {
    value |= 0b00000001;
  }
  mem_write(mem, absolute, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_rol_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b10000000;
  value = value << 1;
  if (cpu->status.c == 1) {
    value |= 0b00000001;
  }
  mem_write(mem, absolute, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_rol_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b10000000;
  value = value << 1;
  if (cpu->status.c == 1) {
    value |= 0b00000001;
  }
  mem_write(mem, zeropage, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_rol_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b10000000;
  value = value << 1;
  if (cpu->status.c == 1) {
    value |= 0b00000001;
  }
  mem_write(mem, zeropage, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_ror_accu(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  uint8_t value = cpu->a;
  bool bit = value & 0b00000001;
  value = value >> 1;
  if (cpu->status.c == 1) {
    value |= 0b10000000;
  }
  cpu->a = value;
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_ror_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b00000001;
  value = value >> 1;
  if (cpu->status.c == 1) {
    value |= 0b10000000;
  }
  mem_write(mem, absolute, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_ror_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b00000001;
  value = value >> 1;
  if (cpu->status.c == 1) {
    value |= 0b10000000;
  }
  mem_write(mem, absolute, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_ror_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b00000001;
  value = value >> 1;
  if (cpu->status.c == 1) {
    value |= 0b10000000;
  }
  mem_write(mem, zeropage, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_ror_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b00000001;
  value = value >> 1;
  if (cpu->status.c == 1) {
    value |= 0b10000000;
  }
  mem_write(mem, zeropage, value);
  cpu->status.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_rti(w65c02_t *cpu, mem_t *mem)
{
  cpu->p = (mem_read(mem, MEM_PAGE_STACK + (++cpu->s)) & ~0x30) |
           (cpu->p & 0x30);
  cpu->pc  = mem_read(mem, MEM_PAGE_STACK + (++cpu->s));
  cpu->pc += mem_read(mem, MEM_PAGE_STACK + (++cpu->s)) * 256;
}

static void op_rts(w65c02_t *cpu, mem_t *mem)
{
  cpu->pc  = mem_read(mem, MEM_PAGE_STACK + (++cpu->s));
  cpu->pc += mem_read(mem, MEM_PAGE_STACK + (++cpu->s)) * 256;
  cpu->pc += 1;
}

static void op_sbc_imm(w65c02_t *cpu, mem_t *mem)
{
  uint8_t value = mem_read(mem, cpu->pc++);
  w65c02_logic_sbc(cpu, value);
}

static void op_sbc_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  w65c02_logic_sbc(cpu, value);
}

static void op_sbc_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  w65c02_logic_sbc(cpu, value);
}

static void op_sbc_absy(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  w65c02_logic_sbc(cpu, value);
}

static void op_sbc_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  w65c02_logic_sbc(cpu, value);
}

static void op_sbc_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  w65c02_logic_sbc(cpu, value);
}

static void op_sbc_zpyi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI_BOUNDARY_CHECK
  uint8_t value = mem_read(mem, absolute);
  w65c02_logic_sbc(cpu, value);
}

static void op_sbc_zpi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPI
  uint8_t value = mem_read(mem, absolute);
  w65c02_logic_sbc(cpu, value);
}

static void op_sbc_zpix(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  uint8_t value = mem_read(mem, absolute);
  w65c02_logic_sbc(cpu, value);
}

static void op_sec(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->status.c = 1;
}

static void op_sed(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->status.d = 1;
}

static void op_sei(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->status.i = 1;
}

static void op_smb0(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value |= 0x01;
  mem_write(mem, zeropage, value);
}

static void op_smb1(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value |= 0x02;
  mem_write(mem, zeropage, value);
}

static void op_smb2(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value |= 0x04;
  mem_write(mem, zeropage, value);
}

static void op_smb3(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value |= 0x08;
  mem_write(mem, zeropage, value);
}

static void op_smb4(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value |= 0x10;
  mem_write(mem, zeropage, value);
}

static void op_smb5(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value |= 0x20;
  mem_write(mem, zeropage, value);
}

static void op_smb6(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value |= 0x40;
  mem_write(mem, zeropage, value);
}

static void op_smb7(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value |= 0x80;
  mem_write(mem, zeropage, value);
}

static void op_sta_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  mem_write(mem, absolute, cpu->a);
}

static void op_sta_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  mem_write(mem, absolute, cpu->a);
}

static void op_sta_absy(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY
  mem_write(mem, absolute, cpu->a);
}

static void op_sta_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  mem_write(mem, zeropage, cpu->a);
}

static void op_sta_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  mem_write(mem, zeropage, cpu->a);
}

static void op_sta_zpyi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI
  mem_write(mem, absolute, cpu->a);
}

static void op_sta_zpi(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPI
  mem_write(mem, absolute, cpu->a);
}

static void op_sta_zpix(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  mem_write(mem, absolute, cpu->a);
}

static void op_stp(w65c02_t *cpu, mem_t *mem)
{
  (void)cpu;
  (void)mem;
  panic("STP instruction not implemented!\n");
}

static void op_stx_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  mem_write(mem, absolute, cpu->x);
}

static void op_stx_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  mem_write(mem, zeropage, cpu->x);
}

static void op_stx_zpy(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPY
  mem_write(mem, zeropage, cpu->x);
}

static void op_sty_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  mem_write(mem, absolute, cpu->y);
}

static void op_sty_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  mem_write(mem, zeropage, cpu->y);
}

static void op_sty_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  mem_write(mem, zeropage, cpu->y);
}

static void op_stz_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  mem_write(mem, absolute, 0);
}

static void op_stz_absx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  mem_write(mem, absolute, 0);
}

static void op_stz_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  mem_write(mem, zeropage, 0);
}

static void op_stz_zpx(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  mem_write(mem, zeropage, 0);
}

static void op_tax(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->x = cpu->a;
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_tay(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->y = cpu->a;
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_trb_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  flag_zero_other(cpu, value & cpu->a);
  value &= ~cpu->a;
  mem_write(mem, absolute, value);
}

static void op_trb_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  flag_zero_other(cpu, value & cpu->a);
  value &= ~cpu->a;
  mem_write(mem, zeropage, value);
}

static void op_tsb_abs(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  flag_zero_other(cpu, value & cpu->a);
  value |= cpu->a;
  mem_write(mem, absolute, value);
}

static void op_tsb_zp(w65c02_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  flag_zero_other(cpu, value & cpu->a);
  value |= cpu->a;
  mem_write(mem, zeropage, value);
}

static void op_tsx(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->x = cpu->s;
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_txa(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->a = cpu->x;
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_txs(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->s = cpu->x;
}

static void op_tya(w65c02_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->a = cpu->y;
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_wai(w65c02_t *cpu, mem_t *mem)
{
  (void)cpu;
  (void)mem;
  panic("WAI instruction not implemented!\n");
}



typedef void (*w65c02_operation_func_t)(w65c02_t *, mem_t *);

static w65c02_operation_func_t opcode_function[UINT8_MAX + 1] = {
  op_brk,      op_ora_zpix, op_nop_imm,  op_nop,  /* 0x00 -> 0x03 */
  op_tsb_zp,   op_ora_zp,   op_asl_zp,   op_rmb0, /* 0x04 -> 0x07 */
  op_php,      op_ora_imm,  op_asl_accu, op_nop,  /* 0x08 -> 0x0B */
  op_tsb_abs,  op_ora_abs,  op_asl_abs,  op_bbr0, /* 0x0C -> 0x0F */
  op_bpl,      op_ora_zpyi, op_ora_zpi,  op_nop,  /* 0x10 -> 0x13 */
  op_trb_zp,   op_ora_zpx,  op_asl_zpx,  op_rmb1, /* 0x14 -> 0x17 */
  op_clc,      op_ora_absy, op_inc,      op_nop,  /* 0x18 -> 0x1B */
  op_trb_abs,  op_ora_absx, op_asl_absx, op_bbr1, /* 0x1C -> 0x1F */
  op_jsr,      op_and_zpix, op_nop_imm,  op_nop,  /* 0x20 -> 0x23 */
  op_bit_zp,   op_and_zp,   op_rol_zp,   op_rmb2, /* 0x24 -> 0x27 */
  op_plp,      op_and_imm,  op_rol_accu, op_nop,  /* 0x28 -> 0x2B */
  op_bit_abs,  op_and_abs,  op_rol_abs,  op_bbr2, /* 0x2C -> 0x2F */
  op_bmi,      op_and_zpyi, op_and_zpi,  op_nop,  /* 0x30 -> 0x33 */
  op_bit_zpx,  op_and_zpx,  op_rol_zpx,  op_rmb3, /* 0x34 -> 0x37 */
  op_sec,      op_and_absy, op_dec,      op_nop,  /* 0x38 -> 0x3B */
  op_bit_absx, op_and_absx, op_rol_absx, op_bbr3, /* 0x3C -> 0x3F */
  op_rti,      op_eor_zpix, op_nop_imm,  op_nop,  /* 0x40 -> 0x43 */
  op_nop_zp,   op_eor_zp,   op_lsr_zp,   op_rmb4, /* 0x44 -> 0x47 */
  op_pha,      op_eor_imm,  op_lsr_accu, op_nop,  /* 0x48 -> 0x4B */
  op_jmp_abs,  op_eor_abs,  op_lsr_abs,  op_bbr4, /* 0x4C -> 0x4F */
  op_bvc,      op_eor_zpyi, op_eor_zpi,  op_nop,  /* 0x50 -> 0x53 */
  op_nop_zpx,  op_eor_zpx,  op_lsr_zpx,  op_rmb5, /* 0x54 -> 0x57 */
  op_cli,      op_eor_absy, op_phy,      op_nop,  /* 0x58 -> 0x5B */
  op_nop_abs,  op_eor_absx, op_lsr_absx, op_bbr5, /* 0x5C -> 0x5F */
  op_rts,      op_adc_zpix, op_nop_imm,  op_nop,  /* 0x60 -> 0x63 */
  op_stz_zp,   op_adc_zp,   op_ror_zp,   op_rmb6, /* 0x64 -> 0x67 */
  op_pla,      op_adc_imm,  op_ror_accu, op_nop,  /* 0x68 -> 0x6B */
  op_jmp_absi, op_adc_abs,  op_ror_abs,  op_bbr6, /* 0x6C -> 0x6F */
  op_bvs,      op_adc_zpyi, op_adc_zpi,  op_nop,  /* 0x70 -> 0x73 */
  op_stz_zpx,  op_adc_zpx,  op_ror_zpx,  op_rmb7, /* 0x74 -> 0x77 */
  op_sei,      op_adc_absy, op_ply,      op_nop,  /* 0x78 -> 0x7B */
  op_jmp_abix, op_adc_absx, op_ror_absx, op_bbr7, /* 0x7C -> 0x7F */
  op_bra,      op_sta_zpix, op_nop_imm,  op_nop,  /* 0x80 -> 0x83 */
  op_sty_zp,   op_sta_zp,   op_stx_zp,   op_smb0, /* 0x84 -> 0x87 */
  op_dey,      op_bit_imm,  op_txa,      op_nop,  /* 0x88 -> 0x8B */
  op_sty_abs,  op_sta_abs,  op_stx_abs,  op_bbs0, /* 0x8C -> 0x8F */
  op_bcc,      op_sta_zpyi, op_sta_zpi,  op_nop,  /* 0x90 -> 0x93 */
  op_sty_zpx,  op_sta_zpx,  op_stx_zpy,  op_smb1, /* 0x94 -> 0x97 */
  op_tya,      op_sta_absy, op_txs,      op_nop,  /* 0x98 -> 0x9B */
  op_stz_abs,  op_sta_absx, op_stz_absx, op_bbs1, /* 0x9C -> 0x9F */
  op_ldy_imm,  op_lda_zpix, op_ldx_imm,  op_nop,  /* 0xA0 -> 0xA3 */
  op_ldy_zp,   op_lda_zp,   op_ldx_zp,   op_smb2, /* 0xA4 -> 0xA7 */
  op_tay,      op_lda_imm,  op_tax,      op_nop,  /* 0xA8 -> 0xAB */
  op_ldy_abs,  op_lda_abs,  op_ldx_abs,  op_bbs2, /* 0xAC -> 0xAF */
  op_bcs,      op_lda_zpyi, op_lda_zpi,  op_nop,  /* 0xB0 -> 0xB3 */
  op_ldy_zpx,  op_lda_zpx,  op_ldx_zpy,  op_smb3, /* 0xB4 -> 0xB7 */
  op_clv,      op_lda_absy, op_tsx,      op_nop,  /* 0xB8 -> 0xBB */
  op_ldy_absx, op_lda_absx, op_ldx_absy, op_bbs3, /* 0xBC -> 0xBF */
  op_cpy_imm,  op_cmp_zpix, op_nop_imm,  op_nop,  /* 0xC0 -> 0xC3 */
  op_cpy_zp,   op_cmp_zp,   op_dec_zp,   op_smb4, /* 0xC4 -> 0xC7 */
  op_iny,      op_cmp_imm,  op_dex,      op_wai,  /* 0xC8 -> 0xCB */
  op_cpy_abs,  op_cmp_abs,  op_dec_abs,  op_bbs4, /* 0xCC -> 0xCF */
  op_bne,      op_cmp_zpyi, op_cmp_zpi,  op_nop,  /* 0xD0 -> 0xD3 */
  op_nop_zpx,  op_cmp_zpx,  op_dec_zpx,  op_smb5, /* 0xD4 -> 0xD7 */
  op_cld,      op_cmp_absy, op_phx,      op_stp,  /* 0xD8 -> 0xDB */
  op_nop_abs,  op_cmp_absx, op_dec_absx, op_bbs5, /* 0xDC -> 0xDF */
  op_cpx_imm,  op_sbc_zpix, op_nop_imm,  op_nop,  /* 0xE0 -> 0xE3 */
  op_cpx_zp,   op_sbc_zp,   op_inc_zp,   op_smb6, /* 0xE4 -> 0xE7 */
  op_inx,      op_sbc_imm,  op_nop,      op_nop,  /* 0xE8 -> 0xEB */
  op_cpx_abs,  op_sbc_abs,  op_inc_abs,  op_bbs6, /* 0xEC -> 0xEF */
  op_beq,      op_sbc_zpyi, op_sbc_zpi,  op_nop,  /* 0xF0 -> 0xF3 */
  op_nop_zpx,  op_sbc_zpx,  op_inc_zpx,  op_smb7, /* 0xF4 -> 0xF7 */
  op_sed,      op_sbc_absy, op_plx,      op_nop,  /* 0xF8 -> 0xFB */
  op_nop_abs,  op_sbc_absx, op_inc_absx, op_bbs7, /* 0xFC -> 0xFF */
};



/* Base cycles when page boundary crossing is not considered. */
static uint8_t opcode_cycles[UINT8_MAX + 1] = {
/* -0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -A -B -C -D -E -F */
    7, 6, 2, 1, 5, 3, 5, 5, 3, 2, 2, 1, 6, 4, 6, 5, /* 0x0- */
    2, 5, 5, 1, 5, 4, 6, 5, 2, 4, 2, 1, 6, 4, 6, 5, /* 0x1- */
    6, 6, 2, 1, 3, 3, 5, 5, 4, 2, 2, 1, 4, 4, 6, 5, /* 0x2- */
    2, 5, 5, 1, 4, 4, 6, 5, 2, 4, 2, 1, 4, 4, 6, 5, /* 0x3- */
    6, 6, 2, 1, 3, 3, 5, 5, 3, 2, 2, 1, 3, 4, 6, 5, /* 0x4- */
    2, 5, 5, 1, 4, 4, 6, 5, 2, 4, 3, 1, 4, 4, 6, 5, /* 0x5- */
    6, 6, 2, 1, 3, 3, 5, 5, 4, 2, 2, 1, 6, 4, 6, 5, /* 0x6- */
    2, 5, 5, 1, 4, 4, 6, 5, 2, 4, 4, 1, 6, 4, 6, 5, /* 0x7- */
    2, 6, 2, 1, 3, 3, 3, 5, 2, 2, 2, 1, 4, 4, 4, 5, /* 0x8- */
    2, 6, 5, 1, 4, 4, 4, 5, 2, 5, 2, 1, 4, 5, 5, 5, /* 0x9- */
    2, 6, 2, 1, 3, 3, 3, 5, 2, 2, 2, 1, 4, 4, 4, 5, /* 0xA- */
    2, 5, 5, 1, 4, 4, 4, 5, 2, 4, 2, 1, 4, 4, 4, 5, /* 0xB- */
    2, 6, 2, 1, 3, 3, 5, 5, 2, 2, 2, 3, 4, 4, 6, 5, /* 0xC- */
    2, 5, 5, 1, 4, 4, 6, 5, 2, 4, 3, 3, 4, 4, 7, 5, /* 0xD- */
    2, 6, 2, 1, 3, 3, 5, 5, 2, 2, 2, 1, 4, 4, 6, 5, /* 0xE- */
    2, 5, 5, 1, 4, 4, 6, 5, 2, 4, 4, 1, 4, 4, 7, 5, /* 0xF- */
};



void w65c02_execute(w65c02_t *cpu, mem_t *mem)
{
  uint8_t opcode;
  opcode = mem_read(mem, cpu->pc++);
  cpu->cycles += opcode_cycles[opcode];
  (opcode_function[opcode])(cpu, mem);
}



void w65c02_reset(w65c02_t *cpu, mem_t *mem)
{
  cpu->pc  = mem_read(mem, W65C02_VECTOR_RESET_LOW);
  cpu->pc += mem_read(mem, W65C02_VECTOR_RESET_HIGH) * 256;
  cpu->a = 0;
  cpu->x = 0;
  cpu->y = 0;
  cpu->s = 0xFD;
  cpu->status.n = 0;
  cpu->status.v = 0;
  cpu->status.b = 0;
  cpu->status.d = 0;
  cpu->status.i = 1;
  cpu->status.z = 0;
  cpu->status.c = 0;
  cpu->cycles = 0;
}



void w65c02_nmi(w65c02_t *cpu, mem_t *mem)
{
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, cpu->pc / 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, cpu->pc % 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, cpu->p);
  cpu->status.i = 1;
  cpu->pc  = mem_read(mem, W65C02_VECTOR_NMI_LOW);
  cpu->pc += mem_read(mem, W65C02_VECTOR_NMI_HIGH) * 256;
}



void w65c02_irq(w65c02_t *cpu, mem_t *mem)
{
  if (cpu->status.i == 1) {
    return; /* Masked. */
  }
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, cpu->pc / 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, cpu->pc % 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->s--, cpu->p);
  cpu->status.i = 1;
  cpu->pc  = mem_read(mem, W65C02_VECTOR_IRQ_LOW);
  cpu->pc += mem_read(mem, W65C02_VECTOR_IRQ_HIGH) * 256;
}



