#include "acia.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "mem.h"
#include "panic.h"

#define ACIA_TRACE_BUFFER_SIZE 1024
#define ACIA_TRACE_MAX 80

static char acia_trace_buffer[ACIA_TRACE_BUFFER_SIZE][ACIA_TRACE_MAX];
static int acia_trace_buffer_n = 0;



static void acia_trace(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vsnprintf(acia_trace_buffer[acia_trace_buffer_n],
    ACIA_TRACE_MAX, format, args);
  va_end(args);

  acia_trace_buffer_n++;
  if (acia_trace_buffer_n >= ACIA_TRACE_BUFFER_SIZE) {
    acia_trace_buffer_n = 0;
  }
}



static bool acia_rx_fifo_read(acia_t *acia, uint8_t *byte)
{
  if (acia->rx_fifo_tail == acia->rx_fifo_head) {
    return false; /* Empty */
  }

  *byte = acia->rx_fifo[acia->rx_fifo_tail];
  acia->rx_fifo_tail = (acia->rx_fifo_tail + 1) % ACIA_RX_FIFO_SIZE;

  return true;
}



static void acia_rx_fifo_write(acia_t *acia, uint8_t byte)
{
  if (((acia->rx_fifo_head + 1) % ACIA_RX_FIFO_SIZE)
    == acia->rx_fifo_tail) {
    return; /* Full */
  }

  acia->rx_fifo[acia->rx_fifo_head] = byte;
  acia->rx_fifo_head = (acia->rx_fifo_head + 1) % ACIA_RX_FIFO_SIZE;
}



static bool acia_tx_fifo_read(acia_t *acia, uint8_t *byte)
{
  if (acia->tx_fifo_tail == acia->tx_fifo_head) {
    return false; /* Empty */
  }

  *byte = acia->tx_fifo[acia->tx_fifo_tail];
  acia->tx_fifo_tail = (acia->tx_fifo_tail + 1) % ACIA_TX_FIFO_SIZE;

  return true;
}



static void acia_tx_fifo_write(acia_t *acia, uint8_t byte)
{
  if (((acia->tx_fifo_head + 1) % ACIA_TX_FIFO_SIZE)
    == acia->tx_fifo_tail) {
    return; /* Full */
  }

  acia->tx_fifo[acia->tx_fifo_head] = byte;
  acia->tx_fifo_head = (acia->tx_fifo_head + 1) % ACIA_TX_FIFO_SIZE;
}



static void acia_update_tty_settings(acia_t *acia)
{
  struct termios tios;
  speed_t speed;

  if (acia->tty_fd == -1) {
    return;
  }

  if (tcgetattr(acia->tty_fd, &tios) == -1) {
    panic("tcgetattr() failed with errno: %d\n", errno);
    return;
  }

  cfmakeraw(&tios);
  tios.c_cflag &= ~(CSIZE | PARENB | PARODD);

  /* Baud Rate */
  switch (acia->control & 0xF) {
  case 0b0110:
    speed = B300;
    break;
  case 0b0111:
    speed = B600;
    break;
  case 0b1000:
    speed = B1200;
    break;
  case 0b1010:
    speed = B2400;
    break;
  case 0b1100:
    speed = B4800;
    break;
  case 0b1110:
    speed = B9600;
    break;
  case 0b1111:
    speed = B19200;
    break;
  default:
    /* Unsupported baud rate, just ignore it. */
    return;
  }

  cfsetispeed(&tios, speed);
  cfsetospeed(&tios, speed);

  /* Data Bits */
  switch ((acia->control >> 5) & 0x3) {
  case 0b11:
    tios.c_cflag |= CS5;
    break;
  case 0b10:
    tios.c_cflag |= CS6;
    break;
  case 0b01:
    tios.c_cflag |= CS7;
    break;
  case 0b00:
  default:
    tios.c_cflag |= CS8;
    break;
  }

  if (tcsetattr(acia->tty_fd, TCSANOW, &tios) == -1) {
    panic("tcsetattr() failed with errno: %d\n", errno);
    return;
  }
}



