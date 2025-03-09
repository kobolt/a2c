#ifndef _ACIA_H
#define _ACIA_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mem.h"

#define ACIA_RX_FIFO_SIZE 1024
#define ACIA_TX_FIFO_SIZE 1024

typedef struct acia_s {
  uint16_t base_address;
  uint8_t control;
  uint8_t command;
  uint8_t status;

  int tty_fd;
  uint8_t rx_fifo[ACIA_RX_FIFO_SIZE];
  uint8_t tx_fifo[ACIA_TX_FIFO_SIZE];
  int rx_fifo_head;
  int tx_fifo_head;
  int rx_fifo_tail;
  int tx_fifo_tail;

  int cycle;
} acia_t;

int acia_init(acia_t *acia, mem_t *mem, uint16_t base_address,
  const char *tty_device);
void acia_execute(acia_t *acia);
void acia_trace_dump(FILE *fh);

#endif /* _ACIA_H */
