#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mem.h"
#include "panic.h"



static uint8_t mem_bank_select_read(void *mem, uint16_t address)
{
  switch (address) {
  case 0xC011:
    return ((mem_t *)mem)->bnk2 << 7;

  case 0xC012:
    return ((mem_t *)mem)->lcram << 7;

  case 0xC013:
    return ((mem_t *)mem)->ram_rd << 7;

  case 0xC014:
    return ((mem_t *)mem)->ram_wrt << 7;

  case 0xC016:
    return ((mem_t *)mem)->alt_zp << 7;

  case 0xC018:
    return ((mem_t *)mem)->store80 << 7;

  case 0xC01C:
    return ((mem_t *)mem)->page2 << 7;

  case 0xC01D:
    return ((mem_t *)mem)->hires << 7;

  case 0xC054:
    ((mem_t *)mem)->page2 = false;
    return 0;

  case 0xC055:
    ((mem_t *)mem)->page2 = true;
    return 0;

  case 0xC056:
    ((mem_t *)mem)->hires = false;
    return 0;

  case 0xC057:
    ((mem_t *)mem)->hires = true;
    return 0;

  case 0xC080:
    ((mem_t *)mem)->lcram = true;
    ((mem_t *)mem)->bnk2 = true;
    ((mem_t *)mem)->wp = true;
    return 0;

  case 0xC081:
    ((mem_t *)mem)->lcram = false;
    ((mem_t *)mem)->bnk2 = true;
    if (((mem_t *)mem)->rr_expect == address) {
      ((mem_t *)mem)->wp = false;
    } else {
      ((mem_t *)mem)->wp = true;
      ((mem_t *)mem)->rr_expect = address;
    }
    return 0;

  case 0xC082:
    ((mem_t *)mem)->lcram = false;
    ((mem_t *)mem)->bnk2 = true;
    ((mem_t *)mem)->wp = true;
    return 0;

  case 0xC083:
    ((mem_t *)mem)->lcram = true;
    ((mem_t *)mem)->bnk2 = true;
    if (((mem_t *)mem)->rr_expect == address) {
      ((mem_t *)mem)->wp = false;
    } else {
      ((mem_t *)mem)->wp = true;
      ((mem_t *)mem)->rr_expect = address;
    }
    return 0;

  case 0xC088:
    ((mem_t *)mem)->lcram = true;
    ((mem_t *)mem)->bnk2 = false;
    ((mem_t *)mem)->wp = true;
    return 0;

  case 0xC089:
    ((mem_t *)mem)->lcram = false;
    ((mem_t *)mem)->bnk2 = false;
    if (((mem_t *)mem)->rr_expect == address) {
      ((mem_t *)mem)->wp = false;
    } else {
      ((mem_t *)mem)->wp = true;
      ((mem_t *)mem)->rr_expect = address;
    }
    return 0;

  case 0xC08A:
    ((mem_t *)mem)->lcram = false;
    ((mem_t *)mem)->bnk2 = false;
    ((mem_t *)mem)->wp = true;
    return 0;

  case 0xC08B:
    ((mem_t *)mem)->lcram = true;
    ((mem_t *)mem)->bnk2 = false;
    if (((mem_t *)mem)->rr_expect == address) {
      ((mem_t *)mem)->wp = false;
    } else {
      ((mem_t *)mem)->wp = true;
      ((mem_t *)mem)->rr_expect = address;
    }
    return 0;

  default:
    return 0;
  }
}



