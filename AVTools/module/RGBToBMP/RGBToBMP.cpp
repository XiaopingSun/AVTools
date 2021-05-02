//
//  RGBToBMP.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/4/30.
//

#include "RGBToBMP.h"
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>

static void convert(char *url, int count, int pixel_width, int pixel_height);

// 位图文件头 - 14Byte
typedef struct tagBITMAPFILEHEADER {
    uint16_t bf_type;       // 位图格式  'BM'  2字节
    uint32_t bf_size;        // 位图文件整体大小  4字节
    uint16_t bf_reserved1;   // 保留字段  为0   2字节
    uint16_t bf_reserved2;   // 保留字段  为0   2字节
    uint32_t bf_offsetBits;   // 像素数据的偏移量   对于RGB24  等于位图文件头+位图信息头大小  单位字节  4字节
} BITMAPFILEHEADER;

// 位图信息头 - 40Byte
typedef struct tagBITMAPINFOHEADER {
    uint32_t bi_size;      // 位图信息头的大小  单位字节   4字节
    uint32_t bi_width;    // 位图的宽度  单位是像素   4字节
    uint32_t bi_height;   // 位图的高度   单位是像素  4字节 (这块如果是正值 位图像素默认排列顺序是左下到右上 所以一般设为负值)
    uint16_t bi_planes;   //目标设备的级别   必须为1    2字节
    uint16_t bi_bitCount;  // 像素位深   1-黑白图，4-16色，8-256色，24-真彩色   2字节
    uint32_t bi_compression;  // 压缩方式  0为不压缩   4字节
    uint32_t bi_sizeImage;   // 像素数据大小  单位字节   biCompression时可设为0  4字节
    uint32_t bi_x_pels_per_meter;  // 位图水平分辨率(像素/米)  0   4字节
    uint32_t bi_y_pels_per_meter;  // 位图垂直分辨率(像素/米)  0   4字节
    uint32_t bi_clr_used;    // 位图实际使用的索引中的颜色数   与biBitCount对应的颜色个数对应   4字节
    uint32_t bi_clr_important;  // 位图索引中重要的颜色数，0代表所有颜色都重要   4字节
} BITMAPINFOHEADER;

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - RGB24\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -c:   BMP Output Count   Default Is All\n");
    printf("  -w:   Width\n");
    printf("  -h:   Height\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools RGBToBMP -c 30 -w 720 -h 1280 -i rgb24.rgb\n\n");
    printf("Get RBG24 With FFMpeg From A Mp4 File:\n\n");
    printf("  ffmpeg -i out.mp4 -an -c:v rawvideo -pix_fmt rgb24 rgb24.rgb\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void raw_to_bmp_parse_cmd(int argc, char *argv[]) {
    
    int option = 0;   // getopt_long的返回值，返回匹配到字符的ascii码，没有匹配到可读参数时返回-1
    int count = 0;    // 输出BMP文件个数
    int width = 0;    // 分辨率宽
    int height = 0;    // 分辨率高
    char *url = NULL;   // 输入文件路径
    
    while (EOF != (option = getopt_long(argc, argv, "i:c:w:h:", tool_long_options, NULL))) {
        switch (option) {
            case '`':
                show_module_help();
                return;
                break;
            case 'i':
                url = optarg;
                break;
            case 'c':
                count = atoi(optarg);
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
        printf("RGBToBMP Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    convert(url, count, width, height);
}

/**
 * Start Convert
 * @param url     RGB File Local Path
 * @param count    Output BMP Count   Default Is All
 * @param pixel_width    Pixel Width Of RGB
 * @param pixel_height   Pixel Height Of RGB
 */
static void convert(char *url, int count, int pixel_width, int pixel_height) {
    
    BITMAPFILEHEADER bf_header = {0};
    BITMAPINFOHEADER bi_header = {0};
    FILE *f_rgb = NULL;
    DIR *bmp_dir = NULL;
    FILE *f_bmp = NULL;
    char *rgb24_buffer = NULL;
    long total_size = 0;
    int bpm_count_in_rgb = 0;
    int output_count = 0;
    int bmp_body_size = pixel_width * pixel_height * 3;
    
    // 输出目录
    char *output_base_name = (char *)"/bmp";
    char *output_dir_name = strcat(dirname(url), output_base_name);
    char output_file_path[100];
    
    // 创建目录文件夹
    bmp_dir = opendir(output_dir_name);
    if (!bmp_dir) {
        if (EOF == mkdir(output_dir_name, S_IRWXU)) {
            printf("RGBToBMP Error: Cannot Create Output Directory.\n");
            goto __FAIL;
            return;
        }
    }

    // 打开rgb文件
    f_rgb = fopen(url, "rb");
    if (!f_rgb) {
        printf("RGBToBMP Error: Cannot Open RGB File.\n");
        goto __FAIL;
        return;
    }
    
    // 读取rgb文件大小
    fseek(f_rgb, 0, SEEK_END);
    total_size = ftell(f_rgb);
    fseek(f_rgb, 0, SEEK_SET);
    
    // 计算rgb文件帧数  比较count参数算出最终输出的bmp个数
    bpm_count_in_rgb = (int)total_size / bmp_body_size;
    output_count = (count > bpm_count_in_rgb || count == 0)  ? bpm_count_in_rgb : count;
    
    // 位图文件头
    bf_header.bf_type = 0x4d42;  // 调整下'B' 'M'的顺序
    bf_header.bf_size = 0x0000000e + 0x00000028 + bmp_body_size;
    bf_header.bf_reserved1 = 0x0000;
    bf_header.bf_reserved2 = 0x0000;
    bf_header.bf_offsetBits = 0x0000000e + 0x00000028;
    
    // 位图信息头
    bi_header.bi_size = 0x00000028;
    bi_header.bi_width = pixel_width;
    bi_header.bi_height = -pixel_height;   // 这里用负值
    bi_header.bi_planes = 0x0001;
    bi_header.bi_bitCount = 0x0018;
    bi_header.bi_compression = 0x00000000;
    bi_header.bi_sizeImage = bmp_body_size;
    bi_header.bi_x_pels_per_meter = 0x00000000;
    bi_header.bi_y_pels_per_meter = 0x00000000;
    bi_header.bi_clr_used = 0x00000000;
    bi_header.bi_clr_important = 0x00000000;
    
    // 写入像素数据
    for (int i = 0; i < output_count; i++) {
        
        // 目标目录下open file
        sprintf(output_file_path, "%s/%s.%d.bmp", output_dir_name, basename(url), i);
        f_bmp = fopen(output_file_path, "wb+");
        if (!f_bmp) {
            printf("RGBToBMP Error: Cannot Open BMP File.\n");
            goto __FAIL;
            return;
        }
        
        printf("RGBToBMP 正在写入 %s\n", output_file_path);
        
        // 从rgb文件中读取单位像素数据大小
        rgb24_buffer = (char *)malloc(bmp_body_size);
        fread(rgb24_buffer, 1,bmp_body_size, f_rgb);
                
        // 这里要将RGB像素数据的R和B调整下位置  将B放到内存低位
        for (int j = 0; j < pixel_height; j++) {
            for (int i = 0; i < pixel_width; i++) {
                char temp_B = rgb24_buffer[(j * pixel_width + i) * 3 + 2];
                rgb24_buffer[(j * pixel_width + i) * 3 + 2] = rgb24_buffer[(j * pixel_width + i) * 3];
                rgb24_buffer[(j * pixel_width + i) * 3] = temp_B;
            }
        }
        
        // 写入位图文件头、位图信息头、像素数据，考虑结构体对齐问题，将两个header中各元素分开写入
        fwrite(&bf_header.bf_type, sizeof(bf_header.bf_type), 1, f_bmp);
        fwrite(&bf_header.bf_size, sizeof(bf_header.bf_size), 1, f_bmp);
        fwrite(&bf_header.bf_reserved1, sizeof(bf_header.bf_reserved1), 1, f_bmp);
        fwrite(&bf_header.bf_reserved2, sizeof(bf_header.bf_reserved2), 1, f_bmp);
        fwrite(&bf_header.bf_offsetBits, sizeof(bf_header.bf_offsetBits), 1, f_bmp);
        
        fwrite(&bi_header.bi_size, sizeof(bi_header.bi_size), 1, f_bmp);
        fwrite(&bi_header.bi_width, sizeof(bi_header.bi_width), 1, f_bmp);
        fwrite(&bi_header.bi_height, sizeof(bi_header.bi_height), 1, f_bmp);
        fwrite(&bi_header.bi_planes, sizeof(bi_header.bi_planes), 1, f_bmp);
        fwrite(&bi_header.bi_bitCount, sizeof(bi_header.bi_bitCount), 1, f_bmp);
        fwrite(&bi_header.bi_compression, sizeof(bi_header.bi_compression), 1, f_bmp);
        fwrite(&bi_header.bi_sizeImage, sizeof(bi_header.bi_sizeImage), 1, f_bmp);
        fwrite(&bi_header.bi_x_pels_per_meter, sizeof(bi_header.bi_x_pels_per_meter), 1, f_bmp);
        fwrite(&bi_header.bi_y_pels_per_meter, sizeof(bi_header.bi_y_pels_per_meter), 1, f_bmp);
        fwrite(&bi_header.bi_clr_used, sizeof(bi_header.bi_clr_used), 1, f_bmp);
        fwrite(&bi_header.bi_clr_important, sizeof(bi_header.bi_clr_important), 1, f_bmp);
        
        // 写入像素数据
        fwrite(rgb24_buffer, bmp_body_size, 1, f_bmp);
        
        fclose(f_bmp);
        free(rgb24_buffer);
    }
    
    printf("Finish!\n");
    
__FAIL:
    
    if (f_rgb) {
        fclose(f_rgb);
    }
    if (bmp_dir) {
        closedir(bmp_dir);
    }
}
