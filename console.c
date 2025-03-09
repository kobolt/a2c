#include "console.h"
#include <ctype.h>
#include <curses.h>
#include <stdint.h>
#include <stdlib.h>

#include "mem.h"
#include "panic.h"
#include "w65c02.h"
#ifdef HIRES_GUI_WINDOW
#include "gui.h"
#endif /* HIRES_GUI_WINDOW */

typedef enum {
  CONSOLE_DRAW_UNKNOWN,
  CONSOLE_DRAW_TEXT_80_COLUMN,
  CONSOLE_DRAW_TEXT_40_COLUMN,
  CONSOLE_DRAW_HIRES_DOUBLE,
  CONSOLE_DRAW_HIRES_80_COLUMN,
  CONSOLE_DRAW_HIRES_40_COLUMN,
  CONSOLE_DRAW_LORES_DOUBLE,
  CONSOLE_DRAW_LORES_80_COLUMN,
  CONSOLE_DRAW_LORES_40_COLUMN,
} console_draw_t;

/* Using colors from the default xterm/rxvt 256 color palette. */
static const int console_color_map[16][2] = {
  {232, 255}, /*  0 = Black        */
  {52 , 255}, /*  1 = Magenta      */
  {62 , 255}, /*  2 = Dark Blue    */
  {164, 255}, /*  3 = Purple       */
  {22 , 255}, /*  4 = Dark Green   */
  {242, 255}, /*  5 = Gray 1       */
  {44 , 255}, /*  6 = Medium Blue  */
  {104, 232}, /*  7 = Light Blue   */
  {58 , 255}, /*  8 = Brown        */
  {208, 255}, /*  9 = Orange       */
  {244, 255}, /* 10 = Gray 2       */
  {218, 232}, /* 11 = Pink         */
  {40 , 255}, /* 12 = Bright Green */
  {142, 232}, /* 13 = Yellow       */
  {116, 232}, /* 14 = Aquamarine   */
  {255, 232}, /* 15 = White        */
};

static const uint16_t console_row_address_map[24] = {
  0x400, 0x480, 0x500, 0x580, 0x600, 0x680, 0x700, 0x780,
  0x428, 0x4A8, 0x528, 0x5A8, 0x628, 0x6A8, 0x728, 0x7A8,
  0x450, 0x4D0, 0x550, 0x5D0, 0x650, 0x6D0, 0x750, 0x7D0
};

static const uint16_t console_hires_row_address_map[24] = {
  0x2000, 0x2080, 0x2100, 0x2180, 0x2200, 0x2280, 0x2300, 0x2380,
  0x2028, 0x20A8, 0x2128, 0x21A8, 0x2228, 0x22A8, 0x2328, 0x23A8,
  0x2050, 0x20D0, 0x2150, 0x21D0, 0x2250, 0x22D0, 0x2350, 0x23D0,
};

static const uint8_t console_primary_char_set[256] = {
  '@','A','B','C','D','E','F','G' ,'H','I','J','K','L' ,'M','N','O',
  'P','Q','R','S','T','U','V','W' ,'X','Y','Z','[','\\',']','^','_',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',' ,'-','.','/',
  '0','1','2','3','4','5','6','7' ,'8','9',':',';','<' ,'=','>','?',
  '@','A','B','C','D','E','F','G' ,'H','I','J','K','L' ,'M','N','O',
  'P','Q','R','S','T','U','V','W' ,'X','Y','Z','[','\\',']','^','_',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',' ,'-','.','/',
  '0','1','2','3','4','5','6','7' ,'8','9',':',';','<' ,'=','>','?',
  '@','A','B','C','D','E','F','G' ,'H','I','J','K','L' ,'M','N','O',
  'P','Q','R','S','T','U','V','W' ,'X','Y','Z','[','\\',']','^','_',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',' ,'-','.','/',
  '0','1','2','3','4','5','6','7' ,'8','9',':',';','<' ,'=','>','?',
  '@','A','B','C','D','E','F','G' ,'H','I','J','K','L' ,'M','N','O',
  'P','Q','R','S','T','U','V','W' ,'X','Y','Z','[','\\',']','^','_',
  '`','a','b','c','d','e','f','g' ,'h','i','j','k','l' ,'m','n','o',
  'p','q','r','s','t','u','v','w' ,'x','y','z','{','|' ,'}','~','#',
};

