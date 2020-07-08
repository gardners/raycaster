// fixed-point implementation

#include "RayCasterFixed.h"

#define LOOKUP_STORAGE extern
#include "RayCasterTables.h"

#define TEXTURE_ADDRESS 0x8000300

#include "debug.h"

uint16_t maze_get_cell(uint8_t x,uint8_t y);

#define true 1
#define false 0

uint16_t _playerX;
uint16_t _playerY;
int16_t  _playerA;
uint8_t  _viewQuarter;
uint8_t  _viewAngle;

uint32_t texture_offset=0;
uint8_t texture_num=0;
uint16_t texture_count=0;

extern char diag_mode;

void print_text80(unsigned char x,unsigned char y,unsigned char colour,char *msg);


void setup_multiplier(void)
{
  POKE(0xD771,0);
  POKE(0xD772,0);
  POKE(0xD773,0);
  POKE(0xD776,0);
  POKE(0xD777,0);
}

// (v * f) >> 8
uint16_t MulU(uint8_t v, uint16_t f)
{
	POKE(0xD770,v);
	POKE(0xD774,f & 0xff);
	POKE(0xD775,f>>8);
        return PEEK(0xD779)+(PEEK(0xD77A)<<8);
}

int16_t MulS(uint8_t v, int16_t f)
{
    const uint16_t uf = MulU(v, ABS(f));
    if(f < 0)
    {
        return ~uf;
    }
    return uf;
}

uint16_t MulCos(uint16_t angle,uint16_t magnitude)
{
  switch(angle) {
  case 0: return magnitude;
  case 0x100: return 0;
  case 0x200: return -magnitude;
  case 0x300: return 0;
  }
  
  switch(angle>>8)
    {
    case 0:	  
      return MulU(LOOKUP8(g_cos, (angle&0xff)), magnitude);
    case 1:
      return -MulU(LOOKUP8(g_sin, (angle&0xff)), magnitude);
    case 2:
      return -MulU(LOOKUP8(g_cos, (angle&0xff)), magnitude);
    case 3:
      return MulU(LOOKUP8(g_sin, (angle&0xff)), magnitude);
      break;
    }

  return 0;
}

uint16_t MulSin(uint16_t angle,uint16_t magnitude)
{
  switch(angle) {
  case 0: return 0;
  case 0x100: return magnitude;
  case 0x200: return 0;
  case 0x300: return -magnitude;
  }

  switch(angle>>8)
    {
    case 0:	  
      return MulU(LOOKUP8(g_sin, (angle&0xff)), magnitude);
    case 1:
      return MulU(LOOKUP8(g_cos, (angle&0xff)), magnitude);
    case 2:
      return -MulU(LOOKUP8(g_sin, (angle&0xff)), magnitude);
    case 3:
      return -MulU(LOOKUP8(g_cos, (angle&0xff)), magnitude);
      break;
    }

  return 0;
}

int16_t MulTan(uint8_t value, char inverse, uint8_t quarter, uint8_t angle, const uint16_t* lookupTable)
{
    uint8_t signedValue = value;
    if(inverse)
    {
        if(value == 0)
        {
            if(quarter & 1)
            {
                return -AbsTan(quarter, angle, lookupTable);
            }
            return AbsTan(quarter, angle, lookupTable);
        }
        signedValue = INVERT(value);
    }
    if(signedValue == 0)
    {
        return 0;
    }
    if(quarter & 1)
    {
        return -MulU(signedValue, LOOKUP16(lookupTable, INVERT(angle)));
    }
    return MulU(signedValue, LOOKUP16(lookupTable, angle));
}

int16_t AbsTan(uint8_t quarter, uint8_t angle, const uint16_t* lookupTable)
{
    if(quarter & 1)
    {
      return LOOKUP16(lookupTable, INVERT(angle));
    }
    return LOOKUP16(lookupTable, angle);
}

char IsWall(uint8_t tileX, uint8_t tileY)
{
  if (!tileX) return true;
  if (!tileY) return true;  
  
  // MSB indicates if square is passable
  if (maze_get_cell(tileX,tileY)&0x8000) return true;
  else return false;

#if 0
    if(tileX > MAP_X - 1 || tileY > MAP_Y - 1)
    {
        return true;
    }
    return LOOKUP8(g_map, (tileX >> 3) + (tileY << (MAP_XS - 3))) & (1 << (8 - (tileX & 0x7)));
#endif
}

