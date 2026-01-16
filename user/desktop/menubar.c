#include "components.h"
#include <user.h>


#define SEPARATOR_COLOR 0x00555555


void
menubar_init(struct menubar* mb, int width, enum menubar_pos pos, uint32 color) 
{
  mb->color   = color;
  mb->pos     = pos;
  mb->updated = 1;
  mb->framebuffer.width = width;
  mb->framebuffer.height = 24;
  mb->framebuffer.pixels = malloc(width * 24 * sizeof(uint32));
}


void
menubar_render(struct menubar* mb) 
{
  int menubar_size = mb->framebuffer.width * mb->framebuffer.height;
  for (int i = 0; i < menubar_size; i++) 
  {
    mb->framebuffer.pixels[i] = mb->color;
  }

  int y = (mb->pos == MENUBAR_TOP) ? (mb->framebuffer.height - 1) : 0; // Check if menubar is positioned at the top or bottom.
  for (int x = 0; x < mb->framebuffer.width; x++) {
    mb->framebuffer.pixels[y * mb->framebuffer.width + x] = SEPARATOR_COLOR;
  }

  fb_draw_string(&mb->framebuffer, "xv6 riscv", 10, 4, 0x00000000);

  mb->updated = 0;
}