static const uint8_t console_alternate_char_set[256] = {
  '@','A','B','C','D','E','F','G' ,'H','I','J','K','L' ,'M','N','O',
  'P','Q','R','S','T','U','V','W' ,'X','Y','Z','[','\\',']','^','_',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',' ,'-','.','/',
  '0','1','2','3','4','5','6','7' ,'8','9',':',';','<' ,'=','>','?',
  'a','a','^','h','c','c','m','m' ,'<','.','v','^','-' ,'r','#','<', /* Mouse */
  '>','v','^','-','L','>','#','#' ,'[',']','|','*','=' ,'+','#','|', /* Text  */
  '`','a','b','c','d','e','f','g' ,'h','i','j','k','l' ,'m','n','o',
  'p','q','r','s','t','u','v','w' ,'x','y','z','{','|' ,'}','~','#',
  '@','A','B','C','D','E','F','G' ,'H','I','J','K','L' ,'M','N','O',
  'P','Q','R','S','T','U','V','W' ,'X','Y','Z','[','\\',']','^','_',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',' ,'-','.','/',
  '0','1','2','3','4','5','6','7' ,'8','9',':',';','<' ,'=','>','?',
  '@','A','B','C','D','E','F','G' ,'H','I','J','K','L' ,'M','N','O',
  'P','Q','R','S','T','U','V','W' ,'X','Y','Z','[','\\',']','^','_',
  '`','a','b','c','d','e','f','g' ,'h','i','j','k','l' ,'m','n','o',
  'p','q','r','s','t','u','v','w' ,'x','y','z','{','|' ,'}','~','#',
};



static uint8_t console_key = 0;
static bool console_open_apple   = false;
static bool console_solid_apple  = false;
static bool console_80_40_switch = false;
static bool console_mouse_button = false;



static void console_io_write(void* dummy, uint16_t address, uint8_t value)
{
  (void)dummy;
  (void)value;

  switch (address) {
  case 0xC010:
    console_key &= ~0x80; /* Clear keyboard strobe. */
    break;
  }
}



static uint8_t console_io_read(void *dummy, uint16_t address)
{
  (void)dummy;

  switch (address) {
  case 0xC000:
    return console_key;

  case 0xC010:
    if (console_key & 0x80) {
      console_key &= ~0x80; /* Clear keyboard strobe. */
      return 0x80;
    } else {
      return 0x00;
    }

  case 0xC019:
    /* NOTE: Reset of Vertical Blanking Interrupt not implemented. */
    return 0;

  case 0xC060:
    return console_80_40_switch << 7;

  case 0xC061:
    return console_open_apple << 7;

  case 0xC062:
    return console_solid_apple << 7;

  case 0xC063:
    return !console_mouse_button << 7; /* Button NOT pressed. */

  default:
    return 0;
  }
}



void console_pause(void)
{
  endwin();
  timeout(-1);
}



void console_resume(void)
{
  timeout(0);
  refresh();
}



void console_exit(void)
{
  curs_set(1);
  endwin();
}



int console_init(mem_t *mem, bool gui_enable)
{
  (void)gui_enable;
  int color_no;

#ifdef HIRES_GUI_WINDOW
  if (gui_enable) {
    if (gui_init() != 0) {
      return -1;
    }
  }
#endif /* HIRES_GUI_WINDOW */

  initscr();
  atexit(console_exit);
  noecho();
  keypad(stdscr, TRUE);
  timeout(0);
  curs_set(0);

  if (has_colors() && can_change_color()) {
    start_color();
    use_default_colors();
    for (color_no = 0; color_no < 16; color_no++) {
      init_pair(color_no + 1,
        console_color_map[color_no][1],
        console_color_map[color_no][0]);
    }
  }

  mem->io_read[0x00].func = console_io_read;
  mem->io_read[0x10].func = console_io_read;
  mem->io_read[0x19].func = console_io_read;
  mem->io_read[0x60].func = console_io_read;
  mem->io_read[0x61].func = console_io_read;
  mem->io_read[0x62].func = console_io_read;
  mem->io_read[0x63].func = console_io_read;
  mem->io_write[0x10].func = console_io_write;

  return 0;
}