static void mem_bank_select_write(void *mem, uint16_t address, uint8_t value)
{
  (void)value;

  switch (address) {
  case 0xC000:
    ((mem_t *)mem)->store80 = false;
    break;

  case 0xC001:
    ((mem_t *)mem)->store80 = true;
    break;

  case 0xC002:
    ((mem_t *)mem)->ram_rd = false;
    break;

  case 0xC003:
    ((mem_t *)mem)->ram_rd = true;
    break;

  case 0xC004:
    ((mem_t *)mem)->ram_wrt = false;
    break;

  case 0xC005:
    ((mem_t *)mem)->ram_wrt = true;
    break;

  case 0xC008:
    ((mem_t *)mem)->alt_zp = false;
    break;

  case 0xC009:
    ((mem_t *)mem)->alt_zp = true;
    break;

  case 0xC028:
    ((mem_t *)mem)->rom_bank = !((mem_t *)mem)->rom_bank;
    break;

  case 0xC054:
  case 0xC055:
  case 0xC056:
  case 0xC057:
  case 0xC080:
  case 0xC081:
  case 0xC082:
  case 0xC083:
  case 0xC088:
  case 0xC089:
  case 0xC08A:
  case 0xC08B:
    /* Pass through as a read for bank selection. */
    (void)mem_bank_select_read(mem, address);
    break;
  }
}



static void iou_write(void *mem, uint16_t address, uint8_t value)
{
  (void)value;

  switch (address) {
  case 0xC058:
    if (((mem_t *)mem)->iou_disable == false) {
      ((mem_t *)mem)->iou_xy_mask = false;
    }
    break;

  case 0xC059:
    if (((mem_t *)mem)->iou_disable == false) {
      ((mem_t *)mem)->iou_xy_mask = true;
    }
    break;

  case 0xC05A:
    if (((mem_t *)mem)->iou_disable == false) {
      ((mem_t *)mem)->iou_vbl_mask = false;
    }
    break;

  case 0xC05B:
    if (((mem_t *)mem)->iou_disable == false) {
      ((mem_t *)mem)->iou_vbl_mask = true;
    }
    break;

  case 0xC05C:
    if (((mem_t *)mem)->iou_disable == false) {
      ((mem_t *)mem)->iou_x0_edge = false;
    }
    break;

  case 0xC05D:
    if (((mem_t *)mem)->iou_disable == false) {
      ((mem_t *)mem)->iou_x0_edge = true;
    }
    break;

  case 0xC05E:
    if (((mem_t *)mem)->iou_disable == false) {
      ((mem_t *)mem)->iou_y0_edge = false;
    } else {
      ((mem_t *)mem)->iou_dhires = true;
    }
    break;

  case 0xC05F:
    if (((mem_t *)mem)->iou_disable == false) {
      ((mem_t *)mem)->iou_y0_edge = true;
    } else {
      ((mem_t *)mem)->iou_dhires = false;
    }
    break;

  case 0xC07E:
    ((mem_t *)mem)->iou_disable = true;
    break;

  case 0xC07F:
    ((mem_t *)mem)->iou_disable = false;
    break;
  }
}



static uint8_t iou_read(void *mem, uint16_t address)
{
  switch (address) {
  case 0xC040:
    return ((mem_t *)mem)->iou_xy_mask << 7;

  case 0xC041:
    return ((mem_t *)mem)->iou_vbl_mask << 7;

  case 0xC042:
    return ((mem_t *)mem)->iou_x0_edge << 7;

  case 0xC043:
    return ((mem_t *)mem)->iou_y0_edge << 7;

  case 0xC07E:
    return ((mem_t *)mem)->iou_disable << 7;

  case 0xC07F:
    return !((mem_t *)mem)->iou_dhires << 7; /* NOTE: Inverted! */

  case 0xC058:
  case 0xC059:
  case 0xC05A:
  case 0xC05B:
  case 0xC05C:
  case 0xC05D:
  case 0xC05E:
  case 0xC05F:
    /* Pass through as a write for switch selection. */
    iou_write(mem, address, 0);
    return 0;
  }

  return 0;
}



