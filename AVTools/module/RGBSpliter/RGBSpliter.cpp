//
//  RGBSpliter.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/5/5.
//

#include "RGBSpliter.h"
#include <getopt.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>

static void split(char *url, int pixel_width, int pixel_height);

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
    printf("  -w:   Width\n");
    printf("  -h:   Height\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools RGBSpliter -w 720 -h 1280 -i rgb24.rgb\n\n");
    printf("Get RGB24 With FFMpeg From Mp4 File:\n\n");
    printf("  ffmpeg -i out.mp4 -an -c:v rawvideo -pix_fmt rgb24 rgb24.rgb\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void rgb_spliter_parse_cmd(int argc, char *argv[]) {
    
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
        printf("YUVSpliter: Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    split(url, width, height);
}

/**
 * Split
 * @param url     RGB File Local Path
 * @param pixel_width    Pixel Width Of RGB
 * @param pixel_height   Pixel Height Of RGB
 */
static void split(char *url, int pixel_width, int pixel_height) {
    
    FILE *f_rgb = NULL;
    DIR *output_dir = NULL;
    FILE *f_r = NULL;
    FILE *f_g = NULL;
    FILE *f_b = NULL;
    char path_r[100];
    char path_g[100];
    char path_b[100];
    long total_size = 0;
    long frame_count = 0;
    unsigned char *pic = NULL;
    
    // 输出目录
    char *output_base_name = (char *)"/RGBSpliter";
    char *output_dir_name = strcat(dirname(url), output_base_name);
    
    // 输出文件名
    sprintf(path_r, "%s/%s.r", output_dir_name, basename(url));
    sprintf(path_g, "%s/%s.g", output_dir_name, basename(url));
    sprintf(path_b, "%s/%s.b", output_dir_name, basename(url));
    
    // 创建目录文件夹
    output_dir = opendir(output_dir_name);
    if (!output_dir) {
        if (-1 == mkdir(output_dir_name, S_IRWXU)) {
            printf("RGBSpliter Error: Cannot Create Output Directory.\n");
            goto __FAIL;
        }
    }
    
    // 打开YUV文件
    f_rgb = fopen(url, "rb");
    if (!f_rgb) {
        printf("RGBSpliter Error: Cannot Open The Input File.\n");
        goto __FAIL;
    }
    
    // 计算yuv文件大小
    fseek(f_rgb, 0, SEEK_END);
    total_size = ftell(f_rgb);
    fseek(f_rgb, 0, SEEK_SET);
    
    // 计算yuv中包含的帧数
    frame_count = total_size / (pixel_width * pixel_height * 3);
    
    // 打开r、g、b文件
    f_r = fopen(path_r, "wb+");
    if (!f_r) {
        printf("RGBSpliter Error: Cannot Open The Output File.\n");
        goto __FAIL;
    }
    
    f_g = fopen(path_g, "wb+");
    if (!f_g) {
        printf("RGBSpliter Error: Cannot Open The Output File.\n");
        goto __FAIL;
    }
    
    f_b = fopen(path_b, "wb+");
    if (!f_b) {
        printf("RGBSpliter Error: Cannot Open The Output File.\n");
        goto __FAIL;
    }
    
    pic = (unsigned char *)malloc(pixel_width * pixel_height * 3);
    
    for (int i = 0; i < 1; i++) {
        fread(pic, 1, pixel_width * pixel_height * 3, f_rgb);
        for (int j = 0; j < pixel_width * pixel_height * 3; j = j + 3) {
            fwrite(pic + j, 1, 1, f_r);
            fwrite(pic + j + 1, 1, 1, f_g);
            fwrite(pic + j + 2, 1, 1, f_b);
        }
    }
    
    printf("Finish!\n");
    
    free(pic);
    
__FAIL:
    if (output_dir) {
        closedir(output_dir);
    }
    if (f_rgb) {
        fclose(f_rgb);
    }
    if (f_r) {
        fclose(f_r);
    }
    if (f_g) {
        fclose(f_g);
    }
    if (f_b) {
        fclose(f_b);
    }
}