void LookupHeight(uint16_t distance, uint8_t* height, uint16_t* step)
{
    if(distance >= 256)
    {
        const uint16_t ds = distance >> 3;
        if(ds >= 256)
        {
            *height = LOOKUP8(g_farHeight, 255) - 1;
            *step   = LOOKUP16(g_farStep, 255);
        }
        *height = LOOKUP8(g_farHeight, ds);
        *step   = LOOKUP16(g_farStep, ds);
    }
    else
    {
        *height = LOOKUP8(g_nearHeight, distance);
        *step   = LOOKUP16(g_nearStep, distance);
    }
}

int16_t stepX;
int16_t stepY;


int8_t  tileStepX;
int8_t  tileStepY;
int16_t interceptX;
int16_t interceptY;

uint8_t quarter;
uint8_t angle;
uint8_t offsetX;
uint8_t offsetY;

// tileX/Y and interceptX/Y both have to be signed for the ray casting comparisons
// to work correctly
int8_t tileX;
int8_t tileY;
int16_t hitX;
int16_t hitY;

void CalculateDistance(
		       uint16_t rayX, uint16_t rayY, uint16_t rayA, int16_t* deltaX, int16_t* deltaY, uint8_t* textureNo, uint8_t* textureX,
		       uint8_t *texture_num)
{
  interceptX = rayX;
  interceptY = rayY;

  quarter = rayA >> 8;
  angle   = rayA & 0xff;
  offsetX = rayX & 0xff;
  offsetY = rayY & 0xff;

    // tileX/Y and interceptX/Y both have to be signed for the ray casting comparisons
    // to work correctly
  tileX = rayX >> 8;
  tileY = rayY >> 8;
    
    if(angle == 0)
    {
        switch(quarter & 1)
        {
        case 0:
            tileStepX = 0;
            tileStepY = quarter == 0 ? 1 : -1;
            if(tileStepY == 1)
            {
                interceptY -= 256;
            }
            for(;;)
            {
                tileY += tileStepY;
                if(IsWall(tileX, tileY))
                {
		  *texture_num=(maze_get_cell(tileX,tileY)&0xff);
		  goto HorizontalHit;
                }
            }
            break;
        case 1:
            tileStepY = 0;
            tileStepX = quarter == 1 ? 1 : -1;
            if(tileStepX == 1)
            {
                interceptX -= 256;
            }
            for(;;)
            {
                tileX += tileStepX;
                if(IsWall(tileX, tileY))
                {
		  *texture_num=maze_get_cell(tileX,tileY);
		  goto VerticalHit;
                }
            }
            break;
        }
    }
    else
    {
        switch(quarter)
        {
        case 0:
        case 1:
            tileStepX = 1;
            interceptY += MulTan(offsetX, true, quarter, angle, g_cotan);
            interceptX -= 256;
            stepX = AbsTan(quarter, angle, g_tan);
            break;
        case 2:
        case 3:
            tileStepX = -1;
	    interceptY -= MulTan(offsetX, false, quarter, angle, g_cotan);
            stepX = -AbsTan(quarter, angle, g_tan);
            break;
        }

        switch(quarter)
        {
        case 0:
        case 3:
            tileStepY = 1;
            interceptX += MulTan(offsetY, true, quarter, angle, g_tan);
            interceptY -= 256;
            stepY = AbsTan(quarter, angle, g_cotan);
            break;
        case 1:
        case 2:
            tileStepY = -1;
            interceptX -= MulTan(offsetY, false, quarter, angle, g_tan);
            stepY = -AbsTan(quarter, angle, g_cotan);
            break;
        }

        for(;;)
        {
	  while((tileStepY == 1 && ((interceptY >> 8) < tileY)) || (tileStepY == -1 && ((interceptY >> 8) >= tileY)))
            {
                tileX += tileStepX;
                if(IsWall(tileX, tileY))
                {
		  *texture_num=(maze_get_cell(tileX,tileY)&0xff);
		  goto VerticalHit;
                }
                interceptY += stepY;
            }
            while((tileStepX == 1 && (interceptX >> 8 < tileX)) || (tileStepX == -1 && (interceptX >> 8 >= tileX)))
            {
                tileY += tileStepY;
                if(IsWall(tileX, tileY))
                {
		  *texture_num=(maze_get_cell(tileX,tileY)&0xff);
		  goto HorizontalHit;
                }
                interceptX += stepX;
            }
        }
    }

HorizontalHit:
    hitX       = interceptX + (tileStepX == 1 ? 256 : 0);
    hitY       = (tileY << 8) + (tileStepY == -1 ? 256 : 0);
    *textureNo = 0;
    *textureX  = interceptX & 0xFF;
    goto WallHit;

VerticalHit:
    hitX       = (tileX << 8) + (tileStepX == -1 ? 256 : 0);
    hitY       = interceptY + (tileStepY == 1 ? 256 : 0);
    *textureNo = 1;
    *textureX  = interceptY & 0xFF;
    goto WallHit;

WallHit:
    *deltaX = hitX - rayX;
    *deltaY = hitY - rayY;

}