static void console_draw_text(mem_t *mem, int row, int col, uint8_t c)
{
  bool reverse = false;

  reverse = (c >> 7) == 0;

  if (mem->video_alt_char_set) {
    if (c >= 0x40 && c <= 0x5F) {
      reverse = false; /* Disable for MouseText characters. */
    }
    if (reverse) {
      attron(A_REVERSE);
    }
    mvaddch(row, col, console_alternate_char_set[c]);
    if (reverse) {
      attroff(A_REVERSE);
    }

  } else {
    if (reverse) {
      attron(A_REVERSE);
    }
    mvaddch(row, col, console_primary_char_set[c]);
    if (reverse) {
      attroff(A_REVERSE);
    }
  }
}



static void console_draw_lores_pixel(int row, int col, int color)
{
  if (has_colors() && can_change_color()) {
    attron(COLOR_PAIR(color + 1));
    mvaddch(row, col, ' ');
    attroff(COLOR_PAIR(color + 1));

  } else {
    if (color > 0) {
      mvaddch(row, col, '#'); /* Any color. */
    } else {
      mvaddch(row, col, ' '); /* Just black. */
    }
  }
}



static void console_draw_hires_pixels(int row, int col, uint8_t byte)
{
  /* Truncate the output to 192/4=48 rows and 280/4=70 columns. */
  if ((byte & 0x0F) > 0) {
    mvaddch(row / 4, (col / 4), '#');
  } else {
    mvaddch(row / 4, (col / 4), ' ');
  }
  if ((byte & 0x70) > 0) {
    mvaddch(row / 4, (col / 4) + 1, '#');
  } else {
    mvaddch(row / 4, (col / 4) + 1, ' ');
  }

#ifdef HIRES_GUI_WINDOW
  int i;
  for (i = 0; i < 7; i++) {
    gui_draw_pixel(row, ((col + i) * 2), (byte >> i) & 1);
    gui_draw_pixel(row, ((col + i) * 2) + 1, (byte >> i) & 1);
  }
#endif /* HIRES_GUI_WINDOW */
}



static void console_draw_double_hires_pixels(int row, int col, uint8_t byte)
{
  /* Truncate the output to 192/4=48 rows and 560/8=70 columns. */
  if ((byte & 0x7F) > 0) {
    mvaddch(row / 4, (col / 8), '#');
  } else {
    mvaddch(row / 4, (col / 8), ' ');
  }

#ifdef HIRES_GUI_WINDOW
  int i;
  for (i = 0; i < 7; i++) {
    gui_draw_pixel(row, col + i, (byte >> i) & 1);
  }
#endif /* HIRES_GUI_WINDOW */
}



static void console_draw_text_40_column(mem_t *mem)
{
  uint16_t address;
  int row;
  int col;

  for (row = 0; row < 24; row++) {
    for (col = 0; col < 40; col++) {
      address = console_row_address_map[row] + col;
      if (mem->page2 == true) {
        address += 0x400;
      }
      console_draw_text(mem, row, col, mem->main[address]);
    }
  }
}



static void console_draw_text_80_column(mem_t *mem)
{
  uint16_t address;
  int row;
  int col;

  for (row = 0; row < 24; row++) {
    for (col = 0; col < 80; col++) {
      address = console_row_address_map[row] + (col / 2);
      if (col % 2 == 0) {
        console_draw_text(mem, row, col, mem->aux[address]);
      } else {
        console_draw_text(mem, row, col, mem->main[address]);
      }
    }
  }
}



