#include <framebuffer.h>
#include <types.h>
#include <user.h>
#include "components.h"

#define BACKGROUND_COLOR 0x00008080
#define SCREEN_WIDTH     1024
#define SCREEN_HEIGHT    768

#define MENUBAR_COLOR 0x00CCCCCC

#define MAIN_LOOP_PAUSE_DURATION 3

void draw_pixel(int x, int y, uint32 color);
void fill_background();
int load_background_image(const char* filename);

static framebuffer_t global_fb;
struct menubar menubar;

int
main(void) 
{
  global_fb.width = SCREEN_WIDTH;
  global_fb.height = SCREEN_HEIGHT;

  if (fb_init(&global_fb) < 0) 
  { 
    printf("malloc failed\n");
    return -1;
  }

  fill_background();

  menubar_init(&menubar, global_fb.width, MENUBAR_TOP, MENUBAR_COLOR);

  while (1) 
  {
    if (menubar.updated) 
    {
      menubar_render(&menubar);
    }

    // Put menubar into the global framebuffer.
    int y_offset = (menubar.pos == MENUBAR_TOP) ? 0 : (global_fb.height - menubar.framebuffer.height);
    for (int y = 0; y < menubar.framebuffer.height; y++) 
    {
      for (int x = 0; x < menubar.framebuffer.width; x++) 
      {
        int global_fb_index = (y + y_offset) * global_fb.width + x;
        int menubar_fb_index = y * menubar.framebuffer.width + x;
        global_fb.pixels[global_fb_index] = menubar.framebuffer.pixels[menubar_fb_index];
      }
    }

    fb_flush(&global_fb);

    pause(MAIN_LOOP_PAUSE_DURATION); // TODO: Maybe implement some type of block until notified to rerender?
  }

  return 0;
}


void draw_pixel(int x, int y, uint32 color) 
{
  // Check if inside the screen.
  if (x < 0 || x >= global_fb.width || y < 0 || y >= global_fb.height)
    return;

  int pixel_index = y * global_fb.width + x;

  global_fb.pixels[pixel_index] = color;
}


void fill_background() 
{
  int screen_size = global_fb.width * global_fb.height;
  for (int i = 0; i < screen_size; i++) 
  {
    global_fb.pixels[i] = BACKGROUND_COLOR;
  }

  fb_flush(&global_fb);
}