// (playerX, playerY) is 8 box coordinate bits, 8 inside coordinate bits
// (playerA) is full circle as 1024
int32_t distance;
int16_t deltaX;
int16_t deltaY;
uint16_t rayAngle;
void Trace(
	   uint16_t screenX, uint8_t* screenY, uint8_t* textureNo, uint8_t* textureX, uint16_t* textureY, uint16_t* textureStep,
	   uint8_t* texture_num)
{

    rayAngle = (_playerA + LOOKUP16(g_deltaAngle, screenX));

    distance=0;
    
    // neutralize artefacts around edges
    if (0)
    switch(rayAngle & 0xff)
    {
    case 1:
    case 254:
        rayAngle--;
        break;
    case 2:
    case 255:
        rayAngle++;
        break;
    }
    rayAngle &= 0x3ff;

    CalculateDistance(_playerX, _playerY, rayAngle, &deltaX, &deltaY, textureNo, textureX,texture_num);

    // distance = deltaY * cos(playerA) + deltaX * sin(playerA)
    if(_playerA == 0)
    {
        distance += deltaY;
    }
    else if(_playerA == 512)
    {
        distance -= deltaY;
    }
    else
        switch(_viewQuarter)
        {
        case 0:	  
            distance += MulS(LOOKUP8(g_cos, _viewAngle), deltaY);
            break;
        case 1:
            distance -= MulS(LOOKUP8(g_sin, _viewAngle), deltaY);
            break;
        case 2:
            distance -= MulS(LOOKUP8(g_cos, _viewAngle), deltaY);
            break;
        case 3:
            distance += MulS(LOOKUP8(g_sin, _viewAngle), deltaY);
            break;
        }

    if(_playerA == 256)
    {
        distance += deltaX;
    }
    else if(_playerA == 768)
    {
        distance -= deltaX;
    }
    else
        switch(_viewQuarter)
        {
        case 0:
            distance += MulS(LOOKUP8(g_sin, _viewAngle), deltaX);
            break;
        case 1:
            distance += MulS(LOOKUP8(g_cos, _viewAngle), deltaX);
            break;
        case 2:
            distance -= MulS(LOOKUP8(g_sin, _viewAngle), deltaX);
            break;
        case 3:
            distance -= MulS(LOOKUP8(g_cos, _viewAngle), deltaX);
            break;
        }
    if (distance>8000) distance=8000;
    if (distance<0) distance=8000;
    // XXX - Use hardware division unit to remove this lookup table and the
    // distortions it causes when you get too close to a wall
    if(distance >= MIN_DIST)
    {
        *textureY = 0;
        LookupHeight((distance - MIN_DIST) >> 2, screenY, textureStep);
    }
    else
    {
        *screenY     = SCREEN_HEIGHT >> 1;
        *textureY    = LOOKUP16(g_overflowOffset, distance);
        *textureStep = LOOKUP16(g_overflowStep, distance);
    }

#if 0
    if (diag_mode&&(screenX<(25*8))) {
	snprintf(msg,80,"(%d,%d)%ld          ",
		 _playerX>>8,_playerY>>8,distance);
	msg[10]=0;
	print_text80((screenX&7)*10,(screenX>>3),15,msg);
      }     
#endif
}

void Start(uint16_t playerX, uint16_t playerY, int16_t playerA)
{
    _viewQuarter = playerA >> 8;
    _viewAngle   = playerA & 0xff;
    _playerX     = playerX;
    _playerY     = playerY;
    _playerA     = playerA;

    // And clear multiplier state
    POKE(0xD770,0);
    POKE(0xD771,0);
    POKE(0xD772,0);
    POKE(0xD773,0);
    POKE(0xD774,0);
    POKE(0xD775,0);
    POKE(0xD776,0);
    POKE(0xD777,0);

}

uint8_t   sso;
uint8_t   tc;
uint8_t   tn;
uint16_t  tso;
uint16_t  tst,to,ts;
uint32_t* lb;
int16_t tx,ws,ty,tv;
int x,y;

