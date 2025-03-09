#ifndef _W65C02_TRACE_H
#define _W65C02_TRACE_H

#include <stdio.h>
#include "w65c02.h"
#include "mem.h"

void w65c02_trace_init(void);
void w65c02_trace_add(w65c02_t *cpu, mem_t *mem);
void w65c02_trace_dump(FILE *fh);

#endif /* _W65C02_TRACE_H */
