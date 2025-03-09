#ifndef _CONSOLE_H
#define _CONSOLE_H

#include <stdbool.h>
#include "mem.h"
#include "w65c02.h"

void console_pause(void);
void console_resume(void);
void console_exit(void);
int console_init(mem_t *mem, bool gui_enable);
void console_execute(w65c02_t *cpu, mem_t *mem);

#endif /* _CONSOLE_H */
