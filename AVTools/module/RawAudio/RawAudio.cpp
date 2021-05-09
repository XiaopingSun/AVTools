//
//  RawAudio.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/4/28.
//

#include "RawAudio.h"
#include <getopt.h>
#include "SDL.h"

#define BLOCK_SIZE  1048576   // 1MB
#define SAMPLE_COUNT_PER_CB  1024   // sdl每次回调获取的buffer大小

// sample_format
#define U8       "U8"
#define S16    "S16"
#define S32    "S32"
#define FLT     "FLT"

static Uint8 *audio_buffer = NULL;   // 每次读取的pcm音频buffer指针
static Uint8 *audio_buffer_pos = NULL;   // SDL每次读取的pcm音频buffer指针位置
static size_t buffer_len = 0;   // 每次读取的PCM长度  默认是BLOCK_SIZE
static size_t buffer_left = 0;   // 每次读取的PCM数据剩余长度

static void open(char *url, char *sample_format, int sample_rate, int channel_count);
static SDL_AudioFormat get_sample_format_in_sdl(char *sample_format);
static void read_audio_data(void *userdata, Uint8 * stream, int len);

static char const *support_sf_list[] {
    U8,
    S16,
    S32,
    FLT
};

static char const *instr_sf_list[] {
    " <Unsigned 8-bit samples>",
    "<Signed 16-bit samples Little-endian>",
    "<Signed 32-bit samples Little-endian>",
    "<32-bit floating point samples Little-endian>"
};

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    for (int i = 0; i < sizeof(support_sf_list) / sizeof(char *); i++) {
        printf("  %s    %s\n", support_sf_list[i], instr_sf_list[i]);
    }
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -f:   Sample Format\n");
    printf("  -r:   Sample Rate\n");
    printf("  -c:   Channel Count\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools RawAudio -f S16LE -r 44100 -c 2 -i input.pcm\n");
}

/**
 * Parse Cmd
 * @param argc     from main.cpp
 * @param argv     from main.cpp
 */
void raw_audio_parse_cmd(int argc, char *argv[]) {
    
    int option = 0;   // getopt_long的返回值，返回匹配到字符的ascii码，没有匹配到可读参数时返回-1
    int sample_rate = 0;    // 采样率
    int channel_count = 0;    // 声道数
    char *sample_format = NULL;   // 像素格式
    char *url = NULL;   // 输入文件路径
    bool format_supported = false;   // 输入像素格式是否支持
    
    while (EOF != (option = getopt_long(argc, argv, "i:r:c:f:", tool_long_options, NULL))) {
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
            case 'f':
                sample_format = optarg;
                break;
            case '?':
                printf("Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
                return;
                break;
            default:
                break;
        }
    }
    
    if (NULL == url || 0 == sample_rate || 0 == channel_count || NULL == sample_format) {
        printf("RawAudio Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    // 判断输入的像素格式是否支持
    for (int i = 0; i < sizeof(support_sf_list) / sizeof(char *); i++) {
        if (0 == strcmp(sample_format, support_sf_list[i])) {
            format_supported = true;
            break;
        }
    }
    
    if (format_supported) {
        open(url, sample_format, sample_rate, channel_count);
    } else {
        printf("Unsupport Sample Buffer Format, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
    }
}

/**
 * Open SDL
 * @param url                   pcm file path
 * @param sample_format     sample format
 * @param sample_rate         sample rate
 * @param channel_count      channel count
 */
static void open(char *url, char *sample_format, int sample_rate, int channel_count) {
    
    FILE *file = NULL;
    SDL_AudioSpec spec;
    
    // 初始化SDL
    if (SDL_Init(SDL_INIT_AUDIO)) {
        printf("Could Not Initialize SDL - %s\n", SDL_GetError());
        goto __FAIL;
    }
    
    // 打开pcm文件
    file = fopen(url, "rb");
    if (!file) {
        printf("Could Not Open This File '%s', Please Check The Path.\n", url);
        goto __FAIL;
    }
    
    // 配置SDL_AudioSpec参数
    spec.freq = sample_rate;
    spec.format = get_sample_format_in_sdl(sample_format);
    spec.channels = channel_count;
    spec.silence = 0;
    spec.samples = SAMPLE_COUNT_PER_CB;
    spec.callback = read_audio_data;
    
    // 打开音频设备
    if (SDL_OpenAudio(&spec, NULL)) {
        printf("Could Not Open Audio Device - %s\n", SDL_GetError());
        goto __FAIL;
    }
    
    audio_buffer = (Uint8 *)malloc(BLOCK_SIZE);
    
    // 开始播放
    SDL_PauseAudio(0);
    
    // 读pcm文件数据
    while (!feof(file)) {
        buffer_len = fread(audio_buffer, 1, BLOCK_SIZE, file);
        printf("RawAudio: Reading PCM Block Size %zu\n", buffer_len);
        audio_buffer_pos = audio_buffer;
        buffer_left = buffer_len;
        while (audio_buffer_pos < audio_buffer + buffer_len) {
            SDL_Delay(1);
        }
    }
    
    printf("Finish!\n");
    
    // 关闭音频设备
    SDL_CloseAudio();
    
    free(audio_buffer);
    
__FAIL:
    if(file){
        fclose(file);
    }
    SDL_Quit();
}

/**
 * Get SDL_AudioFormat
 * @param sample_format         sample format
 * @return SDL_AudioFormat
 */
static SDL_AudioFormat get_sample_format_in_sdl(char *sample_format) {
    if (0 == strcmp(sample_format, U8)) {
        return AUDIO_U8;
    } else if (0 == strcmp(sample_format, S16)) {
        return AUDIO_S16;
    } else if (0 == strcmp(sample_format, S32)) {
        return AUDIO_S32;
    } else if (0 == strcmp(sample_format, FLT)) {
        return AUDIO_F32;
    }
    return AUDIO_S16;
}

/**
 * Callback Method From SDL：To Read Audio Data
 * @param userdata         NULL
 * @param stream           target buffer
 * @param len                target buffer length
 */
static void read_audio_data(void *userdata, Uint8 *stream, int len) {
    if (buffer_left == 0) {
        return;
    }
    size_t stream_len = (size_t)len;
        
    // 初始化目标缓冲区
    SDL_memset(stream, 0, stream_len);
    
    // 计算本次应该传给SDL的数据长度  调用SDL_MixAudio将数据copy到目标缓冲区
    stream_len = (buffer_left > stream_len) ? stream_len : buffer_left;
    SDL_MixAudio(stream, audio_buffer_pos, (Uint32)stream_len, SDL_MIX_MAXVOLUME);
    
    printf("RawAudio: Playing PCM Buffer Size %zu\n", stream_len);
    
    // buffer指针前移  减去本次的数据长度
    audio_buffer_pos += stream_len;
    buffer_left -= stream_len;
}



