#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <cbm.h>

#include <hal.h>
#include <memory.h>
#include <dirent.h>
#include <fileio.h>
#include <debug.h>
#include <random.h>
#include <mouse.h>

#define SCREEN_ADDR 0xE000L
#define TEXTURE_ADDRESS 0x8000000L

extern unsigned short mouse_x,mouse_y;
unsigned short last_mouse_x;

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

uint8_t maze_size=9;
uint32_t maze_seed=1;

unsigned short i,j;
unsigned char a,b,c,d;

unsigned char diag_mode=0;

char msg[160+1];

/* Simple depacker for compressed textures
 */
uint8_t recent_bytes[14]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t recent_index(uint8_t v)
{
  for(a=0;a<14;a++) if (v==recent_bytes[a]) return a;
  return 255;
}
void update_recent_bytes(uint8_t v)
{
#if 0
  snprintf(msg,160,"moving $%02x to head of recent bytes: %02x %02x %02x %02x ...\n",
	   v,recent_bytes[0],recent_bytes[1],recent_bytes[2],recent_bytes[3]);
  debug_msg(msg);
#endif
  a=recent_index(v);
  if (a>13) a=13;
  for(b=a;b>=1;b--) recent_bytes[b]=recent_bytes[b-1];
  recent_bytes[0]=v;
}

uint16_t ofs=0;
uint8_t value,value2;
int16_t count;

uint32_t unpacked_len=0;


void unpack_textures(const uint8_t *packed_data)
{

  unpacked_len=0;
  // Reset recent bytes list
  for(value=0;value<14;value++) recent_bytes[value]=0;
  
  while((packed_data[ofs]!=0xe0)) {

#if 0
    snprintf(msg,160,"depacking @ $%04x : %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    ofs,
	    packed_data[ofs+0],
	    packed_data[ofs+1],
	    packed_data[ofs+2],
	    packed_data[ofs+3],
	    packed_data[ofs+4],
	    packed_data[ofs+5],
	    packed_data[ofs+6],
	    packed_data[ofs+7]);
    debug_msg(msg);
    snprintf(msg,160,"recent bytes: %02x %02x %02x %02x %02x %02x ...\n",
	     recent_bytes[0],recent_bytes[1],recent_bytes[2],recent_bytes[3],
	     recent_bytes[4],recent_bytes[5],recent_bytes[6],recent_bytes[7]);
    debug_msg(msg);
#endif
    
    // Show signs of life while depacking textures
    POKE(0xD020,(PEEK(0xD020)+1)&0x0f);
    
    if (packed_data[ofs]==0xff) {
      // Long RLE sequence
      count=packed_data[++ofs];
      value=packed_data[++ofs];
      update_recent_bytes(value);
      lfill(TEXTURE_ADDRESS+unpacked_len,value,count);
      unpacked_len+=count;
      ofs++;
    } else if ((packed_data[ofs]&0xf0)==0xe0) {
      // String of non-packed bytes
      if ((packed_data[ofs]&0x0f)==0x0f)
	count=packed_data[++ofs];
      else
	count=packed_data[ofs]&0x0f;
#if 0
      snprintf(msg,160,"extracing %d raw bytes.\n",count);
      debug_msg(msg);
#endif
      lcopy(&packed_data[++ofs],TEXTURE_ADDRESS+unpacked_len,count);
      unpacked_len+=count;
      while(count--) {
	update_recent_bytes(packed_data[ofs++]);
      }
    } else if ((packed_data[ofs]&0xf0)==0xf0) {
      // Short RLE sequence
      count=packed_data[ofs]&0x0f;
      value=packed_data[++ofs];
      lfill(TEXTURE_ADDRESS+unpacked_len,value,count);
      unpacked_len+=count;
      update_recent_bytes(value);
      ofs++;
    } else {
      //      fprintf(stderr,"Decoding $%02x\n",packed_data[ofs]);
      value=recent_bytes[packed_data[ofs]>>4];
      count=packed_data[ofs]&0x0f;
      if (count==15) {
	lpoke(TEXTURE_ADDRESS+unpacked_len,value);
	unpacked_len++;
	update_recent_bytes(value);
	count=packed_data[++ofs];
	ofs++;
	lcopy(&packed_data[ofs],TEXTURE_ADDRESS+unpacked_len,count);
	unpacked_len+=count;
	while(count--) {
	  update_recent_bytes(packed_data[ofs++]);
	}
      } else {
	// Actually a 2nd recent byte
	value2=recent_bytes[count];
	//	fprintf(stderr,"Looking up 2nd value from index #%d = $%02x\n",count,value2);
	update_recent_bytes(value);
	lpoke(TEXTURE_ADDRESS+unpacked_len,value);	
	lpoke(TEXTURE_ADDRESS+unpacked_len+1,value2);	
	unpacked_len+=2;
	update_recent_bytes(value2);
	ofs++;
      }
    }
  }
}





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

