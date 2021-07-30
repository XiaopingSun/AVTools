//
//  AACEncoder.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/7/29.
//

#include "AACEncoder.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/frame.h"
#include "libavutil/samplefmt.h"
#include "libavutil/opt.h"
}

static void encode(const char *input_file_url, const char *output_file_url, const int channel_count);
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt);
static int encode_frame(AVCodecContext *context, AVFrame *frame, AVPacket *packet, FILE *output_file, int *packet_index);

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - Source Format: S16 pcm \n  - Target Format: AAC\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -o:   Output File Local Path\n");
    printf("  -c:   Channel Count\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools AACEncoder -i input.pcm -o output.aac -c 2\n\n");
    printf("Get S16 PCM With FFMPEG From Mp4 File:\n\n");
    printf("  ffmpeg -i 1.mp4 -vn -ar 44100 -ac 2 -f s16le s16le.pcm\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void aac_encoder_parse_cmd(int argc, char *argv[]) {
    int option = 0;   // getopt_long的返回值，返回匹配到字符的ascii码，没有匹配到可读参数时返回-1
    const char *input_file_url = NULL;   // 输入文件路径
    const char *output_file_url = NULL;   // 输出文件路径
    int channel_count = 0;
    
    while (EOF != (option = getopt_long(argc, argv, "i:o:c:", tool_long_options, NULL))) {
        switch (option) {
            case '`':
                show_module_help();
                return;
                break;
            case 'i':
                input_file_url = optarg;
                break;
            case 'o':
                output_file_url = optarg;
                break;
            case 'c':
                channel_count = atoi(optarg);
                break;
            case '?':
                printf("Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
                return;
                break;
            default:
                break;
        }
    }
    
    if (NULL == input_file_url || NULL == output_file_url || 0 == channel_count) {
        printf("AACEncoder: Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    encode(input_file_url, output_file_url, channel_count);
}

/**
 * Start Encode
 * @param input_file_url      input pcm file path
 * @param output_file_url     output aac file path
 */
static void encode(const char *input_file_url, const char *output_file_url, const int channel_count) {

    const AVCodec *codec = NULL;
    AVCodecContext *context = NULL;
    AVFrame *frame = NULL;
    AVPacket *packet = NULL;
    
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    
    int ret = 0;
    int frame_buffer_size = 0;
    int packet_index = 0;
    uint8_t *frame_buf = NULL;
    char codec_name[] = "libfdk_aac";
    
    // 打开输入输出文件
    input_file = fopen(input_file_url, "rb");
    if (!input_file) {
        fprintf(stderr, "Could not open input file./n");
        goto __FAIL;
    }
    
    output_file = fopen(output_file_url, "wb+");
    if (!output_file) {
        fprintf(stderr, "Could not open output file./n");
        goto __FAIL;
    }
    
    // 查找编码器 - libfdk_aac
    codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) {
        fprintf(stderr, "Codec '%s' not found\n", codec_name);
        goto __FAIL;
    }

    // 创建编码器上下文
    context = avcodec_alloc_context3(codec);
    if (!context) {
        fprintf(stderr, "Could not allocate audio codec context.\n");
        goto __FAIL;
    }
    
    // 设置码率  默认128kbps
    context->bit_rate = 128000;
    
    // 检查编码器是否支持sample format   fdk_aac只支持AV_SAMPLE_FMT_S16
    context->sample_fmt = AV_SAMPLE_FMT_S16;
    if (!check_sample_fmt(codec, context->sample_fmt)) {
        fprintf(stderr, "Encoder does not support sample format %s.",
                av_get_sample_fmt_name(context->sample_fmt));
        goto __FAIL;
    }
    
    // 设置其他参数
    context->sample_rate = 44100;
    context->channels = channel_count;
    context->channel_layout = av_get_default_channel_layout(channel_count);
    
    // 设置AAC Type
    context->profile = FF_PROFILE_AAC_LOW;
//    context->profile = FF_PROFILE_AAC_HE;
//    context->profile = FF_PROFILE_AAC_HE_V2;
    
    // 打开编码器  传入编码器参数
    if (avcodec_open2(context, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec.\n");
        goto __FAIL;
    }
    
    // 创建AVPacket
    packet = av_packet_alloc();
    if (!packet) {
        fprintf(stderr, "could not allocate the packet.\n");
        goto __FAIL;
    }
    
    // 创建AVFrame
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate audio frame.\n");
        goto __FAIL;
    }
    
    // 设置frame参数
    frame->nb_samples = context->frame_size;
    frame->format = AV_SAMPLE_FMT_S16;
    frame->channel_layout = av_get_default_channel_layout(channel_count);
    
    // 按配置给AVFrame分配buffer
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate audio data buffers.\n");
        goto __FAIL;
    }
    
    // 计算每个frame buffer的大小
    frame_buffer_size = av_samples_get_buffer_size(NULL, channel_count, context->frame_size, context->sample_fmt, 1);
    
    // 分配临时buffer空间
    frame_buf = (uint8_t *)calloc(1, frame_buffer_size);
    
    while (!feof(input_file)) {
        // 读不满一帧数据  进入下一次循环  一般是在文件结尾
        if (fread(frame_buf, 1, frame_buffer_size, input_file) < frame_buffer_size) {
            continue;
        }
        if (av_frame_make_writable(frame)) {
            fprintf(stderr, "AVFrame make writable failed.\n");
            goto __FAIL;
        }
        frame->data[0] = frame_buf;
        
        // 编码
        ret = encode_frame(context, frame, packet, output_file, &packet_index);
        if (ret == -1) {
            goto __FAIL;
        }
    }
    
    // flush
    encode_frame(context, NULL, packet, output_file, &packet_index);
    
    printf("\nAAC Encode Success!\n");
    
__FAIL:
    if (input_file) {
        fclose(input_file);
    }
    
    if (output_file) {
        fclose(output_file);
    }
    
    if (context) {
        avcodec_free_context(&context);
    }
    
    if (frame) {
        av_frame_free(&frame);
    }
    
    if (packet) {
        av_packet_free(&packet);
    }
}

/**
 * Check Sample Fmt Support
 * @param codec      current AVCodec instance
 * @param sample_fmt     target fmt
 * @return bool
 */
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt) {
    const enum AVSampleFormat *p = codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt) {
            return 1;
        }
        p++;
    }
    return 0;
}

/**
 * Encode Frame && Get Packet && Write File
 * @param context      current AVCodec instance
 * @param frame     target fmt
 * @param packet      current AVCodec instance
 * @param output_file     target fmt
 * @param packet_index  use to print index
 * @return -1: error 0: normal
 */
static int encode_frame(AVCodecContext *context, AVFrame *frame, AVPacket *packet, FILE *output_file, int *packet_index) {
    int ret = 0;
    
    ret = avcodec_send_frame(context, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending the frame to the encoder.\n");
        exit(1);
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_packet(context, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame.\n");
            return -1;
        }
        
        fwrite(packet->data, 1, packet->size, output_file);
        printf("Saving Packet %d %d\n", (*packet_index)++, packet->size);
        av_packet_unref(packet);
    }
    
    return 0;
}