static void console_draw_lores_40_column(mem_t *mem)
{
  uint16_t address;
  int row;
  int col;

  for (row = 0; row < 24; row++) {
    for (col = 0; col < 40; col++) {
      address = console_row_address_map[row] + col;
      if (mem->page2 == true) {
        address += 0x400;
      }
      if (row >= 20 && mem->video_mixed_mode) {
        console_draw_text(mem, row + 20, col, mem->main[address]);
      } else {
        console_draw_lores_pixel((row * 2), col, mem->main[address] % 0x10);
        console_draw_lores_pixel((row * 2) + 1, col, mem->main[address] / 0x10);
      }
    }
  }
}



static void console_draw_lores_80_column(mem_t *mem)
{
  uint16_t address;
  int row;
  int col;

  for (row = 0; row < 24; row++) {
    for (col = 0; col < 80; col++) {
      address = console_row_address_map[row] + (col / 2);
      if (row >= 20 && mem->video_mixed_mode) {
        if (col % 2 == 0) {
          console_draw_text(mem, row + 20, col, mem->aux[address]);
        } else {
          console_draw_text(mem, row + 20, col, mem->main[address]);
        }
      } else {
        /* The pixels are repeated/doubled in width in this mode. */
        console_draw_lores_pixel((row * 2), col, mem->main[address] % 0x10);
        console_draw_lores_pixel((row * 2) + 1, col, mem->main[address] / 0x10);
      }
    }
  }
}



static void console_draw_hires_40_column(mem_t *mem)
{
  uint16_t address;
  int row;
  int col;

  for (row = 0; row < 24; row++) {
    for (col = 0; col < 40; col++) {
      if (row >= 20 && mem->video_mixed_mode) {
        address = console_row_address_map[row] + col;
        if (mem->page2 == true) {
          address += 0x400;
        }
        console_draw_text(mem, row + 20, col, mem->main[address]);
      } else {
        address = console_hires_row_address_map[row] + col;
        if (mem->page2 == true) {
          address += 0x2000;
        }
        console_draw_hires_pixels((row * 8),     (col * 7),
          mem->main[address]);
        console_draw_hires_pixels((row * 8) + 1, (col * 7),
          mem->main[address + 0x0400]);
        console_draw_hires_pixels((row * 8) + 2, (col * 7),
          mem->main[address + 0x0800]);
        console_draw_hires_pixels((row * 8) + 3, (col * 7),
          mem->main[address + 0x0C00]);
        console_draw_hires_pixels((row * 8) + 4, (col * 7),
          mem->main[address + 0x1000]);
        console_draw_hires_pixels((row * 8) + 5, (col * 7),
          mem->main[address + 0x1400]);
        console_draw_hires_pixels((row * 8) + 6, (col * 7),
          mem->main[address + 0x1800]);
        console_draw_hires_pixels((row * 8) + 7, (col * 7),
          mem->main[address + 0x1C00]);
      }
    }
  }
}



static void console_draw_hires_80_column(mem_t *mem)
{
  uint16_t address;
  int row;
  int col;

  for (row = 0; row < 24; row++) {
    for (col = 0; col < 80; col++) {
      if (row >= 20 && mem->video_mixed_mode) {
        address = console_row_address_map[row] + (col / 2);
        if (col % 2 == 0) {
          console_draw_text(mem, row + 20, col, mem->aux[address]);
        } else {
          console_draw_text(mem, row + 20, col, mem->main[address]);
        }
      } else {
        if (col % 2 == 0) {
          address = console_hires_row_address_map[row] + (col / 2);
          console_draw_hires_pixels((row * 8),     ((col / 2) * 7),
            mem->main[address]);
          console_draw_hires_pixels((row * 8) + 1, ((col / 2) * 7),
            mem->main[address + 0x0400]);
          console_draw_hires_pixels((row * 8) + 2, ((col / 2) * 7),
            mem->main[address + 0x0800]);
          console_draw_hires_pixels((row * 8) + 3, ((col / 2) * 7),
            mem->main[address + 0x0C00]);
          console_draw_hires_pixels((row * 8) + 4, ((col / 2) * 7),
            mem->main[address + 0x1000]);
          console_draw_hires_pixels((row * 8) + 5, ((col / 2) * 7),
            mem->main[address + 0x1400]);
          console_draw_hires_pixels((row * 8) + 6, ((col / 2) * 7),
            mem->main[address + 0x1800]);
          console_draw_hires_pixels((row * 8) + 7, ((col / 2) * 7),
            mem->main[address + 0x1C00]);
        }
      }
    }
  }
}



