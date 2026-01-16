#pragma once

#include <user.h>

typedef struct {
  uint32 *pixels;
  int     width;
  int     height;
} framebuffer_t;

int fb_init(framebuffer_t* fb);
void fb_flush(framebuffer_t* fb);


