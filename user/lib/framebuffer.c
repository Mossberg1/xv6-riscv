#include <framebuffer.h>
#include <stdint.h>
#include <user.h>
#include <terminus_font.h>


int
fb_init(framebuffer_t* fb) 
{
  fb->pixels = (uint32*)malloc(fb->width * fb->height * 4);

  if (fb->pixels == 0) 
  {
    return -1;
  }

  memset(fb->pixels, 0, fb->width * fb->height * 4);

  return 0;
}


void 
fb_flush(framebuffer_t* fb)
{
  fbcopy(fb->pixels);
}


void 
fb_draw_char(framebuffer_t *fb, char c, int x, int y, uint32 color) {
    if (c > 127) return;
    for (int row = 0; row < 16; row++) {
        unsigned char data = terminus_16n[(int)c][row];
        for (int col = 0; col < 8; col++) {
            if (data & (0x80 >> col)) {
                fb->pixels[(y + row) * fb->width + (x + col)] = color;
            }
        }
    }
}


void 
fb_draw_string(framebuffer_t *fb, const char *str, int x, int y, uint32 color) {
    while (*str) {
        fb_draw_char(fb, *str, x, y, color);
        x += 8; // Move 8 pixels right for the next character
        str++;
    }
}
