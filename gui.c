#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define GUI_WIDTH 560
#define GUI_HEIGHT 192

#define GUI_W_SCALE 2
#define GUI_H_SCALE 4

static SDL_Window *gui_window = NULL;
static SDL_Renderer *gui_renderer = NULL;
static SDL_Texture *gui_texture = NULL;
static SDL_PixelFormat *gui_pixel_format = NULL;
static Uint32 *gui_pixels = NULL;
static int gui_pixel_pitch = 0;



void gui_draw_pixel(int y, int x, bool on)
{
  int scale_x;
  int scale_y;
  int out_x;
  int out_y;

  if (gui_renderer == NULL) {
    return;
  }

  if (y < 0 || y >= GUI_HEIGHT) {
    return;
  }

  if (x < 0 || x >= GUI_WIDTH) {
    return;
  }

  for (scale_y = 0; scale_y < GUI_H_SCALE; scale_y++) {
    for (scale_x = 0; scale_x < GUI_W_SCALE; scale_x++) {
      out_y = (y * GUI_H_SCALE) + scale_y;
      out_x = (x * GUI_W_SCALE) + scale_x;
      if (on) {
        gui_pixels[(out_y * GUI_WIDTH * GUI_W_SCALE) + out_x] =
          SDL_MapRGB(gui_pixel_format, 0xFF, 0xFF, 0xFF);
      } else {
        gui_pixels[(out_y * GUI_WIDTH * GUI_W_SCALE) + out_x] =
          SDL_MapRGB(gui_pixel_format, 0x00, 0x00, 0x00);
      }
    }
  }
}



static void gui_exit_handler(void)
{
  if (gui_pixel_format != NULL) {
    SDL_FreeFormat(gui_pixel_format);
  }
  if (gui_texture != NULL) {
    SDL_UnlockTexture(gui_texture);
    SDL_DestroyTexture(gui_texture);
  }
  if (gui_renderer != NULL) {
    SDL_DestroyRenderer(gui_renderer);
  }
  if (gui_window != NULL) {
    SDL_DestroyWindow(gui_window);
  }
  SDL_Quit();
}



int gui_init(void)
{
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "Unable to initalize SDL: %s\n", SDL_GetError());
    return -1;
  }
  atexit(gui_exit_handler);

  if ((gui_window = SDL_CreateWindow("a2c",
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    GUI_WIDTH * GUI_W_SCALE, GUI_HEIGHT * GUI_H_SCALE, 0)) == NULL) {
    fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
    return -1;
  }

  if ((gui_renderer = SDL_CreateRenderer(gui_window, -1, 0)) == NULL) {
    fprintf(stderr, "Unable to create renderer: %s\n", SDL_GetError());
    return -1;
  }

  if ((gui_texture = SDL_CreateTexture(gui_renderer, 
    SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
    GUI_WIDTH * GUI_W_SCALE, GUI_HEIGHT * GUI_H_SCALE)) == NULL) {
    fprintf(stderr, "Unable to create texture: %s\n", SDL_GetError());
    return -1;
  }

  if (SDL_LockTexture(gui_texture, NULL,
    (void **)&gui_pixels, &gui_pixel_pitch) != 0) {
    fprintf(stderr, "Unable to lock texture: %s\n", SDL_GetError());
    return -1;
  }

  if ((gui_pixel_format = SDL_AllocFormat(
    SDL_PIXELFORMAT_ARGB8888)) == NULL) {
    fprintf(stderr, "Unable to create pixel format: %s\n", SDL_GetError());
    return -1;
  }

  return 0;
}



void gui_execute(void)
{
  SDL_Event event;

  if (gui_renderer != NULL) {
    while (SDL_PollEvent(&event) == 1) {
      switch (event.type) {
      case SDL_QUIT:
        exit(EXIT_SUCCESS);
        break;
      }
    }

    SDL_UnlockTexture(gui_texture);
    SDL_RenderCopy(gui_renderer, gui_texture, NULL, NULL);

    if (SDL_LockTexture(gui_texture, NULL,
      (void **)&gui_pixels, &gui_pixel_pitch) != 0) {
      fprintf(stderr, "Unable to lock texture: %s\n", SDL_GetError());
      exit(EXIT_FAILURE);
    }

    SDL_RenderPresent(gui_renderer);
  }
}



