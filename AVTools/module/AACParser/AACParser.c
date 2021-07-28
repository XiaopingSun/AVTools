//
//  AACParser.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/7/27.
//

#include "AACParser.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ADTS 格式的帧结构包含固定头、可变头和crc检验，crc校验不一定有，所以整体头部占7字节或9字节
   adts_header = adts_fixed_header (28bit) + adts_variable_header (28bit) + *adts_error_check (16bit)
 
 adts_fixed_header:
 1、syncword：12bits  同步头 总是0xFFF  all bits must be 1  代表着一个ADTS帧的开始
 2、ID：1bit MPEG标识符  0标识MPEG-4  1标识MPEG-2
 3、Layer：2bits always: '00'
 4、protection_absent：1bit 表示是否误码校验。0：有crc校验，header 9字节  1：无crc校验，header 7字节
 5、profile：2bits 表示使用哪个级别的AAC  MPEG-2只包含MPEG-4前三个profile
 6、sampling_frequency_index：4bits 表示使用的采样率下标 参考AAC_SAMPLE_FREQUENCY
 7、private_bit：1bit  私有位，编码时设置为0，解码时忽略
 8、channel_configuration：3bits  表示声道数，比如2表示立体声双声道
 9、original_copy：1bit  编码时设置为0，解码时忽略
 10、home：1bits  编码时设置为0，解码时忽略
 
 adts_variable_header:
 1、copyright_identification_bit：1bit  编码时设置为0，解码时忽略
 2、copyright_identification_start：1bit  编码时设置为0，解码时忽略
 3、aac_frame_length：13bits  1个ADTS帧的长度包括ADTS头和音频编码数据  aac_frame_length = (protection_absent == 1 ? 7 : 9) + size(AACFrame)
 4、adts_buffer_fullness：11bits  固定为0x7FF。表示是码率可变的码流
 5、number_of_raw_data_blocks_in_frame：2bits  表示当前帧有number_of_raw_data_blocks_in_frame + 1 个原始帧
 */

/*
    常见aac格式单帧包含采样点个数
        PROFILE                 SAMPLES
        HE-AAC v1/v2             2048
        AAC-LC                       1024
        AAC-LD/AAC-ELD    480/512
 
     因此可以根据adts头部信息估算aac的单帧播放时长、音频时长、帧率、码率
    单帧播放时长 = 单帧sample个数 * 1000 / 采样率   单位ms
    音频时长 = 帧数 * 单帧播放时长
    帧率 = 1 / 单帧播放时长  单位fps
    码率 = header字段aac_frame_length * 8 * 采样率 / 单帧sample个数  单位bps
 */

#define AAC_BUFFER_SIZE  10 * 1024

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

typedef enum {
    AAC_ID_MPEG4 = 0,
    AAC_ID_MPEG2 = 1
} AAC_ID;

typedef enum {
    AAC_PROFILE_MAIN         = 0,
    AAC_PROFILE_LC             = 1,
    AAC_PROFILE_SSR           = 2,
    AAC_PROFILE_RESERVED  = 3
} AAC_PROFILE;

typedef enum {
    AAC_SAMPLE_FREQUENCY_96000    = 0,
    AAC_SAMPLE_FREQUENCY_88200    = 1,
    AAC_SAMPLE_FREQUENCY_64000    = 2,
    AAC_SAMPLE_FREQUENCY_48000    = 3,
    AAC_SAMPLE_FREQUENCY_44100    = 4,
    AAC_SAMPLE_FREQUENCY_32000    = 5,
    AAC_SAMPLE_FREQUENCY_24000    = 6,
    AAC_SAMPLE_FREQUENCY_22050    = 7,
    AAC_SAMPLE_FREQUENCY_16000    = 8,
    AAC_SAMPLE_FREQUENCY_12000    = 9,
    AAC_SAMPLE_FREQUENCY_11025    = 10,
    AAC_SAMPLE_FREQUENCY_8000      = 11,
    AAC_SAMPLE_FREQUENCY_7350      = 12,
    AAC_SAMPLE_FREQUENCY_RESERVED1 = 13,
    AAC_SAMPLE_FREQUENCY_RESERVED2 = 14,
    AAC_SAMPLE_FREQUENCY_ESCAPE_VALUE = 15
} AAC_SAMPLE_FREQUENCY;

typedef enum {
    AAC_CHANNEL_CONFIG_0   = 0,
    AAC_CHANNEL_CONFIG_1   = 1,
    AAC_CHANNEL_CONFIG_2   = 2,
    AAC_CHANNEL_CONFIG_3   = 3,
    AAC_CHANNEL_CONFIG_4   = 4,
    AAC_CHANNEL_CONFIG_5   = 5,
    AAC_CHANNEL_CONFIG_5PLUS1 = 6,
    AAC_CHANNEL_CONFIG_7PLUS1 = 7,
} AAC_CHANNEL_CONFIG;

const static char *aac_id[] = {"MPEG-4", "MPEG-2"};
const static char *aac_profile[] = {"AAC Main", "AAC LC", "AAC SSR", "Reserved"};
const static char *aac_sample_frequency[] = {"96000", "88200", "64000", "48000", "44100", "32000", "24000", "22050", "16000", "12000", "11025", "8000", "7350", "Reserved1", "Reserved2", "escape value"};
const static char *aac_channel_config[] = {"0", "1", "2", "3", "4", "5", "5+1", "7+1"};

typedef struct {
    AAC_ID id;
    bool protection_absent;
    AAC_PROFILE profile;
    AAC_SAMPLE_FREQUENCY sampling_frequency_index;
    AAC_CHANNEL_CONFIG channel_configuration;
    uint32_t aac_frame_length;
    uint32_t number_of_raw_data_blocks_in_frame;
} ADTS_HEADER;