static void video_io_write(void* mem, uint16_t address, uint8_t value)
{
  (void)value;

  switch (address) {
  case 0xC00C:
    ((mem_t *)mem)->video_80_column = false;
    break;

  case 0xC00D:
    ((mem_t *)mem)->video_80_column = true;
    break;

  case 0xC00E:
    ((mem_t *)mem)->video_alt_char_set = false;
    break;

  case 0xC00F:
    ((mem_t *)mem)->video_alt_char_set = true;
    break;

  case 0xC050:
    ((mem_t *)mem)->video_text_mode = false;
    break;

  case 0xC051:
    ((mem_t *)mem)->video_text_mode = true;
    break;

  case 0xC052:
    ((mem_t *)mem)->video_mixed_mode = false;
    break;

  case 0xC053:
    ((mem_t *)mem)->video_mixed_mode = true;
    break;
  }
}



static uint8_t video_io_read(void *mem, uint16_t address)
{
  switch (address) {
  case 0xC01A:
    return ((mem_t *)mem)->video_text_mode << 7;

  case 0xC01B:
    return ((mem_t *)mem)->video_mixed_mode << 7;

  case 0xC01E:
    return ((mem_t *)mem)->video_alt_char_set << 7;

  case 0xC01F:
    return ((mem_t *)mem)->video_80_column << 7;

  case 0xC050:
  case 0xC051:
  case 0xC052:
  case 0xC053:
    /* Pass through as a write for switch selection. */
    video_io_write(mem, address, 0);
    return 0;

  default:
    return 0;
  }
}



