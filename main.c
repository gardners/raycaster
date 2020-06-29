#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <hal.h>
#include <memory.h>
#include <dirent.h>
#include <fileio.h>

void TraceFrame(unsigned char playerX, unsigned char playerY, uint16_t playerDirection);
void TraceFrameFast(unsigned char playerX, unsigned char playerY, uint16_t playerDirection);
void setup_sky(void);
void setup_multiplier(void);


unsigned short i,j;
unsigned char a,b,c,d;

void graphics_mode(void)
{

  // Clear screen RAM first, so that there is no visible glitching
  lfill(0x40000,0x00,0x8000);
  lfill(0x48000,0x00,0x8000);
  lfill(0x50000,0x00,0x8000);
  lfill(0x58000,0x00,0x8000);
  
  // 16-bit text mode, full-colour text for high chars
  POKE(0xD054,0x05);
  // H640, fast CPU
  POKE(0xD031,0xC0);
  // Adjust D016 smooth scrolling for VIC-III H640 offset
  POKE(0xD016,0xC9);

  // H320, fast CPU
  POKE(0xD031,0x40);
  // Adjust D016 smooth scrolling for VIC-III H320 offset
  POKE(0xD016,0xC8);

  // 640x200 16bits per char, 8 pixels wide per char
  // = 640/8 x 16 bits = 160 bytes per row
  POKE(0xD058,160);
  POKE(0xD059,160>>8);
  // Draw 80 chars per row
  POKE(0xD05E,80);
  // Put 4000 byte screen at $C000
  POKE(0xD060,0x00);
  POKE(0xD061,0xc0);
  POKE(0xD062,0x00);

  // Layout screen so that graphics data comes from $40000 -- $5FFFF

  i=0x40000/0x40;
  for(a=0;a<80;a++)
    for(b=0;b<25;b++) {
      POKE(0xC000+b*160+a*2+0,i&0xff);
      POKE(0xC000+b*160+a*2+1,i>>8);

      i++;
    }
   
  // Clear colour RAM, 8-bits per pixel
  lfill(0xff80000L,0x00,80*25*2);

  POKE(0xD020,0);
  POKE(0xD021,0);
}

unsigned long pixel_addr;
unsigned char pixel_temp;
void plot_pixel(unsigned long x,unsigned char y,unsigned char colour)
{
  pixel_addr=(x&7)+64L*25L*(x>>3);
  pixel_addr+=y<<3;

  lpoke(0x40000L+pixel_addr,colour);

}

void main(void)
{
  char dma_draw=1;
    int i;
    uint8_t px=5;
    uint8_t py=5;

    asm ( "sei" );

    
  // Fast CPU, M65 IO
  POKE(0,65);
  POKE(0xD02F,0x47);
  POKE(0xD02F,0x53);

  while(PEEK(0xD610)) POKE(0xD610,0);
  
  POKE(0xD020,0);
  POKE(0xD021,0);

  graphics_mode();

  // Set up grey-scale palette
  for(i=0;i<256;i++) {
    POKE(0xD100+i,(i>>4)+((i&7)<<4));
    POKE(0xD200+i,(i>>4)+((i&7)<<4));
    POKE(0xD300+i,(i>>4)+((i&7)<<4));
  }

  setup_sky();
  setup_multiplier();
  
  i=0;
  while(1)
    {
      if (px<1) px=1;
      if (py<1) py=1;
      if (px>30) px=30;
      if (py>30) py=30;

      if (dma_draw)
	TraceFrameFast(px,py,i);
      else
	TraceFrame(px,py,i);

      i&=0x3FF;
      
      if (PEEK(0xD610)) {
	switch(PEEK(0xD610)) {
	case 0x31: i-=10; break;
	case 0x32: i+=10; break;
	case 0xF1: dma_draw^=1; break;
	  // Rotate left/right
	case 0x1d: case 0x44: case 0x64: i+=0x100; i&=0x3ff; break;
	case 0x9d: case 0x41: case 0x61: i-=0x100; i&=0x3ff; break;

	  // Move forewards/backwards
	case 0x11: case 0x53: case 0x73:
	  switch(i) {
	  case 0: py--; break;
	  case 0x100: px--; break;
	  case 0x200: py++; break;
	  case 0x300: px++; break;
	  }
	  break;
	case 0x91: case 0x57: case 0x77:
	  POKE(0xD020,PEEK(0xD012));
	  switch(i) {
	  case 0: py++; break;
	  case 0x100: px++; break;
	  case 0x200: py--; break;
	  case 0x300: px--; break;
	  }
	  break;
	}
	POKE(0xD610,0);
      }
      
      //      i++; i&=0x3ff;
      POKE(0xD020,PEEK(0xD012)&0x0f);
    }
}