static uint8_t acia_read(void *acia, uint16_t address)
{
  uint8_t byte;

  address -= ((acia_t *)acia)->base_address;

  switch (address) {
  case 0x0:
    if (acia_rx_fifo_read(acia, &byte)) {
      if (((acia_t *)acia)->rx_fifo_tail == ((acia_t *)acia)->rx_fifo_head) {
        ((acia_t *)acia)->status &= ~0x08; /* Receive Data Register Not Full */
      }
      acia_trace("[R] <<< 0x%02x\n", byte);
      return byte;
    } else {
      acia_trace("[R] RX Empty\n");
      return 0;
    }

  case 0x1:
    /* acia_trace("[R] Status=0x%02x\n", ((acia_t *)acia)->status); */
    return ((acia_t *)acia)->status;

  case 0x2:
    acia_trace("[R] Command=0x%02x\n", ((acia_t *)acia)->command);
    return ((acia_t *)acia)->command;

  case 0x3:
    acia_trace("[R] Control=0x%02x\n", ((acia_t *)acia)->control);
    return ((acia_t *)acia)->control;

  default:
    return 0x00;
  }
}



static void acia_write(void *acia, uint16_t address, uint8_t value)
{
  address -= ((acia_t *)acia)->base_address;

  switch (address) {
  case 0x0:
    acia_trace("[W] >>> 0x%02x\n", value);
    acia_tx_fifo_write(acia, value);
    break;

  case 0x1:
    acia_trace("[W] Reset\n");
    ((acia_t *)acia)->command &= ~0x1F; /* Clear lower 5 bits. */
    break;

  case 0x2:
    acia_trace("[W] Command=0x%02x\n", value);
    ((acia_t *)acia)->command = value;
    acia_update_tty_settings(acia);
    break;

  case 0x3:
    acia_trace("[W] Control=0x%02x\n", value);
    ((acia_t *)acia)->control = value;
    acia_update_tty_settings(acia);
    break;

  default:
    break;
  }
}



int acia_init(acia_t *acia, mem_t *mem, uint16_t base_address,
  const char *tty_device)
{
  int i;

  memset(acia, 0, sizeof(acia_t));
  acia->base_address = base_address;
  acia->status |= 0x10; /* Transmit Data Register Empty */

  for (i = (base_address % 0xC000); i < (base_address % 0xC000) + 0x4; i++) {
    mem->io_read[i].func = acia_read;
    mem->io_read[i].cookie = acia;
    mem->io_write[i].func = acia_write;
    mem->io_write[i].cookie = acia;
  }

  if (tty_device != NULL) {
    acia->tty_fd = open(tty_device, O_RDWR | O_NOCTTY);
    if (acia->tty_fd == -1) {
      fprintf(stderr, "open() for '%s' failed with errno: %d\n",
        tty_device, errno);
      return -1;
    }
    /* NOTE: acia->tty_fd is never closed! */
  } else {
    acia->tty_fd = -1;
  }

  for (i = 0; i < ACIA_TRACE_BUFFER_SIZE; i++) {
    acia_trace_buffer[i][0] = '\0';
  }
  acia_trace_buffer_n = 0;

  return 0;
}



void acia_execute(acia_t *acia)
{
  struct pollfd fds[1];
  uint8_t byte;

  if (acia->tty_fd == -1) {
    return;
  }

  /* Only run every X cycle. */
  acia->cycle++;
  if (acia->cycle < 1000) {
    return;
  }
  acia->cycle = 0;

  fds[0].fd = acia->tty_fd;
  fds[0].events = POLLIN;

  /* Non-blocking read. */
  if (poll(fds, 1, 0) > 0) {
    if (read(acia->tty_fd, &byte, 1) == 1) {
      acia_rx_fifo_write(acia, byte);
      acia->status |= 0x08; /* Receive Data Register Full */
    }
  }

  /* Blocking write. */
  if (acia_tx_fifo_read(acia, &byte)) {
    write(acia->tty_fd, &byte, 1);
  }
}



void acia_trace_dump(FILE *fh)
{
  int i;

  for (i = acia_trace_buffer_n; i < ACIA_TRACE_BUFFER_SIZE; i++) {
    if (acia_trace_buffer[i][0] != '\0') {
      fprintf(fh, acia_trace_buffer[i]);
    }
  }
  for (i = 0; i < acia_trace_buffer_n; i++) {
    if (acia_trace_buffer[i][0] != '\0') {
      fprintf(fh, acia_trace_buffer[i]);
    }
  }
}