static void console_draw_lores_double(mem_t *mem)
{
  uint16_t address;
  int row;
  int col;

  for (row = 0; row < 24; row++) {
    for (col = 0; col < 80; col++) {
      address = console_row_address_map[row] + (col / 2);
      if (row >= 20 && mem->video_mixed_mode) {
        if (col % 2 == 0) {
          console_draw_text(mem, row + 20, col, mem->aux[address]);
        } else {
          console_draw_text(mem, row + 20, col, mem->main[address]);
        }
      } else {
        if (col % 2 == 0) {
          console_draw_lores_pixel((row * 2), col,
            mem->aux[address] % 0x10);
          console_draw_lores_pixel((row * 2) + 1, col,
            mem->aux[address] / 0x10);
        } else {
          console_draw_lores_pixel((row * 2), col,
            mem->main[address] % 0x10);
          console_draw_lores_pixel((row * 2) + 1, col,
            mem->main[address] / 0x10);
        }
      }
    }
  }
}



static void console_draw_hires_double(mem_t *mem)
{
  uint16_t address;
  int row;
  int col;

  for (row = 0; row < 24; row++) {
    for (col = 0; col < 80; col++) {
      address = console_row_address_map[row] + (col / 2);
      if (row >= 20 && mem->video_mixed_mode) {
        if (col % 2 == 0) {
          console_draw_text(mem, row + 20, col, mem->aux[address]);
        } else {
          console_draw_text(mem, row + 20, col, mem->main[address]);
        }
      } else {
        address = console_hires_row_address_map[row] + (col / 2);
        if (col % 2 == 0) {
          console_draw_double_hires_pixels((row * 8),     (col * 7),
            mem->aux[address]);
          console_draw_double_hires_pixels((row * 8) + 1, (col * 7),
            mem->aux[address + 0x0400]);
          console_draw_double_hires_pixels((row * 8) + 2, (col * 7),
            mem->aux[address + 0x0800]);
          console_draw_double_hires_pixels((row * 8) + 3, (col * 7),
            mem->aux[address + 0x0C00]);
          console_draw_double_hires_pixels((row * 8) + 4, (col * 7),
            mem->aux[address + 0x1000]);
          console_draw_double_hires_pixels((row * 8) + 5, (col * 7),
            mem->aux[address + 0x1400]);
          console_draw_double_hires_pixels((row * 8) + 6, (col * 7),
            mem->aux[address + 0x1800]);
          console_draw_double_hires_pixels((row * 8) + 7, (col * 7),
            mem->aux[address + 0x1C00]);
        } else {
          console_draw_double_hires_pixels((row * 8),     (col * 7),
            mem->main[address]);
          console_draw_double_hires_pixels((row * 8) + 1, (col * 7),
            mem->main[address + 0x0400]);
          console_draw_double_hires_pixels((row * 8) + 2, (col * 7),
            mem->main[address + 0x0800]);
          console_draw_double_hires_pixels((row * 8) + 3, (col * 7),
            mem->main[address + 0x0C00]);
          console_draw_double_hires_pixels((row * 8) + 4, (col * 7),
            mem->main[address + 0x1000]);
          console_draw_double_hires_pixels((row * 8) + 5, (col * 7),
            mem->main[address + 0x1400]);
          console_draw_double_hires_pixels((row * 8) + 6, (col * 7),
            mem->main[address + 0x1800]);
          console_draw_double_hires_pixels((row * 8) + 7, (col * 7),
            mem->main[address + 0x1C00]);
        }
      }
    }
  }
}



