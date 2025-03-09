#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "acia.h"
#include "console.h"
#include "debugger.h"
#include "iwm.h"
#include "mem.h"
#include "panic.h"
#include "w65c02.h"
#include "w65c02_trace.h"

#define DEFAULT_ROM_FILENAME "rom_ff.bin"



static w65c02_t cpu;
static mem_t mem;
static iwm_t iwm;
static acia_t acia1;
static acia_t acia2;

bool debugger_break = false;
bool warp_mode = false;
static char panic_msg[80];



static void sig_handler(int sig)
{
  switch (sig) {
  case SIGINT:
    debugger_break = true;
    return;
  }
}



void panic(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vsnprintf(panic_msg, sizeof(panic_msg), format, args);
  va_end(args);

  debugger_break = true;
}



static void display_help(const char *progname)
{
  fprintf(stdout, "Usage: %s <options> [floppy-disk-image]\n", progname);
  fprintf(stdout, "Options:\n"
     "  -h        Display this help.\n"
     "  -b        Break into debugger on start.\n"
     "  -w        Warp (full speed) mode.\n"
     "  -r FILE   Use FILE for ROM instead of the default.\n"
     "  -t TYPE   Force override TYPE of floppy disk image.\n"
     "  -s TTY    Assign TTY device for ACIA 2 communication.\n"
#ifdef HIRES_GUI_WINDOW
     "  -g        Run a window for HiRes graphics output in parallel.\n"
#endif /* HIRES_GUI_WINDOW */
     "\n");
  fprintf(stdout, "Floppy disk image types:\n"
    "  %d = Raw\n"
    "  %d = DOS sectoring/interleaving\n"
    "  %d = ProDOS sectoring/interleaving\n"
    "\n",
    DISK_INTERLEAVE_RAW,
    DISK_INTERLEAVE_DOS,
    DISK_INTERLEAVE_PRODOS);
  fprintf(stdout,
    "Default ROM filename: '%s'\n", DEFAULT_ROM_FILENAME);
  fprintf(stdout,
    "Using Ctrl+C will break into debugger, use 'q' from there to quit.\n\n");
}



int main(int argc, char *argv[])
{
  int c;
  int count = 0;
  char *rom_filename = DEFAULT_ROM_FILENAME;
  char *disk_filename = NULL;
  char *tty_device = NULL;
  int disk_type = 0;
  bool gui_enable = false;

  while ((c = getopt(argc, argv, "hbwr:t:s:g")) != -1) {
    switch (c) {
    case 'h':
      display_help(argv[0]);
      return EXIT_SUCCESS;

    case 'b':
      debugger_break = true;
      break;

    case 'w':
      warp_mode = true;
      break;

    case 'r':
      rom_filename = optarg;
      break;

    case 't':
      disk_type = atoi(optarg);
      break;

    case 's':
      tty_device = optarg;
      break;

    case 'g':
#ifdef HIRES_GUI_WINDOW
      gui_enable = true;
#endif /* HIRES_GUI_WINDOW */
      break;

    case '?':
    default:
      display_help(argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (argc > optind) {
    disk_filename = argv[optind];
  }

  w65c02_trace_init();
  panic_msg[0] = '\0';
  mem_init(&mem);
  iwm_init(&iwm, &mem);
  acia_init(&acia1, &mem, 0xC098, NULL);

  if (acia_init(&acia2, &mem, 0xC0A8, tty_device) != 0) {
    return EXIT_FAILURE;
  }

  if (mem_rom_load(&mem, rom_filename) != 0) {
    fprintf(stdout, "Loading of ROM '%s' failed!\n", rom_filename);
    return EXIT_FAILURE;
  }

  if (disk_filename != NULL) {
    if (iwm_disk_load(&iwm, 0, disk_filename, disk_type) != 0) {
      fprintf(stdout, "Loading of disk image '%s' failed!\n", disk_filename);
      return EXIT_FAILURE;
    }
  }

  if (console_init(&mem, gui_enable) != 0) {
    return EXIT_FAILURE;
  }

  signal(SIGINT, sig_handler);

  /* Setup timer to relax host CPU: */
  struct itimerval new;
  new.it_value.tv_sec = 0;
  new.it_value.tv_usec = 10000;
  new.it_interval.tv_sec = 0;
  new.it_interval.tv_usec = 10000;
  signal(SIGALRM, sig_handler);
  setitimer(ITIMER_REAL, &new, NULL);

  w65c02_reset(&cpu, &mem);

  while (1) {
    w65c02_trace_add(&cpu, &mem);
    w65c02_execute(&cpu, &mem);
    console_execute(&cpu, &mem);

    while (cpu.cycles > 0) {
      acia_execute(&acia1);
      acia_execute(&acia2);
      iwm_execute(&iwm);
      cpu.cycles--;
      count++;
    }

    if (debugger_breakpoint == cpu.pc) {
      debugger_break = true;
    }

    if (debugger_break) {
      console_pause();
      if (panic_msg[0] != '\0') {
        fprintf(stdout, "%s", panic_msg);
        panic_msg[0] = '\0';
      }
      debugger_break = debugger(&cpu, &mem, &iwm);
      if (! debugger_break) {
        console_resume();
      }
    }

    if (! warp_mode) {
      if (count > 10230) {
        count = 0;
        pause(); /* Wait for SIGALRM. */
      }
    }
  }

  return EXIT_SUCCESS;
}