// Set horizontal position of the overlay text
void overlaytext_line_x_position(uint8_t row,uint16_t x_pos)
{
  POKE(SCREEN_ADDR+row*160+40*2+0,x_pos);
  POKE(SCREEN_ADDR+row*160+40*2+1,x_pos>>8);
}

void overlaytext_clear_line(uint8_t row)
{
  for(a=41;a<80;a++) {
    POKE(SCREEN_ADDR+row*160+a*2+0,0x20);
    POKE(SCREEN_ADDR+row*160+a*2+1,0x00);      
  }
}

void graphics_mode(void)
{

  // Clear screen RAM first, so that there is no visible glitching
  lfill(0x40000L,0x00,0x8000);
  lfill(0x48000L,0x00,0x8000);

  // Set up palette from texture data
  lcopy(0x8000010,0xffd3110L,0xf0);
  lcopy(0x8000110,0xffd3210L,0xf0);
  lcopy(0x8000210,0xffd3310L,0xf0);
  
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
  POKE(0xD060,SCREEN_ADDR&0xff);
  POKE(0xD061,SCREEN_ADDR>>8);
  POKE(0xD062,SCREEN_ADDR>>16);

  // Layout screen so that graphics data comes from $40000 -- $5FFFF

  i=0x40000/0x40;
  for(a=0;a<80;a++)
    for(b=0;b<25;b++) {
      POKE(SCREEN_ADDR+b*160+a*2+0,i&0xff);
      POKE(SCREEN_ADDR+b*160+a*2+1,i>>8);

      i++;
    }
   
  // Clear colour RAM, 8-bits per pixel
  lfill(0xff80000L,0x00,80*25*2);

  // Now setup raster buffer text over write to give us 39 columns of text
  // as overlay.
  for(b=0;b<25;b++) {
    // Make overlay full of spaces
    overlaytext_clear_line(b);
    // Put the GOTO tokens in to move back to the 2nd char position of the chargen row
    // (We can't go back to the start, because we have used one of the 40 extra columns
    // for the goto token).
    overlaytext_line_x_position(b,0x0007);
    // And activate it in colour RAM
    lpoke(0xFF80000L+b*160+40*2+0,0x90);
    lpoke(0xFF80000L+b*160+40*2+1,0x00);
  }
  
  
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
uint8_t rev_mode=0;

void print_overlaytext(unsigned char x,unsigned char y,unsigned char colour,char *msg)
{  
  if (colour&0x10) rev_mode=0x80; else rev_mode=0;
  
  pixel_addr=SCREEN_ADDR+x*2+y*160+82;
  while(*msg) {
    char_code=*msg;
    if (*msg>=0xc0&&*msg<=0xe0) char_code=*msg-0x80;
    else if (*msg>=0x40&&*msg<=0x60) char_code=*msg-0x40;
    else if (*msg>=0x60&&*msg<=0x7A) char_code=*msg-0x20;
    POKE(pixel_addr+0,char_code|rev_mode);
    lpoke(0xff80000L-SCREEN_ADDR+pixel_addr+1,colour);
    msg++;
    pixel_addr+=2;        
  }
}

void print_overlayraw(unsigned char x,unsigned char y,unsigned char colour,char *msg)
{  
  if (colour&0x10) rev_mode=0x80; else rev_mode=0;
  
  pixel_addr=SCREEN_ADDR+x*2+y*160+82;
  while(*msg) {
    char_code=*msg;
    POKE(pixel_addr+0,char_code|rev_mode);
    lpoke(0xff80000L-SCREEN_ADDR+pixel_addr+1,colour);
    msg++;
    pixel_addr+=2;        
  }
}

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

int pa;
uint16_t px=1<<8;
uint16_t py=1<<8;

void setup_level(uint8_t size,uint32_t seed)
{
  // Generate a maze
  // Must have odd size, so walls an corridors can co-exist.
  
  generate_maze(size,size,seed);
#if 0
  for(px=0;px<40;px++)
    for(py=0;py<40;py++) {
      if (maze_get_cell(px,py)) plot_pixel(px,py,0x80);
      else plot_pixel(px,py,0x00);
    }
#endif
  
  // Make the exit square look like sky
  if (!(maze_get_cell(size-2,size-3)&0x8000))
    maze_set_cell(size-2,size-1,0x8000);
  else
    maze_set_cell(size-1,size-2,0x8000);
  
  // Start in middle of start square, looking down the maze
  px=0x0180; py=0x180;
  if (!IsWall(1,2)) i=0x000;
  else i=0x100;

  // Erase map sprite
  lfill(0x400,0x00,512);
  
}

// From: https://retrocomputing.stackexchange.com/questions/8253/how-to-read-disk-files-using-cbm-specific-functions-in-cc65-with-proper-error-ch
unsigned char kernel_getin(void *ptr, unsigned char size)
{
  unsigned char * data = (unsigned char *)ptr;
  unsigned char i;
  unsigned char st=0;
  
  for(i=0; i<size; ++i)
    {
      st = cbm_k_readst();
      if (st) {
	break;
      }
      data[i] = cbm_k_getin();
    }
  data[i] = '\0';
  size = i;
  return st;
}

extern uint32_t texture_offset;
extern uint8_t texture_num;
extern uint16_t texture_count;

uint8_t disk_buffer[256];
uint16_t load_textures(unsigned char * file_name)
{
  cbm_k_setlfs(3,8,0);
  cbm_k_setnam(file_name);
  
  cbm_k_open();
  if(!cbm_k_readst()) 
    {
      _filetype = 'p';

      printf("Loading textures");
      
      cbm_k_chkin(3);
      texture_offset=0;
      
      while(!cbm_k_readst()) {
	// Use 128 byte reads so our DMA jobs never cross a 64KB boundary
	// (which should be fine, but something fishy is going on. Maybe
	// more a hyperram bug than DMAgic bug?)
	kernel_getin(disk_buffer,128);
	lcopy(disk_buffer,0x8000000L+texture_offset,128);
	texture_offset+=128;
	// Border activity while loading
	POKE(0xD020,(PEEK(0xD020)+1)&0xf);
	if (!(texture_offset&0xfff)) {
	  // And show a . every texture
	  printf(".");
	  cbm_k_chkin(3);
	}
      }
      
    }
  else
    {
      printf("File could not be opened\n\r");
      texture_count=0;
      return 0;
    }

  // Disable interrupts again, as DOS will have enabled them  
  asm ( "sei" );
  // And M65 IO
  POKE(0xD02F,0x47);
  POKE(0xD02F,0x53);

  // Copy sky and floor textures to internal RAM for speed
  lcopy(0x8000300L,0x12000L,4096*9);
  
  texture_count=texture_offset>>12;
  return texture_offset>>12;
}

uint8_t game_setup=1;
uint8_t map_x_target=0;
uint8_t map_x_current=0;

#define PULSE_SEQ_LENGTH 8
uint8_t text_colour=0, pulse_position=0;
uint8_t pulse_sequence[PULSE_SEQ_LENGTH]={0,2,8,7,5,6,4,10};

void generate_idle_map(void)
{
  setup_level(13,1);

  // Carve out an area around us for the rotating camera view
  for(px=6;px<9;px++)
    for(py=6;py<9;py++)
      maze_set_cell(px,py,0);

  px=0x780; py=0x780; pa=0x100;  
}

uint8_t logo[5][34]={
  {0xa0, 0xdf, 0xe9, 0xa0, 0x20, 0xa0, 0xa0, 0xa0, 0x20, 0xa0, 0xa0, 0xa0, 0x20, 0xa0, 0xa0, 0xa0,
   0x20, 0xa0, 0xdf, 0xe9, 0xa0, 0x20, 0xa0, 0xa0, 0xa0, 0x20, 0xa0, 0xa0, 0xa0, 0x20, 0xa0, 0xa0, 0xa0,0},
  {0xa0, 0xa0, 0xa0, 0xa0, 0x20, 0xa0, 0x20, 0x20, 0x20, 0xa0, 0x20, 0x20, 0x20, 0xa0, 0x20, 0xa0,
   0x20, 0xa0, 0xa0, 0xa0, 0xa0, 0x20, 0xa0, 0x20, 0xa0, 0x20, 0x20, 0xe9, 0xa0, 0x20, 0xa0, 0x20, 0x20,0},
  {0xa0, 0x5f, 0x69, 0xa0, 0x20, 0xa0, 0xa0, 0xa0, 0x20, 0xa0, 0x5f, 0xa0, 0x20, 0xa0, 0xa0, 0xa0,
   0x20, 0xa0, 0x5f, 0x69, 0xa0, 0x20, 0xa0, 0xa0, 0xa0, 0x20, 0xe9, 0xa0, 0x69, 0x20, 0xa0, 0xa0, 0xa0,0},
  {0xa0, 0x20, 0x20, 0xa0, 0x20, 0xa0, 0x20, 0x20, 0x20, 0xa0, 0x20, 0xa0, 0x20, 0xa0, 0x20, 0xa0,
   0x20, 0xa0, 0x20, 0x20, 0xa0, 0x20, 0xa0, 0x20, 0xa0, 0x20, 0xa0, 0x69, 0x20, 0x20, 0xa0, 0x20, 0x20,0},
  {0xa0, 0x20, 0x20, 0xa0, 0x20, 0xa0, 0xa0, 0xa0, 0x20, 0xa0, 0xa0, 0xa0, 0x20, 0xa0, 0x20, 0xa0,
   0x20, 0xa0, 0x20, 0x20, 0xa0, 0x20, 0xa0, 0x20, 0xa0, 0x20, 0xa0, 0xa0, 0xa0, 0x20, 0xa0, 0xa0, 0xa0,0}
};  

void main(void)
{
  asm ( "sei" );

  // Fast CPU, M65 IO
  POKE(0,65);
  POKE(0xD02F,0x47);
  POKE(0xD02F,0x53);

  // Clear key input buffer
  while(PEEK(0xD610)) POKE(0xD610,0);

  POKE(0xD020,0);
  POKE(0xD021,0);

  // Clear screen
  printf("%c",0x93);
  
  load_textures("textures.bin");
  printf("Loaded %d textures.\n",texture_count);
  
  // Force lower-case font
  POKE(0xD018,0x16);

  graphics_mode();

  // Disable hotregs
  POKE(0xD05D,PEEK(0xD05D)&0x7f);
  
  setup_multiplier();

  generate_idle_map();

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
  //  POKE(0xD04B,PEEK(0xD04B)|0x90);
  // Make spriet 64px wide and 64px high
  POKE(0xD057,0x90);
  POKE(0xD056,63); // extended height sprites are 63 px tall
  POKE(0xD055,0x90); // and make our sprite so.
  // Now set sprite pointer list to somewhere we can modify, so that we can put the sprite data somewhere
  // useful, since it will be 64x64 = 4kbit = 512 bytes.
  // Well, we aren't using the screen, so we can just put it there.
  POKE(0x07FC,0x400/0x40);
  POKE(0x07FF,0x600/0x40);
  // And erase it
  lfill(0x400,0x00,512);
  lfill(0x600,0xFF,512-8);

  POKE(0xD02B,0x07);
  POKE(0xD02E,0x09);

  
  // Make single pixel sprite for showing our location
  lfill(0x380,0x00,63);
  POKE(0x380,0x80);
  POKE(0x07F8,0x0380/0x40);

  //  while(1) POKE(0xD020,PEEK(0xD012));

  // Draw an initial maze frame
  TraceFrameFast(px,py,pa);

  // Start in game setup mode
  game_setup=1;
  
  while(1)
    {

      if (game_setup) {
	POKE(0xD015,0);

	pa+=last_frame_duration;
	pa&=0x3ff;

      last_frame_start=*(uint16_t *)0xDC08;

      TraceFrameFast(px,py,pa);

      last_frame_duration=(*(uint16_t *)0xDC08) - last_frame_start;
      last_frame_duration&=0xff;
      while (last_frame_duration>9) last_frame_duration+=10;

	
	// Limit mouse to screen
	mouse_set_bounding_box(0,0,319,199);

	// Force upper-case font
	POKE(0xD018,0x14);
	
	for(b=0;b<25;b++) overlaytext_line_x_position(b,0x0007);

	text_colour=pulse_sequence[pulse_position];
	pulse_position++;
	if (pulse_position>PULSE_SEQ_LENGTH) pulse_position=0;

	print_overlaytext(7,1,0,"the mega65 team presents:");

	for(i=0;i<5;i++) print_overlayraw(3,i+4,text_colour,logo[i]);
	snprintf(msg,20,"maze size: %d  ",maze_size);
	print_overlaytext(14,17,0x00,msg);
	snprintf(msg,22,"maze seed: %08ld      ",maze_seed);
	print_overlaytext(10,19,0x00,msg);
	snprintf(msg,22,"press return to start");
	print_overlaytext(9,23,0x00,msg);
	print_overlaytext(9+6,23,0x10+text_colour,"return");

	// Update map sprite positions based on maze size
	if (maze_size>11) {

	  POKE(0x07FC,0x400/0x40);
	  POKE(0x07FF,0x600/0x40);
	  
	  POKE(0xD00F,maze_size-13);
	  POKE(0xD009,maze_size-13);

	  POKE(0xD056,63); // extended height sprites are 63 px tall
	  
	} else {

	  // Cut the top 16 lines of sprite data
	  POKE(0x07FC,0x480/0x40);
	  POKE(0x07FF,0x680/0x40);
	  
	  POKE(0xD00F,maze_size);
	  POKE(0xD009,maze_size);

	  POKE(0xD056,63-16); // extended height sprites are 63 px tall
	}
	map_x_target=0x18+63-maze_size;
	map_x_current=map_x_target;
	POKE(0xD00E,0x18+63-maze_size);
	POKE(0xD008,0x18+63-maze_size);

	
	if (PEEK(0xD610)) {
	  switch(PEEK(0xD610)) {
	  case 0x11: maze_seed--; if (maze_seed<1) maze_seed=99999999; break;
	  case 0x91: maze_seed++; if (maze_seed>99999999) maze_seed=1; break;
	  case 0x1d: maze_size+=2; if (maze_size>63) maze_size=63; break;
	  case 0x9d: maze_size-=2; if (maze_size<5) maze_size=5; break;
	  case 0x0d:
	    // Start game
	    game_setup=0;
	    // 100 million mazes per size, with different seed range for each
	    // to prevent similarity of initial passages
	    setup_level(maze_size,maze_seed+(maze_size/2)*100000000);
	    px=0x180; py=0x180; pa=0;
	    // Make sure we don't start facing a wall
	    if (IsWall(1,2)) pa=0x100;
	    
	    // Slide text out to the right off the screen
	    for(i=7;i<330;i+=8) {
	      for(b=5;b<25;b++)
		overlaytext_line_x_position(b,i);
	      while(PEEK(0xD012)!=0xfe) continue;
	    }
	    for(b=0;b<25;b++)
	      overlaytext_clear_line(b);
	    // MAP sprites visible
	    POKE(0xD015,0x91);

	    // Allow mouse to generate full range of rotations, and place mouse in the
	    // middle
	    mouse_set_bounding_box(0,0,1023,1023);
	    mouse_x=512; mouse_y=512;
	    
	  }
	  POKE(0xD610,0);
	}
	
	continue;
      }

      
      
      // Update map sprite based on where we are
      if (px<0x100) a=0; else a=(px>>8);
      if (py<0x100) b=0; else b=(py>>8);
      if (!IsWall(a,b)) mapsprite_set_pixel(a,b);

      // And set our pulsing location sprite in the right place
      POKE(0xD000,map_x_current+a);
      if (maze_size>11)
	POKE(0xD001,maze_size-13+(63-b));
      else
	POKE(0xD001,maze_size+(47-b));

      // Toggle colour of our little square
      POKE(0xD027,PEEK(0xD027)^0x01);

      
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

      // Update viewing angle based on mouse movement
      mouse_update_position(0,0);
      pa+=mouse_x-512;
      mouse_x=512; 
      
      if (PEEK(0xD610)) {
	switch(PEEK(0xD610)) {
	case 0x4d: case 0x6d:
	  POKE(0xD020,1);
	  // 87 = just off right edge
	  if (map_x_target==88) {
	    map_x_target=0x18+63-maze_size;
	  } else
	    map_x_target=88;

	  break;
	case 0x03:
	  game_setup=1;

	  generate_idle_map();
	  
	  break;
	case 0x31: pa-=1; break;
	case 0x32: pa+=1; break;
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
	  pa+=ROTATE_STEP*last_frame_duration; pa&=0x3ff;
      }
      if (left_held) {
	  pa-=ROTATE_STEP*last_frame_duration; pa&=0x3ff;
      }
      if (back_held) {
	py-=MulCos(pa,STEP*last_frame_duration);
	px-=MulSin(pa,STEP*last_frame_duration);
	
	if (IsWall(px>>8,py>>8)) {
	  py+=MulCos(pa,STEP*last_frame_duration);	    
	  px+=MulSin(pa,STEP*last_frame_duration);
	}
      }
      if (forward_held) {
	py+=MulCos(pa,STEP*last_frame_duration);	    
	px+=MulSin(pa,STEP*last_frame_duration);

	if (IsWall(px>>8,py>>8)) {
	  py-=MulCos(pa,STEP*last_frame_duration);
	  px-=MulSin(pa,STEP*last_frame_duration);
	}
      }
      
      pa&=0x3FF;      

      // Draw frame and keep track of how long it took
      // Use CIA RTC to time it, so we measure in 10ths of seconds
      last_frame_start=*(uint16_t *)0xDC08;

      TraceFrameFast(px,py,pa);

      last_frame_duration=(*(uint16_t *)0xDC08) - last_frame_start;
      last_frame_duration&=0xff;
      while (last_frame_duration>9) last_frame_duration+=10;

      // XXX This looks horribly jerky. It should instead be done on a
      // raster interrupt, which we will need to have at some point
      // for background music
      if (map_x_current<map_x_target) {
	map_x_current+=maze_size>>2;
	if (map_x_current>map_x_target) map_x_current=map_x_target;
	POKE(0xD00E,map_x_current);
	POKE(0xD008,map_x_current);
      }
      if (map_x_current>map_x_target) {
	map_x_current-=maze_size>>2;
	if (map_x_current<map_x_target) map_x_current=map_x_target;
	POKE(0xD00E,map_x_current);
	POKE(0xD008,map_x_current);
      }
      
    }
}
