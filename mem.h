#ifndef _MEM_H
#define _MEM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MEM_RAM_MAIN_MAX 0x10000
#define MEM_RAM_AUX_MAX  0x10000
#define MEM_ROM_MAX      0x8000
#define MEM_IO_MAX       0x100

typedef struct mem_io_read_hook_s {
  void *cookie;
  uint8_t (*func)(void *, uint16_t);
} mem_io_read_hook_t;

typedef struct mem_io_write_hook_s {
  void *cookie;
  void (*func)(void *, uint16_t, uint8_t);
} mem_io_write_hook_t;

typedef struct mem_s {
  uint8_t main[MEM_RAM_MAIN_MAX]; /* Main RAM from 0x0000 to 0xFFFF. */
  uint8_t aux[MEM_RAM_AUX_MAX];   /* Auxiliary RAM from 0x0000 to 0xFFFF. */
  uint8_t rom[MEM_ROM_MAX];       /* ROM from 0xC000 to 0xFFFF. */

  mem_io_read_hook_t  io_read[MEM_IO_MAX];  /* I/O from 0xC000 to 0xC100. */
  mem_io_write_hook_t io_write[MEM_IO_MAX]; /* I/O from 0xC000 to 0xC100. */

  bool iou_disable;
  bool iou_dhires;
  bool iou_xy_mask;
  bool iou_vbl_mask;
  bool iou_x0_edge;
  bool iou_y0_edge;

  bool video_80_column;    /* 0 = Off, 1 = On */
  bool video_text_mode;    /* 0 = Off, 1 = On */
  bool video_mixed_mode;   /* 0 = Off, 1 = On */
  bool video_alt_char_set; /* 0 = Off, 1 = On */

  bool store80;  /* 0 = Off,   1 = On    */
  bool page2;    /* 0 = Off,   1 = On    */
  bool hires;    /* 0 = Off,   1 = On    */
  bool ram_rd;   /* 0 = Main,  1 = Aux   */
  bool ram_wrt;  /* 0 = Main,  1 = Aux   */
  bool alt_zp;   /* 0 = Main,  1 = Aux   */
  bool rom_bank; /* 0 = Low,   1 = High  */
  bool lcram;    /* 0 = ROM,   1 = RAM   */
  bool bnk2;     /* 0 = Bank1, 1 = Bank2 */
  bool wp; /* Write Protect */
  uint16_t rr_expect; /* Expected double address read for RAM write enable. */
} mem_t;

#define MEM_PAGE_STACK 0x100

void mem_init(mem_t *mem);
uint8_t mem_read(mem_t *mem, uint16_t address);
void mem_write(mem_t *mem, uint16_t address, uint8_t value);
int mem_rom_load(mem_t *mem, const char *filename);
void mem_ram_main_dump(FILE *fh, mem_t *mem, uint16_t start, uint16_t end);
void mem_ram_aux_dump(FILE *fh, mem_t *mem, uint16_t start, uint16_t end);
void mem_switch_dump(FILE *fh, mem_t * mem);

#endif /* _MEM_H */
