#pragma once

#include <types.h>
#include <framebuffer.h>

enum menubar_pos {
  MENUBAR_TOP,
  MENUBAR_BOTTOM
};

struct menubar {
  uint32 color;
  framebuffer_t framebuffer;
  enum menubar_pos pos;
  int updated;
};


void menubar_init(struct menubar* mb, int width, enum menubar_pos pos, uint32 color);
void menubar_render(struct menubar* mb);
