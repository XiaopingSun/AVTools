//
//  PCMSpliter.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/5/8.
//

#include "PCMSpliter.h"
#include <getopt.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

static void split(char *url, int sample_bit_depth);

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - 2 Channels PCM\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -b:   Sample Bit Depth\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools PCMSpliter -b 16 -i s16le.pcm\n\n");
    printf("Get 2 Channels PCM With FFMpeg From Mp4 File:\n\n");
    printf("   ffmpeg -i 1.mp4 -vn -ar 44100 -ac 2 -f s16le s16le.pcm\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void pcm_spliter_parse_cmd(int argc, char *argv[]) {
    
    int option = 0;   // getopt_long的返回值，返回匹配到字符的ascii码，没有匹配到可读参数时返回-1
    int sample_bit_depth = 0;    // 采样位深
    char *url = NULL;   // 输入文件路径
    
    while (EOF != (option = getopt_long(argc, argv, "i:b:", tool_long_options, NULL))) {
        switch (option) {
            case '`':
                show_module_help();
                return;
                break;
            case 'i':
                url = optarg;
                break;
            case 'b':
                sample_bit_depth = atoi(optarg);
                break;
            case '?':
                printf("Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
                return;
                break;
            default:
                break;
        }
    }
    
    if (NULL == url || 0 == sample_bit_depth) {
        printf("PCMSpliter Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    split(url, sample_bit_depth);
}

static void split(char *url, int sample_bit_depth) {
    
    char path_pcm_l[100];
    char path_pcm_r[100];
    DIR *output_dir = NULL;
    FILE *f_pcm = NULL;
    FILE *f_l = NULL;
    FILE *f_r = NULL;
    unsigned char *sample = NULL;
    int byte_per_channel = sample_bit_depth / 8;
    
    // 输出目录
    char *output_base_name = (char *)"/PCMSpliter";
    char *output_dir_name = strcat(dirname(url), output_base_name);
    
    // 输出文件名
    sprintf(path_pcm_l, "%s/%s_l.pcm", output_dir_name, basename(url));
    sprintf(path_pcm_r, "%s/%s_r.pcm", output_dir_name, basename(url));
    
    // 创建目录文件夹
    output_dir = opendir(output_dir_name);
    if (!output_dir) {
        if (-1 == mkdir(output_dir_name, S_IRWXU)) {
            printf("PCMSpliter Error: Cannot Create Output Directory.\n");
            goto __FAIL;
        }
    }
    
    // 打开pcm文件
    f_pcm = fopen(url, "rb");
    if (!f_pcm) {
        printf("PCMSpliter Error: Cannot Open The Input File.\n");
        goto __FAIL;
    }
    
    // 打开左右声道输出文件
    f_l = fopen(path_pcm_l, "wb+");
    if (!f_l) {
        printf("PCMSpliter Error: Cannot Open The Output File.");
        goto __FAIL;
    }
    
    f_r = fopen(path_pcm_r, "wb+");
    if (!f_r) {
        printf("PCMSpliter Error: Cannot Open The Output File.");
        goto __FAIL;
    }
    
    sample = (unsigned char *)malloc(byte_per_channel * 2);
    
    while (!feof(f_pcm)) {
        fread(sample, 1, byte_per_channel * 2, f_pcm);
        fwrite(sample, 1, byte_per_channel, f_l);
        fwrite(sample + byte_per_channel, 1, byte_per_channel, f_r);
    }
    
    printf("Finish!\n");
    
    free(sample);
    
__FAIL:
    if (output_dir) {
        closedir(output_dir);
    }
    if (f_pcm) {
        fclose(f_pcm);
    }
    if (f_l) {
        fclose(f_l);
    }
    if (f_r) {
        fclose(f_r);
    }
}
