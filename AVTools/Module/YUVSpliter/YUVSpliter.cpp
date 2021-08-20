//
//  YUVSpliter.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/5/4.
//

#include "YUVSpliter.h"
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
    printf("  - YU12\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -w:   Width\n");
    printf("  -h:   Height\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools YUVSpliter -w 720 -h 1280 -i yu12.yuv\n\n");
    printf("Get YU12 With FFMpeg From Mp4 File:\n\n");
    printf("  ffmpeg -i out.mp4 -an -c:v rawvideo -pix_fmt yuv420p yu12.yuv\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void yuv_spliter_parse_cmd(int argc, char *argv[]) {
    
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
 * @param url     YUV File Local Path
 * @param pixel_width    Pixel Width Of YUV
 * @param pixel_height   Pixel Height Of YUV
 */
static void split(char *url, int pixel_width, int pixel_height) {
    
    FILE *f_yuv = NULL;
    DIR *output_dir = NULL;
    FILE *f_y = NULL;
    FILE *f_u = NULL;
    FILE *f_v = NULL;
    char path_y[100];
    char path_u[100];
    char path_v[100];
    long total_size = 0;
    long frame_count = 0;
    unsigned char *pic = NULL;
    
    // 输出目录
    char *output_base_name = (char *)"/YUVSpliter";
    char *output_dir_name = strcat(dirname(url), output_base_name);
    
    // 输出文件名
    sprintf(path_y, "%s/%s.y", output_dir_name, basename(url));
    sprintf(path_u, "%s/%s.u", output_dir_name, basename(url));
    sprintf(path_v, "%s/%s.v", output_dir_name, basename(url));
    
    // 创建目录文件夹
    output_dir = opendir(output_dir_name);
    if (!output_dir) {
        if (-1 == mkdir(output_dir_name, S_IRWXU)) {
            printf("YUVSpliter Error: Cannot Create Output Directory.\n");
            goto __FAIL;
        }
    }
    
    // 打开YUV文件
    f_yuv = fopen(url, "rb");
    if (!f_yuv) {
        printf("YUVSpliter Error: Cannot Open The Input File.\n");
        goto __FAIL;
    }
    
    // 计算yuv文件大小
    fseek(f_yuv, 0, SEEK_END);
    total_size = ftell(f_yuv);
    fseek(f_yuv, 0, SEEK_SET);
    
    // 计算yuv中包含的帧数
    frame_count = total_size / (pixel_width * pixel_height * 3 / 2);
    
    // 打开y、u、v文件
    f_y = fopen(path_y, "wb+");
    if (!f_y) {
        printf("YUVSpliter Error: Cannot Open The Output File.\n");
        goto __FAIL;
    }
    
    f_u = fopen(path_u, "wb+");
    if (!f_u) {
        printf("YUVSpliter Error: Cannot Open The Output File.\n");
        goto __FAIL;
    }
    
    f_v = fopen(path_v, "wb+");
    if (!f_v) {
        printf("YUVSpliter Error: Cannot Open The Output File.\n");
        goto __FAIL;
    }
    
    pic = (unsigned char *)malloc(pixel_width * pixel_height * 3 / 2);
    
    for (int i = 0; i < 1; i++) {
        fread(pic, 1, pixel_width * pixel_height * 3 / 2, f_yuv);
        fwrite(pic, 1, pixel_width * pixel_height, f_y);
        fwrite(pic + pixel_width * pixel_height, 1, pixel_width * pixel_height / 4, f_u);
        fwrite(pic + pixel_width * pixel_height * 5 / 4, 1, pixel_width * pixel_height / 4, f_v);
    }
    
    printf("Finish!\n");
    
__FAIL:
    if (output_dir) {
        closedir(output_dir);
    }
    if (f_yuv) {
        fclose(f_yuv);
    }
    if (f_y) {
        fclose(f_y);
    }
    if (f_u) {
        fclose(f_u);
    }
    if (f_v) {
        fclose(f_v);
    }
    if (pic) {
        free(pic);
    }
}
