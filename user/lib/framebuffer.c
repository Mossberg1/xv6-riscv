#include <framebuffer.h>
#include <stdint.h>
#include <user.h>

int
fb_init(framebuffer_t* fb) 
{
  fb->width = 640;
  fb->height = 400;

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
