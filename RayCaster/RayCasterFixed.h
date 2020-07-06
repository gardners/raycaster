#include "RayCaster.h"

void Start(uint16_t playerX, uint16_t playerY, int16_t playerDirection);
void Trace(uint16_t screenX, uint8_t* screenY, uint8_t* textureNo, uint8_t* textureX, uint16_t* textureY, uint16_t* textureStep,
	   uint8_t *texture_num);

extern uint16_t _playerX;
extern uint16_t _playerY;
extern int16_t  _playerA;
extern uint8_t  _viewQuarter;
extern uint8_t  _viewAngle;

void CalculateDistance(uint16_t rayX, uint16_t rayY, uint16_t rayA, int16_t* deltaX, int16_t* deltaY, uint8_t* textureNo, uint8_t* textureX,uint8_t *texture_num);
void LookupHeight(uint16_t distance, uint8_t* height, uint16_t* step);
char     IsWall(uint8_t tileX, uint8_t tileY);
static int16_t  MulTan(uint8_t value, char inverse, uint8_t quarter, uint8_t angle, const uint16_t* lookupTable);
static int16_t  AbsTan(uint8_t quarter, uint8_t angle, const uint16_t* lookupTable);
static uint16_t MulU(uint8_t v, uint16_t f);
static int16_t  MulS(uint8_t v, int16_t f);

void plot_pixel(unsigned long x,unsigned char y,unsigned char colour);