static void parse(char *url);
static int get_adts_header(ADTS_HEADER *adts, FILE *aac_bit_stream);
static int find_start_code(unsigned char *buffer, size_t buffer_size);

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - AAC ADTS\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools AACParser -i input.aac\n\n");
    printf("Get Raw AAC With FFMPEG From Mp4 File:\n\n");
    printf("   ffmpeg -i video.mp4 -vn -acodec copy raw.aac\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void aac_parser_parse_cmd(int argc, char *argv[]) {
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
        printf("AACParser Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    parse(url);
}

/**
 * Parse
 * @param url    aac file path
 */
static void parse(char *url) {
    
    ADTS_HEADER *adts = NULL;
    FILE *aac_bit_stream = NULL;
    FILE *myout = stdout;
    int index = 0;
    
    // 打开输入文件
    aac_bit_stream = fopen(url, "rb");
    if (aac_bit_stream == NULL) {
        printf("Open File Error.\n");
        return;
    }
    
    // 初始化ADTS实例
    adts = (ADTS_HEADER *)calloc(1, sizeof(ADTS_HEADER));
    if (adts == NULL) {
        printf("Alloc ADTS Error.\n");
        fclose(aac_bit_stream);
        return;
    }
    
    printf("-------+----------+-----------+-------- ADTS Table -------+--------------+---------------+\n");
    printf("  NUM  |    ID    |  PROFILE  |   FREQUENCY   |  CHANNEL  |  FRAME SIZE  |  FRAME COUNT  \n");
    printf("-------+----------+-----------+---------------+-----------+--------------+---------------+\n");
    
    while (!feof(aac_bit_stream)) {
        if (get_adts_header(adts, aac_bit_stream) == 0) {
            char mpeg_id[10] = {0};
            char profile[10] = {0};
            char frequency[15] = {0};
            char channel[4] = {0};
            int frame_size = 0;
            int frame_count = 0;
            
            sprintf(mpeg_id, "%s", aac_id[adts->id]);
            sprintf(profile, "%s", aac_profile[adts->profile]);
            sprintf(frequency, "%s", aac_sample_frequency[adts->sampling_frequency_index]);
            sprintf(channel, "%s", aac_channel_config[adts->channel_configuration]);
            frame_size = adts->aac_frame_length;
            frame_count = adts->number_of_raw_data_blocks_in_frame;
            
            fprintf(myout, " %5d | %8s | %9s | %13s | %9s | %12d | %13d \n", index, mpeg_id, profile, frequency, channel, frame_size, frame_count);
            index++;
        } else {
            continue;
        }
    }
    
    if (adts) {
        free(adts);
    }
    
    if (aac_bit_stream) {
        fclose(aac_bit_stream);
    }
}

/**
 * Get adts header
 * @param adts    current instance of ADTS_HEADER
 * @param aac_bit_stream   input file
 * @return 0: success  -1: EOF
 */
static int get_adts_header(ADTS_HEADER *adts, FILE *aac_bit_stream) {

    unsigned char *buffer = (unsigned char *)calloc(1, AAC_BUFFER_SIZE);
    int ret = 0;
    size_t data_size = 0;
    
    while (!feof(aac_bit_stream)) {
        // 每次从文件当前位置读取10k的数据
        data_size = fread(buffer, 1, AAC_BUFFER_SIZE, aac_bit_stream);
        // 如果读到的数据小于7  说明不会包含完整header
        if (data_size < 7) {
            return EOF;
        }
        
        // 查找start code 即0xfff
        ret = find_start_code(buffer, data_size);
        if (ret == EOF) {
            // 没有读到start_code 需要将文件seek -1位  避免start_code刚好在buffer边界的极端情况
            fseek(aac_bit_stream, -1, SEEK_CUR);
            continue;
        } else {
            // 找到start_code位置  需要seek
            break;
        }
    }
        
    // 找到syncword，开始读header
    adts->id = (buffer[ret + 1] & 0x08) >> 3;
    adts->protection_absent = buffer[ret + 1] & 0x01;
    adts->profile = (buffer[ret + 2] & 0xc0) >> 6;
    adts->sampling_frequency_index = (buffer[ret + 2] & 0x3c) >> 2;
    
    int channel_config = 0;
    channel_config |= (buffer[ret + 2] & 0x01) << 2;
    channel_config |= (buffer[ret + 3] & 0xc0) >> 6;
    adts->channel_configuration = channel_config;
    
    int frame_size = 0;
    frame_size |= (buffer[ret + 3] & 0x03) << 11;
    frame_size |= buffer[ret + 4] << 3;
    frame_size |= (buffer[ret + 5] & 0xe0) >> 5;
    adts->aac_frame_length = frame_size;
    
    adts->number_of_raw_data_blocks_in_frame = buffer[ret + 6] & 0x03;
    
    fseek(aac_bit_stream, -(data_size - ret - frame_size), SEEK_CUR);
    
    if (buffer) {
        free(buffer);
    }
    
    return 0;
}

/**
 * Find Start Code
 * @param buffer    aac buffer
 * @param buffer_size   size
 * @return -1: EOF  other: start code index
 */
static int find_start_code(unsigned char *buffer, size_t buffer_size) {
    
    int ret = 0;
    
    while (1) {
        if (buffer[ret] == 0xff && (buffer[ret + 1] & 0xf0) == 0xf0) {
            break;
        }
        
        ret++;
        
        // 如果滑动到buffer倒数第二位还是找不到  重新读buffer
        if (ret == buffer_size - 1) {
            ret = EOF;
            break;
        }
    }
    
    return ret;
}
