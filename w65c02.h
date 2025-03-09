#ifndef _W65C02_H
#define _W65C02_H

#include <stdbool.h>
#include <stdint.h>
#include "mem.h"

typedef struct w65c02_s {
  uint16_t pc; /* Program Counter */
  uint8_t a;   /* Accumulator */
  uint8_t x;   /* X Index Register */
  uint8_t y;   /* Y Index Register */
  uint8_t s;   /* Stack Pointer */

  union {
    struct {
      uint8_t c : 1; /* Carry */
      uint8_t z : 1; /* Zero */
      uint8_t i : 1; /* IRQ Disable */
      uint8_t d : 1; /* Decimal */
      uint8_t b : 1; /* Break */
      uint8_t x : 1; /* Unused */
      uint8_t v : 1; /* Overflow */
      uint8_t n : 1; /* Negative */
    } status;
    uint8_t p; /* Processor Status */
  };

  uint8_t cycles;     /* Internal Cycle Counter */
} w65c02_t;

#define W65C02_VECTOR_NMI_LOW    0xFFFA
#define W65C02_VECTOR_NMI_HIGH   0xFFFB
#define W65C02_VECTOR_RESET_LOW  0xFFFC
#define W65C02_VECTOR_RESET_HIGH 0xFFFD
#define W65C02_VECTOR_IRQ_LOW    0xFFFE
#define W65C02_VECTOR_IRQ_HIGH   0xFFFF

void w65c02_execute(w65c02_t *cpu, mem_t *mem);
void w65c02_reset(w65c02_t *cpu, mem_t *mem);
void w65c02_nmi(w65c02_t *cpu, mem_t *mem);
void w65c02_irq(w65c02_t *cpu, mem_t *mem);

#endif /* _W65C02_H */
