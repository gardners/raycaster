#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <hal.h>
#include <memory.h>
#include <dirent.h>
#include <fileio.h>

#define TABLES_320
#define SCREEN_WIDTH (uint16_t)320
#define SCREEN_HEIGHT (uint16_t)256
#define SCREEN_SCALE 2
//#define INV_FACTOR (float)(SCREEN_WIDTH * 95.0f / 320.0f)
#define LOOKUP_TBL
#define LOOKUP8(tbl, offset) tbl[offset]
#define LOOKUP16(tbl, offset) tbl[offset]

#define MAP_X (uint8_t)32
#define MAP_XS (uint8_t)5
#define MAP_Y (uint8_t)32
#define INV_FACTOR_INT ((uint16_t)(SCREEN_WIDTH * 75))
// #define MIN_DIST (int)((150 * ((float)SCREEN_WIDTH / (float)SCREEN_HEIGHT)))
#define MIN_DIST (int)(188)
#define HORIZON_HEIGHT (SCREEN_HEIGHT / 2)
#define INVERT(x) (uint8_t)((x ^ 255) + 1)
#define ABS(x) (x < 0 ? -x : x)

