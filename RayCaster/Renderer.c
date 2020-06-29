#define _USE_MATH_DEFINES

#include <math.h>
#include "Renderer.h"
#include "RayCasterData.h"

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
