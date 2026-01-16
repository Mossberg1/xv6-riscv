#include <framebuffer.h>
#include <types.h>


#define COLOR 0xFFFFFFFF

int 
main(void) 
{
  framebuffer_t fb;

  if (fb_init(&fb) < 0) 
  {
    printf("malloc failed\n");
    return -1;
  }

  for(int i=0; i<fb.width*fb.height; i++) {
    fb.pixels[i] = COLOR;
  }

  fb_flush(&fb);

  return 0;
}