void mem_init(mem_t *mem)
{
  int i;

  /* Initialize all memory: */
  for (i = 0; i < MEM_RAM_MAIN_MAX; i++) {
    mem->main[i] = 0xff;
  }
  for (i = 0; i < MEM_RAM_AUX_MAX; i++) {
    mem->aux[i] = 0xff;
  }
  for (i = 0; i < MEM_ROM_MAX; i++) {
    mem->rom[i] = 0x00;
  }
  for (i = 0; i < MEM_IO_MAX; i++) {
    mem->io_read[i].func = NULL;
    mem->io_read[i].cookie = NULL;
    mem->io_write[i].func = NULL;
    mem->io_write[i].cookie = NULL;
  }

  /* Banked ROM: */
  mem->rom_bank = false;
  mem->io_write[0x28].func = mem_bank_select_write;
  mem->io_write[0x28].cookie = mem;

  /* Banked 48K RAM: */
  mem->ram_rd = false;
  mem->ram_wrt = false;
  mem->io_read[0x13].func = mem_bank_select_read;
  mem->io_read[0x13].cookie = mem;
  mem->io_read[0x14].func = mem_bank_select_read;
  mem->io_read[0x14].cookie = mem;
  mem->io_write[0x02].func = mem_bank_select_write;
  mem->io_write[0x02].cookie = mem;
  mem->io_write[0x03].func = mem_bank_select_write;
  mem->io_write[0x03].cookie = mem;
  mem->io_write[0x04].func = mem_bank_select_write;
  mem->io_write[0x04].cookie = mem;
  mem->io_write[0x05].func = mem_bank_select_write;
  mem->io_write[0x05].cookie = mem;

  /* Banked ZP: */
  mem->alt_zp = false;
  mem->io_read[0x16].func = mem_bank_select_read;
  mem->io_read[0x16].cookie = mem;
  mem->io_write[0x08].func = mem_bank_select_write;
  mem->io_write[0x08].cookie = mem;
  mem->io_write[0x09].func = mem_bank_select_write;
  mem->io_write[0x09].cookie = mem;

  /* Banked $D000 Area: */
  mem->lcram = false;
  mem->bnk2 = false;
  mem->rr_expect = 0;
  mem->io_read[0x11].func = mem_bank_select_read;
  mem->io_read[0x11].cookie = mem;
  mem->io_read[0x12].func = mem_bank_select_read;
  mem->io_read[0x12].cookie = mem;
  mem->io_read[0x80].func = mem_bank_select_read;
  mem->io_read[0x80].cookie = mem;
  mem->io_read[0x81].func = mem_bank_select_read;
  mem->io_read[0x81].cookie = mem;
  mem->io_read[0x82].func = mem_bank_select_read;
  mem->io_read[0x82].cookie = mem;
  mem->io_read[0x83].func = mem_bank_select_read;
  mem->io_read[0x83].cookie = mem;
  mem->io_read[0x88].func = mem_bank_select_read;
  mem->io_read[0x88].cookie = mem;
  mem->io_read[0x89].func = mem_bank_select_read;
  mem->io_read[0x89].cookie = mem;
  mem->io_read[0x8A].func = mem_bank_select_read;
  mem->io_read[0x8A].cookie = mem;
  mem->io_read[0x8B].func = mem_bank_select_read;
  mem->io_read[0x8B].cookie = mem;
  mem->io_write[0x80].func = mem_bank_select_write;
  mem->io_write[0x80].cookie = mem;
  mem->io_write[0x81].func = mem_bank_select_write;
  mem->io_write[0x81].cookie = mem;
  mem->io_write[0x82].func = mem_bank_select_write;
  mem->io_write[0x82].cookie = mem;
  mem->io_write[0x83].func = mem_bank_select_write;
  mem->io_write[0x83].cookie = mem;
  mem->io_write[0x88].func = mem_bank_select_write;
  mem->io_write[0x88].cookie = mem;
  mem->io_write[0x89].func = mem_bank_select_write;
  mem->io_write[0x89].cookie = mem;
  mem->io_write[0x8A].func = mem_bank_select_write;
  mem->io_write[0x8A].cookie = mem;
  mem->io_write[0x8B].func = mem_bank_select_write;
  mem->io_write[0x8B].cookie = mem;

  /* Display Pages: */
  mem->io_read[0x18].func = mem_bank_select_read;
  mem->io_read[0x18].cookie = mem;
  mem->io_read[0x1C].func = mem_bank_select_read;
  mem->io_read[0x1C].cookie = mem;
  mem->io_read[0x1D].func = mem_bank_select_read;
  mem->io_read[0x1D].cookie = mem;
  mem->io_read[0x54].func = mem_bank_select_read;
  mem->io_read[0x54].cookie = mem;
  mem->io_read[0x55].func = mem_bank_select_read;
  mem->io_read[0x55].cookie = mem;
  mem->io_read[0x56].func = mem_bank_select_read;
  mem->io_read[0x56].cookie = mem;
  mem->io_read[0x57].func = mem_bank_select_read;
  mem->io_read[0x57].cookie = mem;
  mem->io_write[0x00].func = mem_bank_select_write;
  mem->io_write[0x00].cookie = mem;
  mem->io_write[0x01].func = mem_bank_select_write;
  mem->io_write[0x01].cookie = mem;
  mem->io_write[0x54].func = mem_bank_select_write;
  mem->io_write[0x54].cookie = mem;
  mem->io_write[0x55].func = mem_bank_select_write;
  mem->io_write[0x55].cookie = mem;
  mem->io_write[0x56].func = mem_bank_select_write;
  mem->io_write[0x56].cookie = mem;
  mem->io_write[0x57].func = mem_bank_select_write;
  mem->io_write[0x57].cookie = mem;

  /* IOU: */
  mem->iou_disable  = true;
  mem->iou_dhires   = false;
  mem->iou_xy_mask  = false;
  mem->iou_vbl_mask = false;
  mem->iou_x0_edge  = false;
  mem->iou_y0_edge  = false;
  mem->io_read[0x40].func = iou_read;
  mem->io_read[0x40].cookie = mem;
  mem->io_read[0x41].func = iou_read;
  mem->io_read[0x41].cookie = mem;
  mem->io_read[0x42].func = iou_read;
  mem->io_read[0x42].cookie = mem;
  mem->io_read[0x43].func = iou_read;
  mem->io_read[0x43].cookie = mem;
  mem->io_read[0x58].func = iou_read;
  mem->io_read[0x58].cookie = mem;
  mem->io_read[0x59].func = iou_read;
  mem->io_read[0x59].cookie = mem;
  mem->io_read[0x5A].func = iou_read;
  mem->io_read[0x5A].cookie = mem;
  mem->io_read[0x5B].func = iou_read;
  mem->io_read[0x5B].cookie = mem;
  mem->io_read[0x5C].func = iou_read;
  mem->io_read[0x5C].cookie = mem;
  mem->io_read[0x5D].func = iou_read;
  mem->io_read[0x5D].cookie = mem;
  mem->io_read[0x5E].func = iou_read;
  mem->io_read[0x5E].cookie = mem;
  mem->io_read[0x5F].func = iou_read;
  mem->io_read[0x5F].cookie = mem;
  mem->io_read[0x7E].func = iou_read;
  mem->io_read[0x7E].cookie = mem;
  mem->io_read[0x7F].func = iou_read;
  mem->io_read[0x7F].cookie = mem;
  mem->io_write[0x58].func = iou_write;
  mem->io_write[0x58].cookie = mem;
  mem->io_write[0x59].func = iou_write;
  mem->io_write[0x59].cookie = mem;
  mem->io_write[0x5A].func = iou_write;
  mem->io_write[0x5A].cookie = mem;
  mem->io_write[0x5B].func = iou_write;
  mem->io_write[0x5B].cookie = mem;
  mem->io_write[0x5C].func = iou_write;
  mem->io_write[0x5C].cookie = mem;
  mem->io_write[0x5D].func = iou_write;
  mem->io_write[0x5D].cookie = mem;
  mem->io_write[0x5E].func = iou_write;
  mem->io_write[0x5E].cookie = mem;
  mem->io_write[0x5F].func = iou_write;
  mem->io_write[0x5F].cookie = mem;
  mem->io_write[0x7E].func = iou_write;
  mem->io_write[0x7E].cookie = mem;
  mem->io_write[0x7F].func = iou_write;
  mem->io_write[0x7F].cookie = mem;

  /* Video Switches: */
  mem->video_80_column    = false;
  mem->video_text_mode    = false;
  mem->video_mixed_mode   = false;
  mem->video_alt_char_set = false;
  mem->io_read[0x1A].func = video_io_read;
  mem->io_read[0x1A].cookie = mem;
  mem->io_read[0x1B].func = video_io_read;
  mem->io_read[0x1B].cookie = mem;
  mem->io_read[0x1E].func = video_io_read;
  mem->io_read[0x1E].cookie = mem;
  mem->io_read[0x1F].func = video_io_read;
  mem->io_read[0x1F].cookie = mem;
  mem->io_read[0x50].func = video_io_read;
  mem->io_read[0x50].cookie = mem;
  mem->io_read[0x51].func = video_io_read;
  mem->io_read[0x51].cookie = mem;
  mem->io_read[0x52].func = video_io_read;
  mem->io_read[0x52].cookie = mem;
  mem->io_read[0x53].func = video_io_read;
  mem->io_read[0x53].cookie = mem;
  mem->io_write[0x0C].func = video_io_write;
  mem->io_write[0x0C].cookie = mem;
  mem->io_write[0x0D].func = video_io_write;
  mem->io_write[0x0D].cookie = mem;
  mem->io_write[0x0E].func = video_io_write;
  mem->io_write[0x0E].cookie = mem;
  mem->io_write[0x0F].func = video_io_write;
  mem->io_write[0x0F].cookie = mem;
  mem->io_write[0x50].func = video_io_write;
  mem->io_write[0x50].cookie = mem;
  mem->io_write[0x51].func = video_io_write;
  mem->io_write[0x51].cookie = mem;
  mem->io_write[0x52].func = video_io_write;
  mem->io_write[0x52].cookie = mem;
  mem->io_write[0x53].func = video_io_write;
  mem->io_write[0x53].cookie = mem;
}



