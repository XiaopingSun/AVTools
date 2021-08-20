//
//  CPrint.c
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/8/19.
//

#include "CPrint.h"
#include <stdarg.h>

/**
 * 带颜色输出  默认高亮
 * @param color_ft         字体颜色
 * @param color_bg        背景颜色
 * @param input             输入字符
 */
void color_print(int color_ft, int color_bg, const char *input) {

    if (color_ft && color_bg) {
        printf("\033[1;%d;%dm%s\033[0m", color_ft, color_bg, input);
    } else if (color_ft) {
        printf("\033[1;%dm%s\033[0m", color_ft, input);
    } else if (color_bg) {
        printf("\033[1;%dm%s\033[0m", color_bg, input);
    } else {
        printf("%s", input);
    }
}