uint8_t dlist[29]={
  0x0a, // F018A style job
  0x80,0x00,0x81,0x00, // src and dst both in first 1MB
  0x83,0x01,0x82,0x00, // src skip
  0x85,0x01,0x84,0x00, // dst skip
  0x86,0x00, // No transparency
  0x87,0x00, // Set transparent colour
  0x00, // No more jobs

  0x00, // Copy, not chained
  0x00,0x00, // size
  0x00,0x00, // src
  0x00, // src bank
  0x00,0x00, // dst
  0x00, // dst bank
  0x00,0x00 // modulo (unused)
  
};

char m[160];

void dma_stepped_copy(uint32_t src, uint32_t dst,uint16_t count,
		      uint8_t step_src_whole,uint8_t step_src_fraction,
		      uint8_t step_dst_whole,uint8_t step_dst_fraction)
{
  if (!count) return;
  dlist[6]=step_src_whole;
  dlist[8]=step_src_fraction;
  dlist[6]=step_src_whole;
  dlist[8]=step_src_fraction;
  dlist[10]=step_dst_whole;
  dlist[12]=step_dst_fraction;
  dlist[14]=0; // no transparency
  dlist[19]=count&0xff;
  dlist[20]=count>>8;
  dlist[21]=src&0xff;
  dlist[22]=src>>8;
  dlist[23]=(src>>16)&0xf;
  dlist[2]=src>>20;
  dlist[24]=dst&0xff;
  dlist[25]=dst>>8;
  dlist[26]=(dst>>16L)&0xf;

  POKE(0xD702,0x00);
  POKE(0xD701,(uint16_t)(&dlist)>>8);
  POKE(0xD705,(uint16_t)(&dlist)&0xff);
}


unsigned long x_offset;
uint16_t sso2;
unsigned char texture_y_offset;

void TraceFrameFast(uint16_t playerX, uint16_t playerY, uint16_t playerDirection)
{
    Start(playerX, playerY, playerDirection);


    x_offset=0x40000;
    
    for(x = 0; x < SCREEN_WIDTH; x++)
    {

      if (x) {
	if (x&7) x_offset++;
	else x_offset+=64*25-7;
      }

      //      POKE(0xD020,0x80);
      Trace(x, &sso, &tn, &tc, &tso, &tst,&texture_num);

      if (texture_num>=texture_count) texture_num=10;
      texture_offset=((uint32_t)texture_num)<<12L;
      
	//	if (sso>2*HORIZON_HEIGHT) sso=2*HORIZON_HEIGHT;

	if (sso>HORIZON_HEIGHT) {
	  // XXX - Use hardware division unit to speed it up
	  texture_y_offset=32-32*HORIZON_HEIGHT/sso;
	  sso=HORIZON_HEIGHT;
	} else
	  texture_y_offset=0;
	
        tx = (tc >> 2);
        ws = HORIZON_HEIGHT - sso;
        if(ws < 0)
        {
            ws  = 0;
            sso = HORIZON_HEIGHT;
        }
        to = tso;
        ts = tst;

	sso2=sso<<1;
	
	//      POKE(0xD020,0x00);
      
	// Plot upper horizon part of the image
	// Use a DMA job with stepped destination
	// For the sky, we have a stretched cloud image that we rotate around
	// to match the view angle. It looks nice and helps the player know where they are
	// pointed
	dma_stepped_copy(TEXTURE_ADDRESS
			 +(320*(rayAngle^0x200)/1024)*64
			 ,x_offset,
		       ws,
		       0x00,0xa0,
		       0x08,0x00);

      //      POKE(0xD020,0x00);
	
	// Use DMA to copy texture.
      // XXX Textures are sideways in RAM compared with how we can get the DMA to step through them
      // A truly optimised version would fix this.
	dma_stepped_copy(TEXTURE_ADDRESS+texture_offset+(tx<<6)+texture_y_offset,x_offset+(ws<<3),
			 sso2,
			 0+(ts>>10),ts>>2,
			 0x08,0x00);

	//      POKE(0xD020,0x00);
	
	// Use DMA job with stepped destination to draw floor
	dma_stepped_copy(TEXTURE_ADDRESS+(5*4096)
			 // Make the texture follow along a bit
			 +64-(((playerX+playerY)>>4)&0x3f)
			 // And rotate around with you
			 +((rayAngle>>1)&0x7f)*64
			 +(ws+sso*2),x_offset+((ws+sso2)<<3),
			 ws,
			 0x00,0xa0,
			 0x08,0x00);
	
	//      POKE(0xD020,0x00);

    }
}

