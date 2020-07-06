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

  printf("Input-file is: width=%d, height=%d.\n", width, height);

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

  printf("Input-file is read and now closed\n");
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

/* ============================================================= */

int cut_sprite(int x1,int y1,int x2,int y2)
{
  
  int xstep=(x2-x1)/24;
  int ystep=(y2-y1)/21;

  int x,y;
  int xx,yy;
  for(y=0;y<21;y++) {
    printf("  /* ");
    unsigned int bit_vector=0;
    for(x=0;x<24;x++)
      {
	
	long long pixel_value=0;
	for(yy=0;yy<ystep;yy++)
	  for(xx=0;xx<xstep;xx++) {
	    pixel_value+=row_pointers[y1+y*ystep+yy][(x1+x*xstep+xx)*multiplier];
	  }
	if (xstep||ystep)
	  pixel_value/=(xstep*ystep);
	else {
	  printf("Error: xstep=%d, ystep=%d\n",xstep,ystep);
	}
	//	printf("[%02x]",pixel_value&0xff);
	if (pixel_value<0x7f) printf("*"); else printf(".");
	bit_vector=bit_vector<<1;
	if (pixel_value<0x7f) bit_vector|=1;
      }
    printf("*/  ");
    printf("  0x%02x,0x%02x,0x%02x,\n",
	   (bit_vector>>16)&0xff,(bit_vector>>8)&0xff,(bit_vector>>0)&0xff);
  }
  printf("  0xff,\n");
  return 0;
}


/* ============================================================= */

int main(int argc, char **argv)
{

  for(int i=1;i<argc;i++)
    {  
      printf("Reading %s\n",argv[i]);
      read_png_file(argv[1]);
      printf("Image is %dx%d\n",width,height);
      if (width!=64||height!=64) {
	fprintf(stderr,"ERROR: Texture images must all be 64x64 pixels.\n");
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
      for(int xx=0;xx<64;xx++) {
	for(int yy=0;yy<64;yy++) {
	  int red=row_pointers[yy][xx*multiplier+0];
	  int green=row_pointers[yy][xx*multiplier+1];
	  int blue=row_pointers[yy][xx*multiplier+2];
	  int pixel_value=palette_lookup(red,green,blue);
	  printf("%02x",pixel_value&0xff);
	}
	printf("\n");
      }
      
      
  }

  return 0;
}

/* ============================================================= */
