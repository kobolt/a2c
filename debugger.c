#include "debugger.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "acia.h"
#include "console.h"
#include "iwm.h"
#include "mem.h"
#include "panic.h"
#include "w65c02.h"
#include "w65c02_trace.h"

#define DEBUGGER_ARGS 3

int32_t debugger_breakpoint = -1;



static void debugger_help(void)
{
  fprintf(stdout, "Debugger Commands:\n");
  fprintf(stdout, "  q               - Quit\n");
  fprintf(stdout, "  h               - Help\n");
  fprintf(stdout, "  c               - Continue\n");
  fprintf(stdout, "  s               - Step\n");
  fprintf(stdout, "  w               - Warp Mode Toggle\n");
  fprintf(stdout, "  f <file> [type] - Load Floppy Disk Image\n");
  fprintf(stdout, "  t               - Dump CPU Trace\n");
  fprintf(stdout, "  d <addr> [end]  - Dump Main RAM\n");
  fprintf(stdout, "  a <addr> [end]  - Dump Auxiliary RAM\n");
  fprintf(stdout, "  m               - Dump Soft Switches\n");
  fprintf(stdout, "  b <addr>        - CPU Breakpoint\n");
  fprintf(stdout, "  r               - CPU Reset\n");
  fprintf(stdout, "  i               - Dump IWM Trace\n");
  fprintf(stdout, "  z               - Dump ACIA Trace\n");
}



bool debugger(w65c02_t *cpu, mem_t *mem, iwm_t *iwm)
{
  char input[128];
  char *argv[DEBUGGER_ARGS];
  int argc;
  int value1, value2;

  fprintf(stdout, "\n");
  while (1) {
    fprintf(stdout, "$%04x> ", cpu->pc);

    if (fgets(input, sizeof(input), stdin) == NULL) {
      if (feof(stdin)) {
        exit(EXIT_SUCCESS);
      }
      continue;
    }

    if ((strlen(input) > 0) && (input[strlen(input) - 1] == '\n')) {
      input[strlen(input) - 1] = '\0'; /* Strip newline. */
    }

    argv[0] = strtok(input, " ");
    if (argv[0] == NULL) {
      continue;
    }

    for (argc = 1; argc < DEBUGGER_ARGS; argc++) {
      argv[argc] = strtok(NULL, " ");
      if (argv[argc] == NULL) {
        break;
      }
    }

    if (strncmp(argv[0], "q", 1) == 0) {
      exit(EXIT_SUCCESS);

    } else if (strncmp(argv[0], "?", 1) == 0) {
      debugger_help();

    } else if (strncmp(argv[0], "h", 1) == 0) {
      debugger_help();

    } else if (strncmp(argv[0], "c", 1) == 0) {
      return false;

    } else if (strncmp(argv[0], "s", 1) == 0) {
      return true;

    } else if (strncmp(argv[0], "w", 1) == 0) {
      if (warp_mode) {
        fprintf(stdout, "Warp Mode: Off\n");
        warp_mode = false;
      } else {
        fprintf(stdout, "Warp Mode: On\n");
        warp_mode = true;
      }

    } else if (strncmp(argv[0], "f", 1) == 0) {
      if (argc >= 3) {
        sscanf(argv[2], "%d", &value1);
        if (iwm_disk_load(iwm, 0, argv[1], value1) != 0) {
          fprintf(stdout, "Loading of disk image '%s' failed!\n", argv[1]);
        }
      } else if (argc >= 2) {
        if (iwm_disk_load(iwm, 0, argv[1], 0) != 0) {
          fprintf(stdout, "Loading of disk image '%s' failed!\n", argv[1]);
        }
      } else {
        fprintf(stdout, "Missing argument!\n");
      }

    } else if (strncmp(argv[0], "t", 1) == 0) {
      w65c02_trace_dump(stdout);

    } else if (strncmp(argv[0], "d", 1) == 0) {
      if (argc >= 3) {
        sscanf(argv[1], "%4x", &value1);
        sscanf(argv[2], "%4x", &value2);
        mem_ram_main_dump(stdout, mem, (uint16_t)value1, (uint16_t)value2);
      } else if (argc >= 2) {
        sscanf(argv[1], "%4x", &value1);
        value2 = value1 + 0xFF;
        if (value2 > 0xFFFF) {
          value2 = 0xFFFF; /* Truncate */
        }
        mem_ram_main_dump(stdout, mem, (uint16_t)value1, (uint16_t)value2);
      } else {
        fprintf(stdout, "Missing argument!\n");
      }

    } else if (strncmp(argv[0], "a", 1) == 0) {
      if (argc >= 3) {
        sscanf(argv[1], "%4x", &value1);
        sscanf(argv[2], "%4x", &value2);
        mem_ram_aux_dump(stdout, mem, (uint16_t)value1, (uint16_t)value2);
      } else if (argc >= 2) {
        sscanf(argv[1], "%4x", &value1);
        value2 = value1 + 0xFF;
        if (value2 > 0xFFFF) {
          value2 = 0xFFFF; /* Truncate */
        }
        mem_ram_aux_dump(stdout, mem, (uint16_t)value1, (uint16_t)value2);
      } else {
        fprintf(stdout, "Missing argument!\n");
      }

    } else if (strncmp(argv[0], "m", 1) == 0) {
      mem_switch_dump(stdout, mem);

    } else if (strncmp(argv[0], "b", 1) == 0) {
      if (argc >= 2) {
        if (sscanf(argv[1], "%4x", &value1) == 1) {
          debugger_breakpoint = (value1 & 0xFFFF);
          fprintf(stdout, "Breakpoint at $%04x set.\n",
            debugger_breakpoint);
        } else {
          fprintf(stdout, "Invalid argument!\n");
        }
      } else {
        if (debugger_breakpoint < 0) {
          fprintf(stdout, "Missing argument!\n");
        } else {
          fprintf(stdout, "Breakpoint at $%04x removed.\n",
            debugger_breakpoint);
        }
        debugger_breakpoint = -1;
      }

    } else if (strncmp(argv[0], "r", 1) == 0) {
      w65c02_reset(cpu, mem);

    } else if (strncmp(argv[0], "i", 1) == 0) {
      iwm_trace_dump(stdout);

    } else if (strncmp(argv[0], "z", 1) == 0) {
      acia_trace_dump(stdout);
    }
  }
}



