// fixed-point implementation

#include "RayCasterFixed.h"
#include "RayCasterData.h"

#define LOOKUP_STORAGE extern
#include "RayCasterTables.h"

#define true 1
#define false 0

uint16_t _playerX;
uint16_t _playerY;
int16_t  _playerA;
uint8_t  _viewQuarter;
uint8_t  _viewAngle;


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
        return PEEK(0xD778)+(PEEK(0xD779)<<8);
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
    if(tileX > MAP_X - 1 || tileY > MAP_Y - 1)
    {
        return true;
    }
    return LOOKUP8(g_map, (tileX >> 3) + (tileY << (MAP_XS - 3))) & (1 << (8 - (tileX & 0x7)));
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

void CalculateDistance(
    uint16_t rayX, uint16_t rayY, uint16_t rayA, int16_t* deltaX, int16_t* deltaY, uint8_t* textureNo, uint8_t* textureX)
{
    register int8_t  tileStepX;
    register int8_t  tileStepY;
    register int16_t interceptX = rayX;
    register int16_t interceptY = rayY;

    const uint8_t quarter = rayA >> 8;
    const uint8_t angle   = rayA & 0xff;
    const uint8_t offsetX = rayX & 0xff;
    const uint8_t offsetY = rayY & 0xff;

    uint8_t tileX = rayX >> 8;
    uint8_t tileY = rayY >> 8;
    int16_t hitX;
    int16_t hitY;

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
                    goto VerticalHit;
                }
            }
            break;
        }
    }
    else
    {
        int16_t stepX;
        int16_t stepY;

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
            while((tileStepY == 1 && (interceptY >> 8 < tileY)) || (tileStepY == -1 && (interceptY >> 8 >= tileY)))
            {
                tileX += tileStepX;
                if(IsWall(tileX, tileY))
                {
                    goto VerticalHit;
                }
                interceptY += stepY;
            }
            while((tileStepX == 1 && (interceptX >> 8 < tileX)) || (tileStepX == -1 && (interceptX >> 8 >= tileX)))
            {
                tileY += tileStepY;
                if(IsWall(tileX, tileY))
                {
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
void Trace(
    uint16_t screenX, uint8_t* screenY, uint8_t* textureNo, uint8_t* textureX, uint16_t* textureY, uint16_t* textureStep)
{
    int16_t distance = 0;
    int16_t deltaX;
    int16_t deltaY;
    uint16_t rayAngle = (_playerA + LOOKUP16(g_deltaAngle, screenX));

    // neutralize artefacts around edges
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

    CalculateDistance(_playerX, _playerY, rayAngle, &deltaX, &deltaY, textureNo, textureX);

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
            distance -= MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), deltaY);
            break;
        case 2:
            distance -= MulS(LOOKUP8(g_cos, _viewAngle), deltaY);
            break;
        case 3:
            distance += MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), deltaY);
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
            distance += MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), deltaX);
            break;
        case 2:
            distance -= MulS(LOOKUP8(g_sin, _viewAngle), deltaX);
            break;
        case 3:
            distance -= MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), deltaX);
            break;
        }
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
}

void Start(uint16_t playerX, uint16_t playerY, int16_t playerA)
{
    _viewQuarter = playerA >> 8;
    _viewAngle   = playerA & 0xff;
    _playerX     = playerX;
    _playerY     = playerY;
    _playerA     = playerA;
}

uint8_t   sso;
uint8_t   tc;
uint8_t   tn;
uint16_t  tso;
uint16_t  tst,to,ts;
uint32_t* lb;
int16_t tx,ws,ty,tv;
int x,y;

void TraceFrame(unsigned char playerX, unsigned char playerY, uint16_t playerDirection)
{
    Start(playerX<<8, playerY<<8, playerDirection);

    for(x = 0; x < SCREEN_WIDTH; x++)
    {

        Trace(x, &sso, &tn, &tc, &tso, &tst);

        tx = (tc >> 2);
        ws = HORIZON_HEIGHT - sso;
        if(ws < 0)
        {
            ws  = 0;
            sso = HORIZON_HEIGHT;
        }
        to = tso;
        ts = tst;

        for(y = 0; y < ws; y++)
        {
            plot_pixel(x,y,96 + (HORIZON_HEIGHT - y));
        }

        for(y = 0; y < sso * 2; y++)
        {
            // paint texture pixel
            ty = (to >> 10);
            tv = g_texture8[(ty << 6) + tx];

            to += ts;

            if(tn == 1 && tv > 0)
            {
                // dark wall
                tv >>= 1;
            }
            plot_pixel(x,y+ws,tv);
        }

        for(y = 0; y < ws; y++)
        {
            plot_pixel(x,y+ws+sso*2,96 + (HORIZON_HEIGHT - (ws - y)));
        }
    }
}

uint8_t sky_texture[0x100];

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

void dma_stepped_copy(long src, long dst,uint16_t count,
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
  dlist[23]=src>>16;
  dlist[24]=dst&0xff;
  dlist[25]=dst>>8;
  dlist[26]=dst>>16;

  POKE(0xD702,0x00);
  POKE(0xD701,(uint16_t)(&dlist)>>8);
  POKE(0xD705,(uint16_t)(&dlist)&0xff);
}

void setup_sky(void)
{
  // Calculate sky texture
  for(y = 0; y < HORIZON_HEIGHT; y++)
    {
      sky_texture[y]=96+(HORIZON_HEIGHT - y);
      sky_texture[HORIZON_HEIGHT+y]=96+(y-HORIZON_HEIGHT);
    }
    
}


unsigned long x_offset;
uint16_t sso2;

void TraceFrameFast(unsigned char playerX, unsigned char playerY, uint16_t playerDirection)
{
    Start(playerX<<8, playerY<<8, playerDirection);


    x_offset=0x40000;
    
    for(x = 0; x < SCREEN_WIDTH; x++)
    {

      if (x) {
	if (x&7) x_offset++;
	else x_offset+=64*25-7;
      }
      
      POKE(0xD020,0x80);
        Trace(x, &sso, &tn, &tc, &tso, &tst);

	if (sso>2*HORIZON_HEIGHT) sso=2*HORIZON_HEIGHT;
	
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
	
      POKE(0xD020,0x00);
      
	// Plot upper horizon part of the image
	// Use a DMA job with stepped destination
      dma_stepped_copy(sky_texture,x_offset,
		       ws,
		       0x01,0x00,
		       0x08,0x00);

      POKE(0xD020,0x00);
	
	// Use DMA to copy texture.
      // XXX Textures are sideways in RAM compared with how we can get the DMA to step through them
      // A truly optimised version would fix this.
	dma_stepped_copy(g_texture8+(tx<<6),x_offset+(ws<<3),
			 sso2,
			 0+(ts>>10),ts>>2,
			 0x08,0x00);

      POKE(0xD020,0x00);
	
	// Use DMA job with stepped destination to draw floor
	dma_stepped_copy(sky_texture+(ws+sso*2),x_offset+((ws+sso2)<<3),
			 ws,
			 0x01,0x00,
			 0x08,0x00);
	
      POKE(0xD020,0x00);

    }
}

