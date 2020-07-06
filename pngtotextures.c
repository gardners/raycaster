/*
 * Copyright 2002-2010 Guillaume Cottenceau.
 * Copyright 2015-2018 Paul Gardner-Stephen.
 *
 * This software may be freely redistributed under the terms
 * of the X11 license.
 *
 */

/* ============================================================= */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>

#define PNG_DEBUG 3
#include <png.h>

/* ============================================================= */

int x, y;

int multiplier;

int width, height;
png_byte color_type;
png_byte bit_depth;

png_structp png_ptr;
png_infop info_ptr;
int number_of_passes;
png_bytep * row_pointers;

FILE *infile;
FILE *outfile;

unsigned char sprite_data[65536];

/* ============================================================= */

void abort_(const char * s, ...)
{
  va_list args;
  va_start(args, s);
  vfprintf(stderr, s, args);
  fprintf(stderr, "\n");
  va_end(args);
  abort();
}

/* ============================================================= */

void read_png_file(char* file_name)
{
  unsigned char header[8];    // 8 is the maximum size that can be checked

  /* open file and test for it being a png */
  infile = fopen(file_name, "rb");
  if (infile == NULL)
    abort_("[read_png_file] File %s could not be opened for reading", file_name);

  fread(header, 1, 8, infile);
  if (png_sig_cmp(header, 0, 8))
    abort_("[read_png_file] File %s is not recognized as a PNG file", file_name);

  /* initialize stuff */
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png_ptr)
    abort_("[read_png_file] png_create_read_struct failed");

  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    abort_("[read_png_file] png_create_info_struct failed");

  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[read_png_file] Error during init_io");

  png_init_io(png_ptr, infile);
  png_set_sig_bytes(png_ptr, 8);

  // Convert palette to RGB values
  png_set_expand(png_ptr);

  png_read_info(png_ptr, info_ptr);

  width = png_get_image_width(png_ptr, info_ptr);
  height = png_get_image_height(png_ptr, info_ptr);
  color_type = png_get_color_type(png_ptr, info_ptr);
  bit_depth = png_get_bit_depth(png_ptr, info_ptr);

  fprintf(stderr,"Input-file is: width=%d, height=%d.\n", width, height);

  number_of_passes = png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);

  /* read file */
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[read_png_file] Error during read_image");

  row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
  for (y=0; y<height; y++)
    row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr,info_ptr));

  png_read_image(png_ptr, row_pointers);

  if (infile != NULL) {
    fclose(infile);
    infile = NULL;
  }

}


struct rgb {
  int r;
  int g;
  int b;
};

struct rgb palette[256];
int palette_first=16;
int palette_index=16; // reserve the C64 colours

int palette_lookup(int r,int g, int b)
{
  int i;

  // Do we know this colour already?
  for(i=palette_first;i<palette_index;i++) {
    if (r==palette[i].r&&g==palette[i].g&&b==palette[i].b) {
      return i;
    }
  }
  
  // new colour
  if (palette_index>255) {
    fprintf(stderr,"Too many colours in image: Must be not more than 16.\n");
    exit(-1);
  }
  
  // allocate it
  palette[palette_index].r=r;
  palette[palette_index].g=g;
  palette[palette_index].b=b;
  return palette_index++;
  
}

unsigned char nyblswap(unsigned char in)
{
  return ((in&0xf)<<4)+((in&0xf0)>>4);
}

/*
  Simple RLE type packer, so that we can stash more textures in a single 64KB executable,
  which we can then extract into a higher memory bank on loading. */
uint8_t recent_bytes[14]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t recent_copy_bytes[14]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

uint8_t texture_data[128*1024];
int texture_offset=0;
uint8_t packed_data[1024*1024];
int packed_len=0;
uint8_t unpacked_data[1024*1024];
int unpacked_len=0;

void copy_recent_list(void)
{
  for(int i=0;i<14;i++) recent_copy_bytes[i]=recent_bytes[i];
}

int recent_index(uint8_t v)
{
  for(int i=0;i<14;i++) if (v==recent_bytes[i]) return i;
  return 255;
}
int recent_copy_index(uint8_t v)
{
  for(int i=0;i<14;i++) if (v==recent_copy_bytes[i]) return i;
  return 255;
}

int update_recent_bytes(uint8_t v)
{
  int index=recent_index(v);
  if (index>13) index=13;
  for(int i=index;i>=1;i--) recent_bytes[i]=recent_bytes[i-1];
  recent_bytes[0]=v;
  fprintf(stderr,"Putting $%02 at head of recent values.\n",v);
}

int update_copy_recent_bytes(uint8_t v)
{
  int index=recent_copy_index(v);
  if (index>13) index=13;
  for(int i=index;i>=1;i--) recent_copy_bytes[i]=recent_copy_bytes[i-1];
  recent_copy_bytes[0]=v;
}

