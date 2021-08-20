//
//  CPrint.h
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/8/19.
//

#ifndef CPrint_h
#define CPrint_h

#include <stdio.h>

#define COLOR_FT_NONE 0
#define COLOR_BG_NONE 0

//字颜色 : 30 - 37
#define COLOR_FT_BLACK 30
#define COLOR_FT_RED 31
#define COLOR_FT_GREED 32
#define COLOR_FT_YELLOW 33
#define COLOR_FT_BLUE 34
#define COLOR_FT_PURPLE 35
#define COLOR_FT_DARK_GREEN 36
#define COLOR_FT_WHITE 37

//字背景颜色范围 : 40 - 47
#define COLOR_BG_BLACK 40      //黑
#define COLOR_BG_RED 41      //深红
#define COLOR_BG_GREED 42      //绿
#define COLOR_BG_YELLOW 43      //黄色
#define COLOR_BG_BLUE 44          //蓝色
#define COLOR_BG_PURPLE 45      //紫色
#define COLOR_BG_DARK_GREEN 46 //深绿
#define COLOR_BG_WHITE 47      //白色

void color_print(int color_ft, int color_bg, const char *input);

#endif /* CPrint_h */
