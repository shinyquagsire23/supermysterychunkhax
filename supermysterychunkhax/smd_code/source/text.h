#ifndef TEXT_H
#define TEXT_H

void drawCharacter(u8* fb, char c, u16 x, u16 y, u32 color);
void drawString(u8* fb, char* str, u16 x, u16 y);
void drawStringColor(u8* fb, char* str, u16 x, u16 y, u32 color);

void centerString(u8* fb, char* str, u16 y, u16 screen_x);
void clearScreen();
void draw_border(u8* fb, u16 screen_x, u16 screen_y, u16 screen_w, u16 screen_h, u8 border_width, u32 color);

void hex2str(char* out, u32 val);

#endif
