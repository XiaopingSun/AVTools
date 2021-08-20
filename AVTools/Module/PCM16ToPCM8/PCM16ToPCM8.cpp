//
//  PCM16ToPCM8.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/5/9.
//

#include "PCM16ToPCM8.h"
#include <getopt.h>
#include <CoreFoundation/CoreFoundation.h>

static void convert(char *url);

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - 16 Bit Depth PCM\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools PCM16ToPCM8 -i s16le.pcm\n\n");
    printf("Get 2 Channels PCM16 With FFMpeg From Mp4 File:\n\n");
    printf("   ffmpeg -i 1.mp4 -vn -ar 44100 -ac 2 -f s16le s16le.pcm\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void pcm16_to_pcm8_parse_cmd(int argc, char *argv[]) {
    
    int option = 0;   // getopt_long的返回值，返回匹配到字符的ascii码，没有匹配到可读参数时返回-1
    char *url = NULL;   // 输入文件路径
    
    while (EOF != (option = getopt_long(argc, argv, "i:", tool_long_options, NULL))) {
        switch (option) {
            case '`':
                show_module_help();
                return;
                break;
            case 'i':
                url = optarg;
                break;
            case '?':
                printf("Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
                return;
                break;
            default:
                break;
        }
    }
    
    if (NULL == url) {
        printf("PCM16ToPCM8 Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    convert(url);
}

/**
 * Convert
 * @param url    PCM16 Local Url Path
 */
static void convert(char *url) {
    
    FILE *f_pcm16 = NULL;
    FILE *f_pcm8 = NULL;
    char output_url[100];
    unsigned char *sample = NULL;
    short *sample_num_16 = NULL;
    char sample_num_8 = 0;
    unsigned char sample_num_8_u = 0;
    
    // 输出路径
    sprintf(output_url, "%s_8bit.pcm", url);
    
    // 打开PCM16文件
    f_pcm16 = fopen(url, "rb");
    if (!f_pcm16) {
        printf("PCM16ToPCM8: Could Not Open PCM16 File.\n");
        goto __FAIL;
    }

    // 打开PCM8输出路径
    f_pcm8 = fopen(output_url, "wb+");
    if (!f_pcm8) {
        printf("PCM16ToPCM8: Could Not Open Output Path.\n");
        goto __FAIL;
    }
    
    // 初始化单个采样所占的堆内存  4字节
    sample = (unsigned char *)malloc(4);
    
    while (!feof(f_pcm16)) {
        fread(sample, 1, 4, f_pcm16);
        
        // L
        sample_num_16 = (short *)sample;
        sample_num_8 = (*sample_num_16) >> 8;
        sample_num_8_u = sample_num_8 + 128;
        fwrite(&sample_num_8_u, 1, 1, f_pcm8);
        
        // R
        sample_num_16 = (short *)(sample + 2);
        sample_num_8 = (*sample_num_16) >> 8;
        sample_num_8_u = sample_num_8 + 128;
        fwrite(&sample_num_8_u, 1, 1, f_pcm8);
        
        // 这里虽然s16le的存储方式是小端存储  转换成short后实际是按高位到低位来排列  所以右移8位实际是将short的低位8bit舍掉  只保留高位8bit
    }
    
    printf("Finish!\n");
    free(sample);
    
__FAIL:
    if (f_pcm16) {
        fclose(f_pcm16);
    }
    if (f_pcm8) {
        fclose(f_pcm8);
    }
}
