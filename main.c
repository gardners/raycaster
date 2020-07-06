#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <hal.h>
#include <memory.h>
#include <dirent.h>
#include <fileio.h>
#include <debug.h>
#include <random.h>

#include "textures.h"

// These are the step per jiffy. Multiply by 10 for movement per second
// 256 = one whole square
#define STEP 48
#define ROTATE_STEP 18

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

  // Set up palette from texture data
  lcopy(&colours[0x010],0xffd3110L,0xf0);
  lcopy(&colours[0x110],0xffd3210L,0xf0);
  lcopy(&colours[0x210],0xffd3310L,0xf0);
  
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

uint16_t last_frame_start=0;
uint16_t last_frame_duration=0;

uint8_t forward_held=0;
uint8_t back_held=0;
uint8_t right_held=0;
uint8_t left_held=0;

void mapsprite_set_pixel(uint8_t x,uint8_t y)
{
  if (x>63) return;
  pixel_addr=0x400+(x>>3)+((63-y)<<3);
  if (pixel_addr>0x5ff) return;
#if 1
  POKE(pixel_addr,PEEK(pixel_addr)|(1<<(7-(x&7))));
#endif
}

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
  generate_maze(63,63,1);
  for(px=0;px<40;px++)
    for(py=0;py<40;py++) {
      if (maze_get_cell(px,py)) plot_pixel(px,py,0x80);
      else plot_pixel(px,py,0x00);
    }

  // Start in middle of start square, looking down the maze
  px=0x0180; py=0x180;
  if (!IsWall(1,2)) i=0x000;
  else i=0x100;

  // Set up sprite for showing map discovery
  // It sits in the upper right of the screen
  POKE(0xD015,0x91);
  POKE(0xD00E,0x18);
  POKE(0xD00F,0x32);
  POKE(0xD008,0x18);
  POKE(0xD009,0x32);
  POKE(0xD010,0x91);
  // Make sprite a bit-plane modifier, so it kind of ghosts over the
  // background. this is also why we use sprite 7, so that it modifies
  // bit 7 of the underlying colour
  POKE(0xD04B,PEEK(0xD04B)|0x90);
  // Make spriet 64px wide and 64px high
  POKE(0xD057,0x90);
  POKE(0xD056,63); // extended height sprites are 63 px tall
  POKE(0xD055,0x90); // and make our sprite so.
  // Now set sprite pointer list to somewhere we can modify, so that we can put the sprite data somewhere
  // useful, since it will be 64x64 = 4kbit = 512 bytes.
  // Well, we aren't using the screen, so we can just put it there.
  POKE(0x07FF,0x400/0x40);
  POKE(0x07FC,0x600/0x40);
  // And erase it
  lfill(0x400,0x00,512);
  lfill(0x600,0xFF,512-8);

  // Make single pixel sprite for showing our location
  lfill(0x380,0x00,63);
  POKE(0x380,0x80);
  POKE(0x07F8,0x0380/0x40);
  
  while(1)
    {
      // Update map sprite based on where we are
      if (px<0x100) a=0; else a=(px>>8);
      if (py<0x100) b=0; else b=(py>>8);
      if (!IsWall(a,b)) mapsprite_set_pixel(a,b);

      // And set our pulsing location sprite in the right place
      POKE(0xD000,0x18+a); POKE(0xD001,0x32+63-b);
      POKE(0xD027,PEEK(0xD027)^0x80);

      
      POKE(0xD614,0x01);
      forward_held=(~PEEK(0xD613))&0x02; // W
      left_held=(~PEEK(0xD613))&0x04; // A 
      back_held=(~PEEK(0xD613))&0x20; // S
      POKE(0xD614,0x02);
      right_held=(~PEEK(0xD613))&0x04; // D

      if (!(PEEK(0xDC00)&1)) forward_held=1;
      if (!(PEEK(0xDC00)&2)) back_held=1;
      if (!(PEEK(0xDC00)&4)) left_held=1;
      if (!(PEEK(0xDC00)&8)) right_held=1;

      
      // Directly check the cursor left/up key status to tell the different situations apart
      POKE(0xD614,0x00);
      if (PEEK(0xD60F)&0x02) forward_held=1;
      else back_held|=(~PEEK(0xD613))&0x80; // Cursor down      
      if (PEEK(0xD60F)&0x01) left_held=1;
      else right_held|=(~PEEK(0xD613))&0x04; // Cursor right
      
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
	case 0x1d: case 0x44: case 0x64:
	  right_held=1;
	  break;
	case 0x9d: case 0x41: case 0x61:
	  left_held=1;
	  break;
	  // Move forewards/backwards
	case 0x11: case 0x53: case 0x73:

	  back_held=1;

	  break;
	case 0x91: case 0x57: case 0x77:

	  forward_held=1;

	  break;
	}
	POKE(0xD610,0);
      }

      if (right_held) {
	  i+=ROTATE_STEP*last_frame_duration; i&=0x3ff;
      }
      if (left_held) {
	  i-=ROTATE_STEP*last_frame_duration; i&=0x3ff;
      }
      if (back_held) {
	py-=MulCos(i,STEP*last_frame_duration);
	px-=MulSin(i,STEP*last_frame_duration);
	
	if (IsWall(px>>8,py>>8)) {
	  py+=MulCos(i,STEP*last_frame_duration);	    
	  px+=MulSin(i,STEP*last_frame_duration);
	}
      }
      if (forward_held) {
	py+=MulCos(i,STEP*last_frame_duration);	    
	px+=MulSin(i,STEP*last_frame_duration);

	if (IsWall(px>>8,py>>8)) {
	  py-=MulCos(i,STEP*last_frame_duration);
	  px-=MulSin(i,STEP*last_frame_duration);
	}
      }
	  
	  

      
      i&=0x3FF;      

      // Draw frame and keep track of how long it took
      // Use CIA RTC to time it, so we measure in 10ths of seconds
      last_frame_start=*(uint16_t *)0xDC08;
      if (dma_draw)
	TraceFrameFast(px,py,i);
      else
	TraceFrame(px,py,i);
      last_frame_duration=(*(uint16_t *)0xDC08) - last_frame_start;
      last_frame_duration&=0xff;
      while (last_frame_duration>9) last_frame_duration+=10;

      snprintf(m,80,"last frame took $%04x jiffies: %d,%d,%d,%d\n",last_frame_duration,
	       left_held,right_held,forward_held,back_held);
      debug_msg(m);
    }
}
