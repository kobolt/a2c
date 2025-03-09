#ifndef _GUI_H
#define _GUI_H

#include <stdbool.h>
#include <stdint.h>

void gui_draw_pixel(int y, int x, bool on);
int gui_init(void);
void gui_execute(void);

#endif /* _GUI_H */
