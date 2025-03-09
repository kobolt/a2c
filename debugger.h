#ifndef _DEBUGGER_H
#define _DEBUGGER_H

#include <stdbool.h>
#include <stdint.h>
#include "iwm.h"
#include "mem.h"
#include "w65c02.h"

extern int32_t debugger_breakpoint;

bool debugger(w65c02_t *cpu, mem_t *mem, iwm_t *iwm);

#endif /* _DEBUGGER_H */
