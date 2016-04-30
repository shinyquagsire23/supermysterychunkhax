#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <3ds.h>
#include "text.h"
#include "stocks.h"

#include "imports.h"

extern const u8 msx_font[];

void drawCharacter(u8* fb, char c, u16 x, u16 y, u32 color)
{
    u8 *font = (u8*)(msx_font + c * 8);
    int i, j;
    for (i = 7; i >= 0; --i)
    {
        for (j = 0; j < 8; ++j)
        {
            u8 *pixel = (u8*)(fb+(((y+i) - 1) * 3 + (x+j) * 3 * 240));

            pixel[0]=pixel[1]=pixel[2]=0x0;
            if ((*font & (128 >> j)))
            {
                pixel[0]=color&0xFF;
                pixel[1]=(color&0xFF00)>>8;
                pixel[2]=(color&0xFF0000)>>16;
            }
        }
        ++font;
    }
}

void drawString(u8* fb, char* str, u16 x, u16 y)
{
    drawStringColor(fb,str,x,y,0xFFFFFF);
}

void drawStringColor(u8* fb, char* str, u16 x, u16 y, u32 color)
{
    if(!str)return;
    y=232-y;
    int k;
    int dx=0, dy=0;
    for(k=0;k<_strlen(str);k++)
    {
        if(str[k]>=32 && str[k]<128)drawCharacter(fb,str[k],x+dx,y+dy,color);
        dx+=8;
        if(str[k]=='\n'){dx=0;dy-=8;}
    }
}

void centerString(u8* fb, char* str, u16 y, u16 screen_x)
{
    drawString(fb, str, (screen_x-(_strlen(str)*8))/2, y);
}

void clearScreen()
{
    memset((u32*)&LINEAR_BUFFER[0x00100000], 0, (400*240*3)+(320*240*3));
    _GSPGPU_FlushDataCache(gspHandle, 0xFFFF8001, (u32*)&LINEAR_BUFFER[0x00100000], (400*240*3)+(320*240*3));
    draw_border((u8*)&LINEAR_BUFFER[0x00100000], 10, 10, 375, 215, 3, 0x00a0ff);
}

void draw_border(u8* fb, u16 screen_x, u16 screen_y, u16 screen_w, u16 screen_h, u8 border_width, u32 color)
{
    int x = 0, y = 0;

    for(y = screen_y; y < screen_h + screen_y + border_width; y++)
    {
        for(x = screen_x; x < screen_w + screen_x + border_width; x++)
        {
            u8 *pixel = (u8*)(fb+((240 - y) * 3) + (x * 3 * 240));

            pixel[0]=pixel[1]=pixel[2]=0x0;
            if((x >= screen_x && x <= screen_x + border_width) ||
               (y >= screen_y && y <= screen_y + border_width) ||
               (x >= screen_w + screen_x - 1 && x <= screen_w + screen_x - 1 + border_width) ||
               (y >= screen_h + screen_y - 1 && y <= screen_h + screen_y - 1 + border_width))
                {
                    pixel[0]=color&0xFF;
                    pixel[1]=(color&0xFF00)>>8;
                    pixel[2]=(color&0xFF0000)>>16;
                }
        }
    }
}

const u8 hexTable[]=
{
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

void hex2str(char* out, u32 val)
{
    int i;
    for(i=0;i<8;i++){out[7-i]=hexTable[val&0xf];val>>=4;}
    out[8]=0x00;
}
