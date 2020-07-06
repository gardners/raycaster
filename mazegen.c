#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <hal.h>
#include <memory.h>
#include <dirent.h>
#include <fileio.h>
#include <debug.h>
#include <random.h>

void plot_pixel(unsigned long x,unsigned char y,unsigned char colour);


#define MAZE_MEM 0x50000L

char dm[80+1];

void maze_set_cell(uint8_t x,uint8_t y,uint16_t v)
{
  lpoke(MAZE_MEM+(((uint16_t)x)<<1)+(((uint16_t)y)<<9),v&0xff);
  lpoke(MAZE_MEM+(((uint16_t)x)<<1)+(((uint16_t)y)<<9)+1,v>>8);
  plot_pixel(x,y,v?0x80:0);
}

uint16_t maze_get_cell(uint8_t x,uint8_t y)
{
  return lpeek(MAZE_MEM+(((uint16_t)x)<<1)+(((uint16_t)y)<<9))
    +((uint16_t)lpeek(MAZE_MEM+(((uint16_t)x)<<1)+(((uint16_t)y)<<9)+1)<<8);
}

uint8_t mx,my;
char mm[80];

void divide_maze(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2)
{
  uint8_t msx,msy;
  uint16_t n;
  
  if (x2-x1<1) return;
  if (y2-y1<1) return;

  // Work out the split point
  if (x2-x1>2) msx=rand8(x2-x1-2)+x1+1;
  else msx=x1+1;
  if (y2-y1>2) msy=rand8(y2-y1-2)+y1+1;
  else msy=y1+1;
  if (msy>y2) msy=y2-1;
  if (msx>x2) msx=x2-1;
  // Walls on even coords, corridors on odd
  msx&=0xfe;
  msy&=0xfe;

  // Pick split direction
  if ((x2-x1)>(y2-y1)) n=1;
  else if ((x2-x1)<(y2-y1)) n=0;
  else n=rand8(0)&1;

  if (!n) {
    // Split horizontally
    for(n=x1;n<=x2;n++) maze_set_cell(n,msy,0xffff);
    if (x2-x1) n=rand8(x2-x1)+x1; else n=x1;
    n|=1; // corridors on odd coordinates, walls on even
    maze_set_cell(n,msy,0);

    if (msy) divide_maze(x1,y1,x2,msy-1);
    if (msy<y2) divide_maze(x1,msy+1,x2,y2);
    
  } else {
    // Split vertically
    for(n=y1;n<=y2;n++) maze_set_cell(msx,n,0xffff);  
    if (y2-y1) n=rand8(y2-y1)+y1; else n=y1;
    n|=1; // corridors on odd coordinates, walls on even
    maze_set_cell(msx,n,0);

    if (msx) divide_maze(x1,y1,msx-1,y2);
    if (msx<x2) divide_maze(msx+1,y1,x2,y2);    
  }
  
  // Then we need to draw the walls and then put a hole in each  
}

void generate_maze(uint8_t width, uint8_t height,uint32_t seed)
{
  // Make mazes deterministic
  srand(seed);
  
  // Erase the upper 128KB of RAM to store 256x256 16-bit values
  lfill(MAZE_MEM,0x00,0x8000);
  lfill(MAZE_MEM,0x00,0x8000);
  lfill(MAZE_MEM,0x00,0x8000);
  lfill(MAZE_MEM,0x00,0x8000);

  // Draw walls around the area
  for(mx=0;mx<width;mx++) {
    maze_set_cell(mx,0,0xffff);
    maze_set_cell(mx,height-1,0xffff);
  }
  for(my=0;my<height;my++) {
    maze_set_cell(0,my,0xffff);
    maze_set_cell(width-1,my,0xffff);
  }
  
  // Now perform recursive division
  divide_maze(1,1,width-2,height-2);
}
 
