#ifndef _IWM_H
#define _IWM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mem.h"

#define DISK_TRACKS 35
#define DISK_SECTORS 16
#define DISK_SECTOR_SIZE 256
#define DISK_SIZE (DISK_TRACKS * DISK_SECTORS * DISK_SECTOR_SIZE)
#define DISK_TRACK_SIZE 5808 /* With address/data fields and GCR encoding. */

typedef enum {
  DISK_INTERLEAVE_RAW    = 1,
  DISK_INTERLEAVE_DOS    = 2,
  DISK_INTERLEAVE_PRODOS = 3,
} disk_interleave_t;

typedef struct disk_s {
  bool loaded;
  int track_n;
  uint8_t volume_no;
  disk_interleave_t interleave;
  uint8_t data[DISK_SIZE];
  uint8_t track[DISK_TRACK_SIZE];
} disk_t;

typedef struct iwm_s {
  bool ph0;
  bool ph1;
  bool ph2;
  bool ph3;
  bool motor_on;
  bool drive_select;
  bool l6;
  bool l7;
  uint8_t mode;
  uint8_t data;
  uint8_t status;
  uint8_t handshake;
  int ph0_energy;
  int ph1_energy;
  int ph2_energy;
  int ph3_energy;
  int stepper_pos;
  disk_t disk[2];
} iwm_t;

void iwm_init(iwm_t *iwm, mem_t *mem);
void iwm_trace_dump(FILE *fh);
void iwm_execute(iwm_t *iwm);
int iwm_disk_load(iwm_t *iwm, int disk_no, const char *filename,
  int interleave_override);

#endif /* _IWM_H */