void unpack_textures(void)
{
  int ofs=0;

  uint8_t value;
  int count;
  
  int last_unpacked_len=0;
  
  // Reset recent bytes list
  for(int i=0;i<14;i++) recent_bytes[i]=0;
  
  while((packed_data[ofs]!=0xe0)) {

    fprintf(stderr,"Depacking %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    packed_data[ofs+0],
	    packed_data[ofs+1],
	    packed_data[ofs+2],
	    packed_data[ofs+3],
	    packed_data[ofs+4],
	    packed_data[ofs+5],
	    packed_data[ofs+6],
	    packed_data[ofs+7]);
    fprintf(stderr,"Recent bytes: ");
    for(int i=0;i<14;i++) fprintf(stderr,"%02x ",recent_bytes[i]);
    fprintf(stderr,"\n");
    
    int errors=0;
    for(int o=0;o<unpacked_len;o++) {
      if (unpacked_data[o]!=texture_data[o]) {
	fprintf(stderr,"ERROR: Unpacked data at offset $%04x should be $%02x, but saw $%02x\n",
		texture_data[o],unpacked_data[o]);
	errors++;
      }
    }
    if (errors) exit(-3);
    
    if (ofs>=packed_len||unpacked_len>texture_offset) {
      fprintf(stderr,"Error verifying decompression.\n");
      exit(-1);
    }

    if (packed_data[ofs]==0xff) {
      // Long RLE sequence
      count=packed_data[++ofs];
      value=packed_data[++ofs];
      update_recent_bytes(value);
      while(count--) unpacked_data[unpacked_len++]=value;
    } else if ((packed_data[ofs]&0xf0)==0xe0) {
      // String of non-packed bytes
      if ((packed_data[ofs]&0x0f)==0x0f)
	count=packed_data[++ofs];
      else
	count=packed_data[ofs]&0x0f;
      while(count--) {
	unpacked_data[unpacked_len++]=packed_data[++ofs];
	update_recent_bytes(packed_data[ofs]);
      }
    } else if ((packed_data[ofs]&0xf0)==0xf0) {
      // Short RLE sequence
      count=packed_data[ofs]&0x0f;
      value=packed_data[++ofs];
      update_recent_bytes(value);
      while(count--) unpacked_data[unpacked_len++]=value;
    } else {
      value=recent_bytes[packed_data[ofs]>>4];
      update_recent_bytes(value);
      count=recent_bytes[packed_data[ofs]]&0x0f;
      if (count==15) {
	while(count--) {
	  unpacked_data[unpacked_len++]=packed_data[++ofs];
	  update_recent_bytes(packed_data[ofs]);
	}
      } else {
	// Actually a 2nd recent byte
	value=recent_bytes[count];
	unpacked_data[unpacked_len++]=value;
	update_recent_bytes(value);	
	ofs++;
      }
    }
  }
}


void pack_textures(void)
{
  int last_packed_len=0;
  
  for(int i=0;i<14;i++) recent_bytes[i]=0;
  for(int i=0;i<texture_offset;) {
    int count=0;
    int index=recent_index(texture_data[i]);
    int index_next=recent_index(texture_data[i+1]);
    while(texture_data[i+count]==texture_data[i]) count++;

    fprintf(stderr,"Packed data emitted: ");
    while(last_packed_len<packed_len) fprintf(stderr,"%02x ",packed_data[last_packed_len++]);
    fprintf(stderr,"\n");

    fprintf(stderr,"@$%04x : %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    i,
	    texture_data[i+0],
	    texture_data[i+1],
	    texture_data[i+2],
	    texture_data[i+3],
	    texture_data[i+4],
	    texture_data[i+5],
	    texture_data[i+6],
	    texture_data[i+7]);
    fprintf(stderr,"index=%d,%d, count=%d\n",index,index_next,count);
    fprintf(stderr,"Recent bytes: ");
    for(int i=0;i<14;i++) fprintf(stderr,"%02x ",recent_bytes[i]);
    fprintf(stderr,"\n");
    
    if (count<4) {
      if (index<14&&index_next<14) {
		
	// Two bytes we have seen recently, so pack them in a single byte
	// $<2nd byte index><first byte index>
	packed_data[packed_len++]=index_next+(index<<4);
	// Update recent data table
	update_recent_bytes(texture_data[i]);
	update_recent_bytes(texture_data[i+1]);
	i+=2;
      } else if (index<14) {
	// Two bytes, the 2nd of which is not recent
	// We need to know how many bytes to encode raw
	// after this point.
	int nonpackable_count=1;
	copy_recent_list();
	while((i+nonpackable_count)<texture_offset) {
	  if (recent_index(texture_data[i+nonpackable_count])<14) break;
	  if (recent_copy_index(texture_data[i+nonpackable_count])<14) break;
	  update_copy_recent_bytes(texture_data[i+nonpackable_count]);
	}
	// XXX Need dynamic programming to optimise this.
	// We'll just use a greedy algorithm for now.
	packed_data[packed_len++]=(index<<4)+0xf;
	if (nonpackable_count>255) nonpackable_count=255;
	packed_data[packed_len++]=nonpackable_count;
	for(int j=0;j<nonpackable_count;j++) {
	  packed_data[packed_len++]=texture_data[i+1+j];
	  update_recent_bytes(texture_data[i+j]);
	}
	i+=1+nonpackable_count;
      } else {
	// One or more non-recent bytes
	int nonpackable_count=1;
	copy_recent_list();
	while((i+nonpackable_count)<texture_offset) {
	  if (recent_index(texture_data[i+nonpackable_count])<14) break;
	  if (recent_copy_index(texture_data[i+nonpackable_count])<14) break;
	  update_copy_recent_bytes(texture_data[i+nonpackable_count]);
	}
	if (nonpackable_count>255) nonpackable_count=255;
	if (nonpackable_count<15) {
	  packed_data[packed_len++]=0xe0+nonpackable_count;	  
	} else {
	  packed_data[packed_len++]=0xef;
	  packed_data[packed_len++]=nonpackable_count;
	}
	for(int j=0;j<nonpackable_count;j++) {
	  packed_data[packed_len++]=texture_data[i+j];
	  update_recent_bytes(texture_data[i+j]);
	}
	i+=nonpackable_count;	
      }
    } else {
      // More than 4 identical bytes: RLE encode
      uint8_t the_value=texture_data[i];
      update_recent_bytes(the_value);
      i+=count;
      while(count) {	
	if (count>=15) {
	  // RLE encode 15 identical bytes, whether seen recently or not
	  // $FF <count> <byte>
	  packed_data[packed_len++]=0xFF;
	  packed_data[packed_len++]=count<255?count:255;
	  packed_data[packed_len++]=the_value;
	  if (count>255) count-=255; else count=0;
	} else {
	  // $F<count> <byte>
	  packed_data[packed_len++]=0xF0+count;
	  packed_data[packed_len++]=the_value;
	  count=0;
	}
      }
    }
  }
  // End stream with byte that cannot occur otherwise
  packed_data[packed_len++]=0xe0;

  fprintf(stderr,"Packed texture data into %d bytes.\n",packed_len);

  // Test decompression
  unpack_textures();
  fprintf(stderr,"Verified decompression. Extracted size is %d bytes.\n",unpacked_len);
  
}