uint8_t mem_read(mem_t *mem, uint16_t address)
{
  if (address < 0x200) { /* Zero Page & Stack Page */
    if (mem->alt_zp == false) {
      return mem->main[address];
    } else {
      return mem->aux[address];
    }

  } else if (address < 0xC000) { /* Low 48K Area */
    if (mem->store80 == true && address >= 0x400 && address < 0x800) {
      if (mem->page2 == true) {
        return mem->aux[address]; /* Text & LoRes Page 1X */
      } else {
        return mem->main[address]; /* Text & LoRes Page 1 */
      }
    }
    if (mem->store80 == true && address >= 0x2000 && address < 0x4000) {
      if (mem->page2 == true && mem->hires == true) {
        return mem->aux[address]; /* HiRes Page 1X */
      } else {
        return mem->main[address]; /* HiRes Page 1 */
      }
    }
    if (mem->ram_rd == false) {
      return mem->main[address];
    } else {
      return mem->aux[address];
    }

  } else if (address < 0xC100) { /* Hardware I/O Page */
    uint8_t hook = address - 0xC000;
    if (mem->io_read[hook].func != NULL) {
      return (mem->io_read[hook].func)(mem->io_read[hook].cookie, address);
    } else {
      /* panic("Unhandled read from I/O address $%04x\n", address); */
      return 0;
    }

  } else if (address < 0xD000) { /* INTCXROM */
    if (mem->rom_bank == false) {
      return mem->rom[address - 0xC000];
    } else {
      return mem->rom[address - 0x8000];
    }

  } else { /* $D000 Area */
    if (mem->lcram == false) { /* ROM */
      if (mem->rom_bank == false) {
        return mem->rom[address - 0xC000];
      } else {
        return mem->rom[address - 0x8000];
      }
    } else { /* RAM */
      if (address < 0xE000 && mem->bnk2 == false) {
        address -= 0x1000;
      }
      if (mem->alt_zp == false) {
        return mem->main[address];
      } else {
        return mem->aux[address];
      }
    }
  }
}



