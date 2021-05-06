//
//  YUVToRGB.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/5/3.
//

#include "YUVToRGB.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

static void convert(char *url, int pixel_width, int pixel_height);
static void yuv420p_to_rgb24(unsigned char *yuv_buf, unsigned char *rgb_buf, int pixel_width, int pixel_height);

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - Source Format: YUV420P \n  - Target Format: RGB24\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -w:   Width\n");
    printf("  -h:   Height\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools YUVToRGB -w 720 -h 1280 -i yu12.yuv\n\n");
    printf("Get YUV420P With FFMpeg From A Mp4 File:\n\n");
    printf("  ffmpeg -i out.mp4 -an -c:v rawvideo -pix_fmt yuv420p yu12.yuv\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void yuv_to_rgb_parse_cmd(int argc, char *argv[]) {
    int option = 0;   // getopt_long的返回值，返回匹配到字符的ascii码，没有匹配到可读参数时返回-1
    int width = 0;    // 分辨率宽
    int height = 0;    // 分辨率高
    char *url = NULL;   // 输入文件路径
    
    while (EOF != (option = getopt_long(argc, argv, "i:w:h:", tool_long_options, NULL))) {
        switch (option) {
            case '`':
                show_module_help();
                return;
                break;
            case 'i':
                url = optarg;
                break;
            case 'w':
                width = atoi(optarg);
                break;
            case 'h':
                height= atoi(optarg);
                break;
            case '?':
                printf("Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
                return;
                break;
            default:
                break;
        }
    }
    
    if (NULL == url || 0 == width || 0 == height) {
        printf("YUVToRGB: Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    convert(url, width, height);
}

/**
 * Start Convert
 * @param url     YUV File Local Path
 * @param pixel_width    Pixel Width Of RGB
 * @param pixel_height   Pixel Height Of RGB
 */
static void convert(char *url, int pixel_width, int pixel_height) {
    
    FILE *f_yuv = NULL;
    FILE *f_rgb = NULL;
    char output_url[100];
    long total_size = 0;
    long frame_count = 0;
    
    unsigned char *pic_yuv = NULL;
    unsigned char *pic_rgb = NULL;
    
    // 输出路径
    sprintf(output_url, "%s.rgb", url);
    
    // 打开rgb文件
    f_yuv = fopen(url, "rb");
    if (!f_yuv) {
        printf("YUVToRGB: Could Not Open YUV File.\n");
        goto __FAIL;
    }
    
    // 获取yuv文件大小
    fseek(f_yuv, 0, SEEK_END);
    total_size = ftell(f_yuv);
    fseek(f_yuv, 0, SEEK_SET);
    
    // 计算视频帧数量
    frame_count = total_size / (pixel_width * pixel_height * 3 / 2);
    
    // 打开rgb输出路径
    f_rgb = fopen(output_url, "wb+");
    if (!f_rgb) {
        printf("YUVToRGB: Could Not Open Output Path.\n");
        goto __FAIL;
    }
    
    // 初始化单帧图片的堆内存
    pic_yuv = (unsigned char *)malloc(pixel_width * pixel_height * 3 / 2);
    pic_rgb = (unsigned char *)malloc(pixel_width * pixel_height * 3);
    
    for (int i = 0; i < frame_count; i++) {
        fread(pic_yuv, 1, pixel_width * pixel_height * 3 / 2, f_yuv);
        yuv420p_to_rgb24(pic_yuv, pic_rgb, pixel_width, pixel_height);
        printf("YUVToRGB: Writing Data...  Frame Index %d\n", i);
        fwrite(pic_rgb, 1, pixel_width * pixel_height * 3, f_rgb);
    }
    
    printf("Finish!\n");
        
__FAIL:
    if (pic_yuv) {
        free(pic_yuv);
    }
    if (pic_rgb) {
        free(pic_rgb);
    }
    if (f_yuv) {
        fclose(f_yuv);
    }
    if (f_rgb) {
        fclose(f_rgb);
    }
}

/**
 * Convert
 * @param rgb_buf       rgb数据字符指针
 * @param yuv_buf       yuv数据字符指针
 * @param pixel_width     分辨率宽
 * @param pixel_height    分辨率高
 */
static void yuv420p_to_rgb24(unsigned char *yuv_buf, unsigned char *rgb_buf, int pixel_width, int pixel_height) {
    
    // 定义y、u、v分量指针  和rgb像素点的滑动指针
    unsigned char *y_ptr, *u_ptr, *v_ptr, *rgb_ptr;
    
    // 初始化rgb_buf
    memset(rgb_buf, 0, pixel_width * pixel_height * 3);
    
    // y、u、v三个分量指针分别指向yuv_buf中三个分量的位置
    y_ptr = yuv_buf;
    u_ptr = yuv_buf + pixel_width * pixel_height;
    v_ptr = u_ptr + pixel_width * pixel_height / 4;
    
    // rgb指针指向rgb_buf开始
    rgb_ptr = rgb_buf;
    
    // 定义y、r、g、b分量的无符号字符常量(0, 255)   uv分量计算时使用有符号(-128, 127)
    // 这里使用的unsigned char和char类型的区别是  如果char的二进制高位（char的符号位）是1   在与整型数做计算时   会对char按1做扩展
    unsigned char y, r, g, b;
    char u, v;
    for (int j = 0; j < pixel_height; j++) {
        for (int i = 0; i < pixel_width; i++) {
            // 4个相邻的正方形像素点为一个item单位  计算该像素点应该使用的uv偏移量
            int item_j = j / 2;
            int item_i = i / 2;
            int uv_offset = pixel_width / 2 * item_j + item_i;
                                    
            // 取出该点的y、u、v分量值
            y = (unsigned char)*(y_ptr++);
            u = (char)((unsigned char)*(u_ptr + uv_offset) - 128);
            v = (char)((unsigned char)*(v_ptr + uv_offset) - 128);
                        
//            // 计算r、g、b分量值
//            r = (unsigned char)(y + 1.371 * v);
//            g = (unsigned char)(y - 0.338 * u - 0.698 * v);
//            b = (unsigned char)(y + 1.732 * u);
            
            // 优化：减少浮点运算
            r = (unsigned char)((255 * y + 350 * v) >> 8);
            g = (unsigned char)((255* y - 86 * u - 178 * v) >> 8);
            b = (unsigned char)((255* y + 442 * u) >> 8);
            
            // 赋值给rgb指针
            *(rgb_ptr++) = r;
            *(rgb_ptr++) = g;
            *(rgb_ptr++) = b;
        }
    }
}
