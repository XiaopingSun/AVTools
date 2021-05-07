//
//  RGBToYUV.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/5/3.
//

#include "RGBToYUV.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

static void convert(char *url, int pixel_width, int pixel_height);
static void rgb24_to_yuv420p(unsigned char *rgb_buf, unsigned char *yuv_buf, int pixel_width, int pixel_height);

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - Source Format: RGB24 \n  - Target Format: YUV420P\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -w:   Width\n");
    printf("  -h:   Height\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools RGBToYUV -w 720 -h 1280 -i rgb24.rgb\n\n");
    printf("Get RGB24 With FFMpeg From A Mp4 File:\n\n");
    printf("  ffmpeg -i out.mp4 -an -c:v rawvideo -pix_fmt rgb24 rgb24.rgb\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void rgb_to_yuv_parse_cmd(int argc, char *argv[]) {
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
        printf("RGBToYUV: Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    convert(url, width, height);
}

/**
 * Start Convert
 * @param url     RGB File Local Path
 * @param pixel_width    Pixel Width Of RGB
 * @param pixel_height   Pixel Height Of RGB
 */
static void convert(char *url, int pixel_width, int pixel_height) {
    
    FILE *f_rgb = NULL;
    FILE *f_yuv = NULL;
    char output_url[100];
    long total_size = 0;
    long frame_count = 0;
    
    unsigned char *pic_rgb = NULL;
    unsigned char *pic_yuv = NULL;
    
    // 输出路径
    sprintf(output_url, "%s.yuv", url);
    
    // 打开rgb文件
    f_rgb = fopen(url, "rb");
    if (!f_rgb) {
        printf("RGBToYUV: Could Not Open RGB File.\n");
        goto __FAIL;
    }
    
    // 获取rgb文件大小
    fseek(f_rgb, 0, SEEK_END);
    total_size = ftell(f_rgb);
    fseek(f_rgb, 0, SEEK_SET);
    
    // 计算视频帧数量
    frame_count = total_size / (pixel_width * pixel_height * 3);
    
    // 打开yuv输出路径
    f_yuv = fopen(output_url, "wb+");
    if (!f_yuv) {
        printf("RGBToYUV: Could Not Open Output Path.\n");
        goto __FAIL;
    }
    
    // 初始化单帧图片的堆内存
    pic_rgb = (unsigned char *)malloc(pixel_width * pixel_height * 3);
    pic_yuv = (unsigned char *)malloc(pixel_width * pixel_height * 3 / 2);
    
    for (int i = 0; i < frame_count; i++) {
        fread(pic_rgb, 1, pixel_width * pixel_height * 3, f_rgb);
        rgb24_to_yuv420p(pic_rgb, pic_yuv, pixel_width, pixel_height);
        printf("RGBToYUV: Writing Data...  Frame Index %d\n", i);
        fwrite(pic_yuv, 1, pixel_width * pixel_height * 3 / 2, f_yuv);
    }
    
    printf("Finish!\n");
        
__FAIL:
    if (pic_rgb) {
        free(pic_rgb);
    }
    if (pic_yuv) {
        free(pic_yuv);
    }
    if (f_rgb) {
        fclose(f_rgb);
    }
    if (f_yuv) {
        fclose(f_yuv);
    }
}

/**
 * Convert
 * @param rgb_buf       rgb数据字符指针
 * @param yuv_buf       yuv数据字符指针
 * @param pixel_width     分辨率宽
 * @param pixel_height    分辨率高
 */
static void rgb24_to_yuv420p(unsigned char *rgb_buf, unsigned char *yuv_buf, int pixel_width, int pixel_height) {
    
    // 定义y、u、v分量指针  和rgb像素点的滑动指针
    unsigned char *y_ptr, *u_ptr, *v_ptr, *rgb_ptr;
    
    // 初始化yuv_buf
    memset(yuv_buf, 0, pixel_width * pixel_height * 3 / 2);
    
    // y、u、v三个分量指针分别指向yuv_buf中三个分量的位置
    y_ptr = yuv_buf;                                                 // y分量指针指向yuv_buf的开头位置
    u_ptr = yuv_buf + pixel_width * pixel_height;            // u分量指针指向yuv_buf偏移y分量大小的位置（pixel_width * pixel_height）
    v_ptr = u_ptr + pixel_width * pixel_height / 4;         // v分量指针指向u_ptr偏移u分量大小的位置（pixel_width * pixel_height / 4）
    
    // 定义y、u、v、r、g、b各分量的字符常量
    unsigned char y, u, v, r, g, b;
    for (int j = 0; j < pixel_height; j++) {
        // 每一行首个R的指针位置
        rgb_ptr = rgb_buf + pixel_width * j * 3;
        for (int i = 0; i < pixel_width; i++) {
            r = *(rgb_ptr++);   // 保存该像素点R的值  并将指针前移一个字节
            g = *(rgb_ptr++);   // 保存该像素点G的值  并将指针前移一个字节
            b = *(rgb_ptr++);   // 保存该像素点B的值  并将指针前移一个字节
            
//            // 矩阵计算  u、v的取值范围是(-128, 127)  存储时跟Y一样按无符号存储  所以要加上128
//            y = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
//            u = (unsigned char)(-0.172 * r - 0.339 * g + 0.511 * b) + 128;
//            v = (unsigned char)(0.512 * r - 0.428 * g - 0.083 * b) + 128;
            
            // 优化：减少浮点运算
            y = (unsigned char)((76 * r + 150 * g + 29 * b) >> 8);
            u = (unsigned char)(((-44 * r - 87 * g + 131 * b) >> 8) + 128);
            v = (unsigned char)(((131 * r - 110 * g - 21 * b) >> 8) + 128);
            
            // 每个像素写入一个Y
            *(y_ptr++) = y;
            
            if (0 == j % 2 && 0 == i % 2) {
                // 每四个像素写入一个U  取右下角的U值
                *(u_ptr++) = u;
            } else if (0 == i % 2) {
                // 每四个像素写入一个V  取右上角的V值
                *(v_ptr++) = v;
            }
        }
    }
}