void mem_write(mem_t *mem, uint16_t address, uint8_t value)
{
  if (address < 0x200) { /* Zero Page & Stack Page */
    if (mem->alt_zp == false) {
      mem->main[address] = value;
    } else {
      mem->aux[address] = value;
    }

  } else if (address < 0xC000) { /* Low 48K Area */
    if (mem->store80 == true && address >= 0x400 && address < 0x800) {
      if (mem->page2 == true) {
        mem->aux[address] = value; /* Text & LoRes Page 1X */
      } else {
        mem->main[address] = value; /* Text & LoRes Page 1 */
      }
      return;
    }
    if (mem->store80 == true && address >= 0x2000 && address < 0x4000) {
      if (mem->page2 == true && mem->hires == true) {
        mem->aux[address] = value; /* HiRes Page 1X */
      } else {
        mem->main[address] = value; /* HiRes Page 1 */
      }
      return;
    }
    if (mem->ram_wrt == false) {
      mem->main[address] = value;
    } else {
      mem->aux[address] = value;
    }

  } else if (address < 0xC100) { /* Hardware I/O Page */
    uint8_t hook = address - 0xC000;
    if (mem->io_write[hook].func != NULL) {
      (mem->io_write[hook].func)(mem->io_write[hook].cookie, address, value);
    } else {
      /* panic("Unhandled write to I/O address $%04x ($%02x)\n",
        address, value); */
    }

  } else if (address < 0xD000) { /* INTCXROM */
    /* Read-Only ROM! */

  } else { /* $D000 Area */
    if (mem->lcram == true) { /* RAM */
      if (mem->wp == false) {
        if (address < 0xE000 && mem->bnk2 == false) {
          address -= 0x1000;
        }
        if (mem->alt_zp == false) {
          mem->main[address] = value;
        } else {
          mem->aux[address] = value;
        }
      } else {
        panic("Write protected RAM address $%04x ($%02x)\n", address, value);
      }
    }
  }
}