static console_draw_t console_draw_mode(mem_t* mem)
{
  if (mem->video_text_mode) {
    if (mem->video_80_column) {
      return CONSOLE_DRAW_TEXT_80_COLUMN;
    } else {
      return CONSOLE_DRAW_TEXT_40_COLUMN;
    }
  } else { /* Graphics */
    if (mem->hires) { /* HiRes */
      if (mem->video_80_column) {
        if (mem->iou_dhires) { /* Double HiRes */
          return CONSOLE_DRAW_HIRES_DOUBLE;
        } else {
          return CONSOLE_DRAW_HIRES_80_COLUMN;
        }
      } else {
        return CONSOLE_DRAW_HIRES_40_COLUMN;
      }
    } else { /* LoRes */
      if (mem->video_80_column) {
        if (mem->iou_dhires) { /* Double LoRes */
          return CONSOLE_DRAW_LORES_DOUBLE;
        } else {
          return CONSOLE_DRAW_LORES_80_COLUMN;
        }
      } else {
        return CONSOLE_DRAW_LORES_40_COLUMN;
      }
    }
  }
}



void console_execute(w65c02_t *cpu, mem_t *mem)
{
  static int cycle = 0;
  static console_draw_t last_draw = CONSOLE_DRAW_UNKNOWN;
  console_draw_t next_draw;
  int c;

  /* Only run every X cycle. */
  cycle++;
  if (cycle % 10000 != 0) {
    return;
  }

  /* Output */
  next_draw = console_draw_mode(mem);
  if (next_draw != last_draw) {
    clear(); /* Clear the screen to avoid garbage if there is a resizing. */
  }
  switch (next_draw) {
  case CONSOLE_DRAW_TEXT_80_COLUMN:
    console_draw_text_80_column(mem);
    break;
  case CONSOLE_DRAW_TEXT_40_COLUMN:
    console_draw_text_40_column(mem);
    break;
  case CONSOLE_DRAW_HIRES_DOUBLE:
    console_draw_hires_double(mem);
    break;
  case CONSOLE_DRAW_HIRES_80_COLUMN:
    console_draw_hires_80_column(mem);
    break;
  case CONSOLE_DRAW_HIRES_40_COLUMN:
    console_draw_hires_40_column(mem);
    break;
  case CONSOLE_DRAW_LORES_DOUBLE:
    console_draw_lores_double(mem);
    break;
  case CONSOLE_DRAW_LORES_80_COLUMN:
    console_draw_lores_80_column(mem);
    break;
  case CONSOLE_DRAW_LORES_40_COLUMN:
    console_draw_lores_40_column(mem);
    break;
  case CONSOLE_DRAW_UNKNOWN:
  default:
    break;
  }
  last_draw = next_draw;
  refresh();

  /* Input */
  c = getch();
  if (c != ERR) {
    switch (c) {
    case '\n':
    case KEY_ENTER:
      console_key = 0x0D; /* Convert to CR. */
      break;

    case KEY_UP:
      console_key = 0x0B;
      break;

    case KEY_DOWN:
      console_key = 0x0A;
      break;

    case KEY_RIGHT:
      console_key = 0x15;
      break;

    case KEY_LEFT:
      console_key = 0x08;
      break;

    case KEY_BACKSPACE:
      console_key = 0x7F;
      break;

    case KEY_F(1):
      w65c02_reset(cpu, mem);
      break;

    case KEY_F(2):
      console_open_apple = !console_open_apple;
      break;

    case KEY_F(3):
      console_solid_apple = !console_solid_apple;
      break;

    case KEY_F(4):
      console_80_40_switch = !console_80_40_switch;
      break;

    default:
      console_key = c;
      break;
    }

    console_key |= 0x80;
  }

#ifdef HIRES_GUI_WINDOW
  gui_execute();
#endif /* HIRES_GUI_WINDOW */
}



