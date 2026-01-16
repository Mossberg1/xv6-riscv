#pragma once

#include <user.h>

typedef struct {
  uint32 *pixels;
  int     width;
  int     height;
} framebuffer_t;

int fb_init(framebuffer_t* fb);
void fb_flush(framebuffer_t* fb);
void fb_draw_char(framebuffer_t *fb, char c, int x, int y, uint32 color);
void fb_draw_string(framebuffer_t *fb, const char *str, int x, int y, uint32 color);