int mem_rom_load(mem_t *mem, const char *filename)
{
  FILE *fh;
  uint16_t address;
  int c;

  fh = fopen(filename, "rb");
  if (fh == NULL) {
    return -1;
  }

  address = 0;
  while ((c = fgetc(fh)) != EOF) {
    mem->rom[address] = c;
    address++;
    if (address > MEM_ROM_MAX) {
      break;
    }
  }

  fclose(fh);
  return 0;
}



static void mem_dump_16(FILE *fh, uint8_t m[], uint16_t start, uint16_t end)
{
  int i;
  uint16_t address;

  fprintf(fh, "$%04x   ", start & 0xFFF0);

  /* Hex */
  for (i = 0; i < 16; i++) {
    address = (start & 0xFFF0) + i;
    if ((address >= start) && (address <= end)) {
      fprintf(fh, "%02x ", m[address]);
    } else {
      fprintf(fh, "   ");
    }
    if (i % 4 == 3) {
      fprintf(fh, " ");
    }
  }

  /* Character */
  for (i = 0; i < 16; i++) {
    address = (start & 0xFFF0) + i;
    if ((address >= start) && (address <= end)) {
      if (isprint(m[address])) {
        fprintf(fh, "%c", m[address]);
      } else {
        fprintf(fh, ".");
      }
    } else {
      fprintf(fh, " ");
    }
  }

  fprintf(fh, "\n");
}



void mem_ram_main_dump(FILE *fh, mem_t *mem, uint16_t start, uint16_t end)
{
  int i;
  mem_dump_16(fh, &mem->main[0], start, end);
  for (i = (start & 0xFFF0) + 16; i <= end; i += 16) {
    mem_dump_16(fh, &mem->main[0], i, end);
  }
}



void mem_ram_aux_dump(FILE *fh, mem_t *mem, uint16_t start, uint16_t end)
{
  int i;
  mem_dump_16(fh, &mem->aux[0], start, end);
  for (i = (start & 0xFFF0) + 16; i <= end; i += 16) {
    mem_dump_16(fh, &mem->aux[0], i, end);
  }
}



void mem_switch_dump(FILE *fh, mem_t * mem)
{
  fprintf(fh, "Bank Switching:\n");
  fprintf(fh, "  80STORE : %d\n", mem->store80);
  fprintf(fh, "  Page 2  : %d\n", mem->page2);
  fprintf(fh, "  HiRes   : %d\n", mem->hires);
  fprintf(fh, "  RamRd   : %s\n", mem->ram_rd == 0 ? "Main" : "Aux");
  fprintf(fh, "  RamWrt  : %s\n", mem->ram_wrt == 0 ? "Main" : "Aux");
  fprintf(fh, "  AltZP   : %s\n", mem->alt_zp == 0 ? "Main" : "Aux");
  fprintf(fh, "  ROM Bank: %s\n", mem->rom_bank == 0 ? "Low" : "High");
  fprintf(fh, "  LCRam   : %s\n", mem->lcram == 0 ? "ROM" : "RAM");
  fprintf(fh, "  RAM Bank: %d\n", mem->bnk2 + 1);
  fprintf(fh, "  WrtProt : %d\n", mem->wp);

  fprintf(fh, "Video:\n");
  fprintf(fh, "  80 Columns  : %d\n", mem->video_80_column);
  fprintf(fh, "  Text Mode   : %d\n", mem->video_text_mode);
  fprintf(fh, "  Mixed Mode  : %d\n", mem->video_mixed_mode);
  fprintf(fh, "  Alt Char Set: %d\n", mem->video_alt_char_set);

  fprintf(fh, "IOU:\n");
  fprintf(fh, "  Disable : %d\n", mem->iou_disable);
  fprintf(fh, "  DHiRes  : %d\n", mem->iou_dhires);
  fprintf(fh, "  XY Mask : %d\n", mem->iou_xy_mask);
  fprintf(fh, "  VBL Mask: %d\n", mem->iou_vbl_mask);
  fprintf(fh, "  X0 Edge : %d\n", mem->iou_x0_edge);
  fprintf(fh, "  Y0 Edge : %d\n", mem->iou_y0_edge);
}



