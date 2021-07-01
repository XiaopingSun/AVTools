//
//  PCMToWAV.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/5/9.
//

#include "PCMToWAV.h"
#include <getopt.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdlib.h>

// 所谓的大端模式（Big Endian），是指数据的低位保存在内存的高地址中，而数据的高位，保存在内存的低地址中。
// 所谓的小端模式（Little Endian），是指数据的低位保存在内存的低地址中，而数据的高位保存在内存的高地址中。

#pragma pack(1)
typedef struct tag_WAV_RIFF_CHUNK {
    uint32_t ID;           // 'RIFF'标识   Big Endian   4Byte   内存：0x52494646
    uint32_t size;        // 整个wav文件的长度减去ID和size的长度   Little Endian   4Byte
    uint32_t type;       // type是'WAVE'标识后边需要两个字块：fmt区块和data区块   Big Endian   4Byte   内存：0x57415645
} WAV_RIFF_CHUNK;

#pragma pack(1)
typedef struct tag_WAV_FMT_CHUNK {
    uint32_t ID;                      // 'fmt '标识   Big Endian   4Byte   内存：0x666D7420
    uint32_t size;                   // fmt区块的长度  不包含ID和size的长度   Little Endian   4Byte   16
    uint16_t audio_format;       // 标识data区块存储的音频数据格式  PCM为1   Little Endian   2Byte
    uint16_t num_channels;      // 标识音频数据的声道数  1：单声道   2：双声道   Little Endian   2Byte
    uint32_t sample_rate;        // 标识音频数据的采样率   Little Endian    4Byte
    uint32_t byte_rate;           // 每秒数据字节数 = sample_rate * num_channels * bits_per_sample / 8    Little Endian   4Byte
    uint16_t block_align;          // 每个采样所需的字节数 = num_channels * bits_per_sample / 8   Little Endian   2Byte
    uint16_t bits_per_sample;   // 每个采样存储的bit数   Little Endian   2Byte
} WAV_FMT_CHUNK;

#pragma pack(1)
typedef struct tag_WAV_DATA_CHUNK {
    uint32_t ID;              // 'data'标识   Big Endian   4Byte  内存：0x64617461
    uint32_t size;           // 标识音频数据的长度 = byte_rate * seconds   Little Endian   4Byte
    void *data;               // 音频数据  Little Endian
} WAV_DATA_CHUNK;

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

static void convert(char *url, int sample_rate, int channel_count, int sample_bit_depth);

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - U8\n");
    printf("  - S16\n");
    printf("  - S32\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -r:   Sample Rate\n");
    printf("  -b:   Sample Bit Depth\n");
    printf("  -c:   Channel Count\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools PCMToWAV -r 44100 -c 2 -b 16 -i s16le.pcm\n\n");
    printf("Get PCM With FFMpeg From Mp4 File:\n\n");
    printf("   ffmpeg -i 1.mp4 -vn -ar 44100 -ac 2 -f s16le s16le.pcm\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void pcm_to_wav_parse_cmd(int argc, char *argv[]) {
    
    int option = 0;   // getopt_long的返回值，返回匹配到字符的ascii码，没有匹配到可读参数时返回-1
    char *url = NULL;   // 输入文件路径
    int sample_rate = 0;
    int channel_count = 0;
    int sample_bit_depth = 0;
    
    while (EOF != (option = getopt_long(argc, argv, "i:r:c:b:", tool_long_options, NULL))) {
        switch (option) {
            case '`':
                show_module_help();
                return;
                break;
            case 'i':
                url = optarg;
                break;
            case 'r':
                sample_rate = atoi(optarg);
                break;
            case 'c':
                channel_count = atoi(optarg);
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
    
    if (NULL == url || 0 == sample_rate || 0 == channel_count || 0 == sample_bit_depth) {
        printf("PCMToWAV Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    convert(url, sample_rate, channel_count, sample_bit_depth);
}

/**
 * Convert
 * @param url     Input PCM Url
 * @param sample_rate     Sample Rate
 * @param channel_count     Channel Count
 * @param sample_bit_depth    Sample Bit Depth
 */
static void convert(char *url, int sample_rate, int channel_count, int sample_bit_depth) {
    
    WAV_RIFF_CHUNK riff_chunk = {0};
    WAV_FMT_CHUNK fmt_chunk = {0};
    WAV_DATA_CHUNK data_chunk = {0};
    char output_url[100];
    FILE *f_pcm = NULL;
    FILE *f_wav = NULL;
    size_t data_offset;
    uint32_t data_length = 0;
    
    // 拼接输出路径
    sprintf(output_url, "%s.wav", url);
    
    // 打开PCM文件
    f_pcm = fopen(url, "rb");
    if (!f_pcm) {
        printf("PCMToWAV: Could Not Open PCM File.\n");
        goto __FAIL;
    }
    
    // 打开WAV输出文件
    f_wav = fopen(output_url, "wb+");
    if (!f_wav) {
        printf("PCMToWAV: Could Not Open Output Url.\n");
        goto __FAIL;
    }
    
    // seek到音频数据的位置  先写入data
    data_offset = sizeof(WAV_RIFF_CHUNK) + sizeof(WAV_FMT_CHUNK) + sizeof(data_chunk.ID) + sizeof(data_chunk.size);
    fseek(f_wav, data_offset, SEEK_CUR);
    
    data_chunk.data = (void *)malloc(channel_count * sample_bit_depth / 8);
    
    // 写入data
    while (!feof(f_pcm)) {
        fread(data_chunk.data, 1, channel_count * sample_bit_depth / 8, f_pcm);
        fwrite(data_chunk.data, 1, channel_count * sample_bit_depth / 8, f_wav);
        data_length += channel_count * sample_bit_depth / 8;
    }
        
    // seek到wav文件头
    rewind(f_wav);
    
    // 写入WAV_RIFF_CHUNK
    riff_chunk.ID = 0x46464952;    // 'RIFF'
    riff_chunk.size = sizeof(riff_chunk.type) + sizeof(WAV_FMT_CHUNK) + sizeof(data_chunk.ID) + sizeof(data_chunk.size) + data_length;
    riff_chunk.type = 0x45564157;   // 'WAVE'
    fwrite(&riff_chunk, 1, sizeof(WAV_RIFF_CHUNK), f_wav);
    
    // 写入WAV_FMT_CHUNK
    fmt_chunk.ID = 0x20746D66;   // 'fmt '
    fmt_chunk.size = 0x00000010;  // 16
    fmt_chunk.audio_format = 0x0001;  // 1：pcm
    fmt_chunk.num_channels = channel_count;
    fmt_chunk.sample_rate = sample_rate;
    fmt_chunk.byte_rate = sample_rate * channel_count * sample_bit_depth / 8;
    fmt_chunk.block_align = channel_count * sample_bit_depth / 8;
    fmt_chunk.bits_per_sample = sample_bit_depth;
    fwrite(&fmt_chunk, 1, sizeof(WAV_FMT_CHUNK), f_wav);
    
    // 写WAV_DATA_CHUNK的ID和size
    data_chunk.ID = 0x61746164;    // 'data'
    data_chunk.size = data_length;
    fwrite(&data_chunk.ID, 1, sizeof(data_chunk.ID), f_wav);
    fwrite(&data_chunk.size, 1, sizeof(data_chunk.size), f_wav);
    
    free(data_chunk.data);
    printf("Finish!\n");
    
__FAIL:
    if (f_pcm) {
        fclose(f_pcm);
    }
    if (f_wav) {
        fclose(f_wav);
    }
}
