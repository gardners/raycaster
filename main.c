#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <hal.h>
#include <memory.h>
#include <dirent.h>
#include <fileio.h>
#include <debug.h>
#include <random.h>

// 256 = one whole square
#define STEP 64
// 0x100 = 90 degrees at a time
#define ROTATE_STEP (0x100>>2)

void TraceFrame(uint16_t playerX, uint16_t playerY, uint16_t playerDirection);
void TraceFrameFast(uint16_t playerX, uint16_t playerY, uint16_t playerDirection);
void setup_sky(void);
void setup_multiplier(void);
uint16_t MulCos(uint16_t angle,uint16_t magnitude);
uint16_t MulSin(uint16_t angle,uint16_t magnitude);
char IsWall(uint8_t tileX, uint8_t tileY);
void generate_maze(uint8_t width, uint8_t height,uint32_t seed);
uint16_t maze_get_cell(uint8_t x,uint8_t y);
void maze_set_cell(uint8_t x,uint8_t y,uint16_t v);


unsigned short i,j;
unsigned char a,b,c,d;

unsigned char diag_mode=0;

void text80_mode(void)
{
  // Normal 8-bit text mode 
  POKE(0xD054,0x00);
  // H640, fast CPU
  POKE(0xD031,0xC0);
  // Adjust D016 smooth scrolling for VIC-III H640 offset
  POKE(0xD016,0xC9);

  POKE(0xD058,80);
  POKE(0xD059,80>>8);
  // Draw 80 chars per row
  POKE(0xD05E,80);
  // Put 4000 byte screen at $B000
  POKE(0xD060,0x00);
  POKE(0xD061,0xB0);
  POKE(0xD062,0x00);

  lfill(0xB000,0x20,2000);

  // Yellow colour in palette
  POKE(0xD10F,0x0f);
  POKE(0xD20F,0x0f);
  POKE(0xD30F,0x00);
}


void graphics_mode(void)
{

  // Clear screen RAM first, so that there is no visible glitching
  lfill(0x40000,0x00,0x8000);
  lfill(0x48000,0x00,0x8000);
  lfill(0x50000,0x00,0x8000);
  lfill(0x58000,0x00,0x8000);

  // Set up grey-scale palette
  for(i=0;i<256;i++) {
    POKE(0xD100+i,(i>>4)+((i&7)<<4));
    POKE(0xD200+i,(i>>4)+((i&7)<<4));
    POKE(0xD300+i,(i>>4)+((i&7)<<4));
  }  
  
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

uint8_t char_code;

void print_text80(unsigned char x,unsigned char y,unsigned char colour,char *msg)
{
  pixel_addr=0xB000+x+y*80;
  while(*msg) {
    char_code=*msg;
    if (*msg>=0xc0&&*msg<=0xe0) char_code=*msg-0x80;
    else if (*msg>=0x40&&*msg<=0x60) char_code=*msg-0x40;
    else if (*msg>=0x60&&*msg<=0x7A) char_code=*msg-0x20;
    POKE(pixel_addr+0,char_code);
    lpoke(0xff80000L-0xB000+pixel_addr,colour);
    msg++;
    pixel_addr+=1;
  }
}

char m[80+1];

void main(void)
{
  char dma_draw=1;
  int i;
  uint16_t px=1<<8;
  uint16_t py=1<<8;

    asm ( "sei" );

    
  // Fast CPU, M65 IO
  POKE(0,65);
  POKE(0xD02F,0x47);
  POKE(0xD02F,0x53);

  while(PEEK(0xD610)) POKE(0xD610,0);
  
  POKE(0xD020,0);
  POKE(0xD021,0);

  graphics_mode();

  setup_sky();
  setup_multiplier();

  // Generate a maze
  // Must have odd size, so walls an corridors can co-exist.
  generate_maze(41,41,1);
  for(px=0;px<40;px++)
    for(py=0;py<40;py++) {
      if (maze_get_cell(px,py)) plot_pixel(px,py,0x80);
      else plot_pixel(px,py,0x00);
    }

  // Start in middle of start square, looking down the maze
  px=0x0180; py=0x180;
  if (!IsWall(1,2)) i=0x000;
  else i=0x100;

  
  while(1)
    {
      if (px<(0<<8)) px=0<<8;
      if (py<(0<<8)) py=0<<8;
      if (px>(31<<8)) px=31<<8;
      if (py>(31<<8)) py=31<<8;

      if (PEEK(0xD610)) {
	snprintf(m,80,"key $%02x pressed.\n",PEEK(0xD610));
	debug_msg(m);
	switch(PEEK(0xD610)) {
	case 0x31: i-=1; break;
	case 0x32: i+=1; break;
	case 0xF1: dma_draw^=1; break;
	  // Rotate left/right
	case 0xF3: diag_mode^=1;
	  if (diag_mode) text80_mode();
	  else graphics_mode();
	  break;
	case 0x1d: case 0x44: case 0x64: i+=ROTATE_STEP; i&=0x3ff; break;
	case 0x9d: case 0x41: case 0x61: i-=ROTATE_STEP; i&=0x3ff; break;

	  // Move forewards/backwards
	case 0x11: case 0x53: case 0x73:

	  py-=MulCos(i,STEP);	    
	  px-=MulSin(i,STEP);

	  if (IsWall(px>>8,py>>8)) {
	    py+=MulCos(i,STEP);	    
	    px+=MulSin(i,STEP);
	  }
	  
	  break;
	case 0x91: case 0x57: case 0x77:

	  py+=MulCos(i,STEP);	    
	  px+=MulSin(i,STEP);

	  if (IsWall(px>>8,py>>8)) {
	    py-=MulCos(i,STEP);	    
	    px-=MulSin(i,STEP);
	  }
	  
	  break;
	}
	POKE(0xD610,0);
      }

      i&=0x3FF;      

      if (dma_draw)
	TraceFrameFast(px,py,i);
      else
	TraceFrame(px,py,i);
      
      // XXX wait for key between frames
      while(!PEEK(0xD610)) continue;
    }
}