/* ============================================================= */

int main(int argc, char **argv)
{

  for(int i=1;i<argc;i++)
    {
      if (texture_offset>=(128*1024)) {
	fprintf(stderr,"Too many textures.\n");
	exit(-1);
      }
      
      fprintf(stderr,"Reading %s\n",argv[i]);
      read_png_file(argv[i]);
      fprintf(stderr,"Image is %dx%d\n",width,height);
      if (width%64||height!=64) {
	fprintf(stderr,"ERROR: Texture images must all be (64 x n)x64 pixels.\n");
	exit(-1);
      }
      
      int multiplier=-1;
      if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB)
	multiplier=3;
      
      if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGBA)
	multiplier=4;
      
      if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY)
	multiplier=1;
      
      if (multiplier==-1) {
	fprintf(stderr,"Could not convert file to grey-scale, RGB or RGBA\n");
      }
      if (multiplier<3) {
	fprintf(stderr,"PNG images must be RGB or RGBA\n");
	exit(-1);
      }

      long long pixel_value=0;
      for(int xx=0;xx<width;xx++) {
	for(int yy=0;yy<64;yy++) {
	  int red=row_pointers[yy][xx*multiplier+0];
	  int green=row_pointers[yy][xx*multiplier+1];
	  int blue=row_pointers[yy][xx*multiplier+2];
	  int pixel_value=palette_lookup(red,green,blue);
	  texture_data[texture_offset++]=pixel_value;
	}
      }
    }
  fprintf(stderr,"%d bytes of texture data used %d colours\n",
	  texture_offset,palette_index);

  printf("#include <stdint.h>\n");
  printf("const uint8_t colours[768] = {\n");
  for(int i=0;i<256;i++) {
    printf("0x%02x%c",(palette[i].r>>4)|((palette[i].r<<4)&0xe0),
	   i<255?',':',');
    if ((i&0xf)==0xf) printf("\n");
  }
  for(int i=0;i<256;i++) {
    printf("0x%02x%c",(palette[i].g>>4)|((palette[i].g<<4)&0xf0),
	   i<255?',':',');
    if ((i&0xf)==0xf) printf("\n");
  }
  for(int i=0;i<256;i++) {
    printf("0x%02x%c",(palette[i].b>>4)|((palette[i].b<<4)&0xf0),
	   i<255?',':' ');
    if ((i&0xf)==0xf) printf("\n");
  }
  printf("};\n\n");

  printf("const uint8_t textures[%d] = {\n",texture_offset);
  for(int i=0;i<texture_offset;i++) {
    printf("0x%02x%c",texture_data[i],i<(texture_offset-1)?',':' ');
    if ((i&0xf)==0xf) printf("\n");
  }
  printf("};\n\n");  

  FILE *f=fopen("textures.h","w");
  if (f) {
    fprintf(f,"#define NUM_TEXTURES %d\n",texture_offset/(64*64));
    fprintf(f,"extern const uint8_t colours[768];\n");
    fprintf(f,"extern const uint8_t textures[%d];\n",texture_offset);
    fclose(f);
  }

  pack_textures();
  
  return 0;  
}

/* ============================================================= */
