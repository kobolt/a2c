#include "iwm.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "panic.h"

#define IWM_TRACE_BUFFER_SIZE 1024
#define IWM_TRACE_MAX 80

static char iwm_trace_buffer[IWM_TRACE_BUFFER_SIZE][IWM_TRACE_MAX];
static int iwm_trace_buffer_n = 0;

static const uint8_t disk_gcr_map[64] = {
  0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
  0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
  0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
  0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
  0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
  0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
  0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
  0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

static const int disk_interleave_dos[DISK_SECTORS] = {
  0x0, 0x7, 0xE, 0x6, 0xD, 0x5, 0xC, 0x4,
  0xB, 0x3, 0xA, 0x2, 0x9, 0x1, 0x8, 0xF,
};

static const int disk_interleave_prodos[DISK_SECTORS] = {
  0x0, 0x8, 0x1, 0x9, 0x2, 0xA, 0x3, 0xB,
  0x4, 0xC, 0x5, 0xD, 0x6, 0xE, 0x7, 0xF,
};



static void iwm_trace(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vsnprintf(iwm_trace_buffer[iwm_trace_buffer_n],
    IWM_TRACE_MAX, format, args);
  va_end(args);

  iwm_trace_buffer_n++;
  if (iwm_trace_buffer_n >= IWM_TRACE_BUFFER_SIZE) {
    iwm_trace_buffer_n = 0;
  }
}



static void disk_odd_even_encode(uint8_t byte, uint8_t *xx, uint8_t *yy)
{
  *xx = 0xAA;
  *yy = 0xAA;
  *xx += ((byte >> 1) & 0x40);
  *xx += ((byte >> 1) & 0x10);
  *xx += ((byte >> 1) & 0x04);
  *xx += ((byte >> 1) & 0x01);
  *yy += ( byte       & 0x40);
  *yy += ( byte       & 0x10);
  *yy += ( byte       & 0x04);
  *yy += ( byte       & 0x01);
}



static void disk_sector_to_nibble(uint8_t sector[], uint8_t nibble[])
{
  int i;

  for (i = 0; i < 256; i++) {
    /* High 6 bits: */
    nibble[i + 86] = sector[i] >> 2;

    /* Low 2 bits: */
    switch (i / 86) {
    case 0:
      nibble[i % 86]  = (sector[i] >> 1) & 0x01; /* Bit 1 to 0 */
      nibble[i % 86] |= (sector[i] << 1) & 0x02; /* Bit 0 to 1 */
      break;
    case 1:
      nibble[i % 86] |= (sector[i] << 1) & 0x04; /* Bit 1 to 2 */
      nibble[i % 86] |= (sector[i] << 3) & 0x08; /* Bit 0 to 3 */
      break;
    case 2:
      nibble[i % 86] |= (sector[i] << 3) & 0x10; /* Bit 1 to 4 */
      nibble[i % 86] |= (sector[i] << 5) & 0x20; /* Bit 0 to 5 */
      break;
    }
  }
}



static void disk_load_track(iwm_t *iwm, int disk_no, int track_no)
{
  int i;
  int sector_no;
  int byte_no = 0;
  uint8_t nibble[342];
  uint8_t data;
  uint8_t xx;
  uint8_t yy;
  int offset;

  for (sector_no = 0; sector_no < DISK_SECTORS; sector_no++) {
    /* Address Field Prologue */
    iwm->disk[disk_no].track[byte_no++] = 0xD5;
    iwm->disk[disk_no].track[byte_no++] = 0xAA;
    iwm->disk[disk_no].track[byte_no++] = 0x96;

    /* Address Field */
    disk_odd_even_encode(iwm->disk[disk_no].volume_no, &xx, &yy); /* Volume */
    iwm->disk[disk_no].track[byte_no++] = xx;
    iwm->disk[disk_no].track[byte_no++] = yy;
    disk_odd_even_encode(track_no, &xx, &yy); /* Track */
    iwm->disk[disk_no].track[byte_no++] = xx;
    iwm->disk[disk_no].track[byte_no++] = yy;
    disk_odd_even_encode(sector_no, &xx, &yy); /* Sector */
    iwm->disk[disk_no].track[byte_no++] = xx;
    iwm->disk[disk_no].track[byte_no++] = yy;
    disk_odd_even_encode(iwm->disk[disk_no].volume_no ^ track_no ^ sector_no,
      &xx, &yy); /* Checksum */
    iwm->disk[disk_no].track[byte_no++] = xx;
    iwm->disk[disk_no].track[byte_no++] = yy;

    /* Address Field Epilogue */
    iwm->disk[disk_no].track[byte_no++] = 0xDE;
    iwm->disk[disk_no].track[byte_no++] = 0xAA;
    iwm->disk[disk_no].track[byte_no++] = 0xEB;

    /* Data Field Prologue */
    iwm->disk[disk_no].track[byte_no++] = 0xD5;
    iwm->disk[disk_no].track[byte_no++] = 0xAA;
    iwm->disk[disk_no].track[byte_no++] = 0xAD;

    /* Data Field */
    switch (iwm->disk[disk_no].interleave) {
    case DISK_INTERLEAVE_DOS:
      offset = (track_no * DISK_SECTORS * DISK_SECTOR_SIZE) +
               (disk_interleave_dos[sector_no] * DISK_SECTOR_SIZE);
      break;

    case DISK_INTERLEAVE_PRODOS:
      offset = (track_no * DISK_SECTORS * DISK_SECTOR_SIZE) +
               (disk_interleave_prodos[sector_no] * DISK_SECTOR_SIZE);
      break;

    case DISK_INTERLEAVE_RAW:
    default:
      offset = (track_no * DISK_SECTORS * DISK_SECTOR_SIZE) +
               (sector_no * DISK_SECTOR_SIZE);
      break;
    }
    disk_sector_to_nibble(&iwm->disk[disk_no].data[offset], nibble);
    data = 0;
    for (i = 0; i < 342; i++) {
      iwm->disk[disk_no].track[byte_no++] = disk_gcr_map[data ^ nibble[i]];
      data = nibble[i];
    }
    iwm->disk[disk_no].track[byte_no++] = disk_gcr_map[data]; /* Checksum */

    /* Data Field Epilogue */
    iwm->disk[disk_no].track[byte_no++] = 0xDE;
    iwm->disk[disk_no].track[byte_no++] = 0xAA;
    iwm->disk[disk_no].track[byte_no++] = 0xEB;
  }
}



static uint8_t disk_spin_and_read(iwm_t *iwm, int disk_no)
{
  uint8_t data;

  data = iwm->disk[disk_no].track[iwm->disk[disk_no].track_n];
  iwm->disk[disk_no].track_n++;
  if (iwm->disk[disk_no].track_n >= DISK_TRACK_SIZE) {
    iwm->disk[disk_no].track_n = 0;
  }

  return data;
}



static void iwm_switch(iwm_t *iwm, uint16_t address)
{
  switch (address) {
  case 0xC0E0:
    iwm->ph0 = false;
    break;
  case 0xC0E1:
    iwm->ph0 = true;
    break;
  case 0xC0E2:
    iwm->ph1 = false;
    break;
  case 0xC0E3:
    iwm->ph1 = true;
    break;
  case 0xC0E4:
    iwm->ph2 = false;
    break;
  case 0xC0E5:
    iwm->ph2 = true;
    break;
  case 0xC0E6:
    iwm->ph3 = false;
    break;
  case 0xC0E7:
    iwm->ph3 = true;
    break;
  case 0xC0E8:
    iwm->motor_on = false;
    break;
  case 0xC0E9:
    iwm->motor_on = true;
    break;
  case 0xC0EA:
    iwm->drive_select = false;
    break;
  case 0xC0EB:
    iwm->drive_select = true;
    break;
  case 0xC0EC:
    iwm->l6 = false;
    break;
  case 0xC0ED:
    iwm->l6 = true;
    break;
  case 0xC0EE:
    iwm->l7 = false;
    break;
  case 0xC0EF:
    iwm->l7 = true;
    break;
  }
}



static uint8_t iwm_read(void *iwm, uint16_t address)
{
  int disk_no;
  iwm_switch(iwm, address);

  /* Read is done on even-numbered addresses only. */
  if ((address & 1) == 1) {
    return 0;
  }

  if (((iwm_t *)iwm)->l6 == false && ((iwm_t *)iwm)->l7 == false) {
    if (((iwm_t *)iwm)->motor_on == false) {
      return 0xFF;
    } else {
      disk_no = ((iwm_t *)iwm)->drive_select ? 1 : 0;
      if (((iwm_t *)iwm)->disk[disk_no].loaded) {
        ((iwm_t *)iwm)->data = disk_spin_and_read(iwm, disk_no);
        iwm_trace("[R] Data=0x%02x\n", ((iwm_t *)iwm)->data);
        return ((iwm_t *)iwm)->data;
      } else {
        return 0xFF;
      }
    }

  } else if (((iwm_t *)iwm)->l6 == false && ((iwm_t *)iwm)->l7 == true) {
    iwm_trace("[R] Handshake=0x%02x\n", ((iwm_t *)iwm)->handshake);
    return ((iwm_t *)iwm)->handshake;

  } else if (((iwm_t *)iwm)->l6 == true && ((iwm_t *)iwm)->l7 == false) {
    ((iwm_t *)iwm)->status = 0;
    ((iwm_t *)iwm)->status |= (((iwm_t *)iwm)->mode & 0x1F);
    ((iwm_t *)iwm)->status |= (((iwm_t *)iwm)->motor_on << 5);
    /* iwm_trace("[R] Status=0x%02x\n", ((iwm_t *)iwm)->status); */
    return ((iwm_t *)iwm)->status;
  }

  return 0;
}



static void iwm_write(void *iwm, uint16_t address, uint8_t value)
{
  iwm_switch(iwm, address);

  /* Write is done on odd-numbered addresses only. */
  if ((address & 1) == 0) {
    return;
  }

  if (((iwm_t *)iwm)->l6 == true && ((iwm_t *)iwm)->l7 == true) {
    if (((iwm_t *)iwm)->motor_on == false) {
      iwm_trace("[W] Mode=0x%02x\n", value);
      ((iwm_t *)iwm)->mode = value;
    } else {
      iwm_trace("[W] Data=0x%02x\n", value);
      /* NOTE: Write of data is not implemented... */
    }
  }
}



void iwm_init(iwm_t *iwm, mem_t *mem)
{
  int i;

  memset(iwm, 0, sizeof(iwm_t));

  for (i = 0xE0; i <= 0xEF; i++) {
    mem->io_read[i].func = iwm_read;
    mem->io_read[i].cookie = iwm;
    mem->io_write[i].func = iwm_write;
    mem->io_write[i].cookie = iwm;
  }

  for (i = 0; i < IWM_TRACE_BUFFER_SIZE; i++) {
    iwm_trace_buffer[i][0] = '\0';
  }
  iwm_trace_buffer_n = 0;
}



void iwm_execute(iwm_t *iwm)
{
  int prev_track = iwm->stepper_pos / 2;

  /* Simulate build-up of energy in the drive stepper motor phases. */
  if (iwm->ph0) {
    iwm->ph0_energy++;
  } else {
    iwm->ph0_energy = 0;
  }
  if (iwm->ph1) {
    iwm->ph1_energy++;
  } else {
    iwm->ph1_energy = 0;
  }
  if (iwm->ph2) {
    iwm->ph2_energy++;
  } else {
    iwm->ph2_energy = 0;
  }
  if (iwm->ph3) {
    iwm->ph3_energy++;
  } else {
    iwm->ph3_energy = 0;
  }

  /* Simulate movement of drive stepper motor. */
  switch (iwm->stepper_pos % 4) {
  case 0:
    if (iwm->ph0_energy == 0) {
      if (iwm->ph1_energy > 1000) {
        iwm->stepper_pos++;
      } else if (iwm->ph3_energy > 1000) {
        iwm->stepper_pos--;
        if (iwm->stepper_pos < 0) {
          iwm->stepper_pos = 0;
        }
      }
    }
    break;

  case 1:
    if (iwm->ph1_energy == 0) {
      if (iwm->ph2_energy > 1000) {
        iwm->stepper_pos++;
      } else if (iwm->ph0_energy > 1000) {
        iwm->stepper_pos--;
      }
    }
    break;

  case 2:
    if (iwm->ph2_energy == 0) {
      if (iwm->ph3_energy > 1000) {
        iwm->stepper_pos++;
      } else if (iwm->ph1_energy > 1000) {
        iwm->stepper_pos--;
      }
    }
    break;

  case 3:
    if (iwm->ph3_energy == 0) {
      if (iwm->ph0_energy > 1000) {
        iwm->stepper_pos++;
      } else if (iwm->ph2_energy > 1000) {
        iwm->stepper_pos--;
      }
    }
    break;
  }

  /* Change track if the stepper motor has moved and it is enabled. */
  if (iwm->motor_on) {
    if (prev_track != iwm->stepper_pos / 2) {
      iwm_trace("[E] T%d -> T%d\n", prev_track, iwm->stepper_pos / 2);
      disk_load_track(iwm, 0, iwm->stepper_pos / 2);
    }
  }
}



void iwm_trace_dump(FILE *fh)
{
  int i;

  for (i = iwm_trace_buffer_n; i < IWM_TRACE_BUFFER_SIZE; i++) {
    if (iwm_trace_buffer[i][0] != '\0') {
      fprintf(fh, iwm_trace_buffer[i]);
    }
  }
  for (i = 0; i < iwm_trace_buffer_n; i++) {
    if (iwm_trace_buffer[i][0] != '\0') {
      fprintf(fh, iwm_trace_buffer[i]);
    }
  }
}



static disk_interleave_t iwm_disk_type_detect(const char *filename,
  uint8_t data[])
{
  const char *ext;

  /* Prioritize to use the filename extension if it is recognized. */
  ext = strrchr(filename, '.');
  if (ext != NULL && ext != filename) {
    if ((strcmp(ext, ".do") == 0) ||
        (strcmp(ext, ".DO") == 0)) { /* DOS extension. */
      return DISK_INTERLEAVE_DOS;

    } else if ((strcmp(ext, ".po") == 0) ||
               (strcmp(ext, ".PO") == 0)) { /* ProDOS extension. */
      return DISK_INTERLEAVE_PRODOS;
    }
  }

  /* Check for DOS signature. */
  if (data[0x0] == 0x01 &&
      data[0x1] == 0xA5 &&
      data[0x2] == 0x27 &&
      data[0x3] == 0xC9 &&
      data[0x4] == 0x09) {
    return DISK_INTERLEAVE_DOS;
  }

  /* Check for ProDOS signature. */
  if (data[0x0] == 0x01 &&
      data[0x1] == 0x38 &&
      data[0x2] == 0xB0 &&
      data[0x3] == 0x03 &&
      data[0x4] == 0x4C) {
    return DISK_INTERLEAVE_PRODOS;
  }

  return DISK_INTERLEAVE_RAW; /* Unknown... */
}



int iwm_disk_load(iwm_t *iwm, int disk_no, const char *filename,
  int interleave_override)
{
  FILE *fh;
  int n;
  int c;

  iwm->disk[disk_no].loaded = false;

  if (filename == NULL) {
    return 0; /* Just unload the image. */
  }

  fh = fopen(filename, "rb");
  if (fh == NULL) {
    return -1;
  }

  if (disk_no != 0 && disk_no != 1) {
    return -2;
  }

  n = 0;
  while ((c = fgetc(fh)) != EOF) {
    iwm->disk[disk_no].data[n] = c;
    n++;
    if (n > DISK_SIZE) {
      break;
    }
  }
  fclose(fh);

  if (interleave_override > 0) {
    iwm->disk[disk_no].interleave = interleave_override;
  } else {
    /* Try to automatically determine the type. */
    iwm->disk[disk_no].interleave = iwm_disk_type_detect(filename,
      iwm->disk[disk_no].data);
  }

  if (iwm->disk[disk_no].interleave == DISK_INTERLEAVE_DOS) {
    /* Set volume number from DOS 3.3 VTOC structure on T11/S0. */
    iwm->disk[disk_no].volume_no = iwm->disk[disk_no].data[0x11006];
  }

  /* Load T0 now since there will initially be no track change detected. */
  disk_load_track(iwm, 0, 0);

  iwm->disk[disk_no].loaded = true;
  return 0;
}



