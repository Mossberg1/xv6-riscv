#include "doomgeneric.h"
#include <framebuffer.h>
#include <user.h>


static framebuffer_t fb;

void
DG_Init() 
{
  if (fb_init(&fb) < 0) 
  {
    printf("Failed to initialize framebuffer.\n");
    exit(1);
  }

  DG_ScreenBuffer = (pixel_t*)fb.pixels;
}


void
DG_DrawFrame() 
{
  fb_flush();
}

void
DG_SleepMs(uint32 ms) 
{
  sleep(ms/10);
}


uint32 
DG_GetTicksMs() 
{
  return uptime() * 10;
}


int
DG_GetKey(int* pressed, unsigned char* key) 
{
  return 0;
}


void
DG_SetWindowTitle(const char* title) { (void)title; }
