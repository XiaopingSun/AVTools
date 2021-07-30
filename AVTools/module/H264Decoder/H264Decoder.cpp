//
//  H264Decoder.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/4/27.
//

#include "H264Decoder.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "libavcodec/avcodec.h"
}

#define INBUF_SIZE  4096

static void decode(const char *input_file_url, const char *output_file_url);
static int decode_packet(AVCodecContext *context, AVFrame *frame, AVPacket *packet, FILE *output_file);
static void save_as_yuv420p(AVFrame *frame, FILE *output_file);

static const char *pic_type[]  = {
                   "NONE",
                   "I",
                   "P",
                   "B",
                   "S",
                   "SI",
                   "SP",
                   "BI"
};

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - Source Format: H264（YUV420P） \n  - Target Format: YUV420P\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -o:   Output File Local Path\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools H264Decoder -i input.h264 -o output.yuv\n\n");
    printf("Get H264 With FFMpeg From Mp4 File:\n\n");
    printf("  ffmpeg -i video.mp4 -c copy -bsf: h264_mp4toannexb -f h264 raw.h264\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void h264_decoder_parse_cmd(int argc, char *argv[]) {
    int option = 0;   // getopt_long的返回值，返回匹配到字符的ascii码，没有匹配到可读参数时返回-1
    const char *input_file_url = NULL;   // 输入文件路径
    const char *output_file_url = NULL;   // 输出文件路径
        
    while (EOF != (option = getopt_long(argc, argv, "i:o:", tool_long_options, NULL))) {
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
            case '?':
                printf("Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
                return;
                break;
            default:
                break;
        }
    }
    
    if (NULL == input_file_url || NULL == output_file_url) {
        printf("H264Decoder: Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    decode(input_file_url, output_file_url);
}

/**
 * Start Decode
 * @param input_file_url     输入文件路径
 * @param output_file_url     输出文件路径
 */
static void decode(const char *input_file_url, const char *output_file_url) {
    
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    
    // 缓冲区buffer
    uint8_t input_buf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    // 滑动指针
    uint8_t *data = NULL;
    size_t data_size = 0;
    int ret = 0;
    
    const AVCodec *codec = NULL;
    AVCodecParserContext* parse_context = NULL;
    AVCodecContext *context = NULL;
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    
    /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    // 后64个字节置为0
    memset(input_buf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    
    // 查找h264编解码器
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "H264 Codec not found\n");
        goto __FAIL;
    }
    
    // 打开输入文件
    input_file = fopen(input_file_url, "rb");
    if (!input_file) {
        fprintf(stderr, "Could not open %s\n", input_file_url);
        goto __FAIL;
    }
    
    // 打开输出文件
    output_file = fopen(output_file_url, "wb+");
    if (!output_file) {
        fprintf(stderr, "Could not open %s\n", output_file_url);
        goto __FAIL;
    }
    
    // 初始化parser
    parse_context = av_parser_init(codec->id);
    if (!parse_context) {
        fprintf(stderr, "parser not found\n");
        goto __FAIL;
    }
    
    // 初始化context
    context = avcodec_alloc_context3(codec);
    if (!context) {
        fprintf(stderr, "Could not allocate video codec context\n");
        goto __FAIL;
    }
    
    // 打开codec
    if (avcodec_open2(context, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        goto __FAIL;
    }
    
    // 初始化AVPacket 存放未解码数据
    packet = av_packet_alloc();
    if (!packet) {
        fprintf(stderr, "av_packet_alloc failed.\n");
        goto __FAIL;
    }
    
    // 初始化AVFrame 存放解码后数据
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        goto __FAIL;
    }
    
    while (!feof(input_file)) {
        
        // 从input_file读取数据到input_buf  每次读INBUF_SIZE  默认4k
        data_size = fread(input_buf, 1, INBUF_SIZE, input_file);
        if (0 == data_size) {
            break;
        }
        
        data = input_buf;
        while (data_size > 0) {
            // parse input buffer into packet    return bytes passed to parser
            ret = av_parser_parse2(parse_context,
                                   context,
                                   &packet->data,
                                   &packet->size,
                                   data,
                                   (int)data_size,
                                   AV_NOPTS_VALUE,
                                   AV_NOPTS_VALUE,
                                   0);
            if (ret < 0) {
                fprintf(stderr, "Error while parsing\n");
                goto __FAIL;
            }
            data += ret;
            data_size -= ret;
            
            // 如果有可用的AVPacket就进行解码操作
            if (packet->size > 0) {
                ret = decode_packet(context, frame, packet, output_file);
                if (ret < 0) {
                    goto __FAIL;
                }
            }
        }
    }
    
    /* flush the decoder */
    decode_packet(context, frame, NULL, output_file);
    
    printf("\nDecode Success!\n");
    printf("Run 'AVTools RawVideo -f YUV420P -r 25 -w %d -h %d -i %s'\n", context->width, context->height, output_file_url);
    
__FAIL:
    
    if (input_file) {
        fclose(input_file);
    }
    
    if (output_file) {
        fclose(output_file);
    }
    
    if (parse_context) {
        av_parser_close(parse_context);
    }
    
    if (context) {
        avcodec_free_context(&context);
    }
    
    if (packet) {
        av_packet_free(&packet);
    }
    
    if (frame) {
        av_frame_free(&frame);
    }
}

/**
 * Decode AVPacket To AVFrame
 * @param context   编码器上下文
 * @param frame    解码后数据
 * @param packet   解码前数据
 * @param output_file   输出文件
 * @return success 0   fail -1
 */
static int decode_packet(AVCodecContext *context, AVFrame *frame, AVPacket *packet, FILE *output_file) {
    
    int ret = 0;
    
    ret = avcodec_send_packet(context, packet);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        return -1;
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_frame(context, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            return -1;
        }
        
        printf("Saving Frame: number %3d   width %4d   height %4d   pix_format %2d   key_frame %d   pic_type %s   coded_picture_number %3d\n", context->frame_number, frame->width, frame->height, frame->format, frame->key_frame, pic_type[frame->pict_type], frame->coded_picture_number);
        fflush(stdout);
        
        save_as_yuv420p(frame, output_file);
    }
    
    return 0;
}

/**
 * Save As YUV420P
 * @param frame   解码后数据
 * @param output_file    输出文件
 */
static void save_as_yuv420p(AVFrame *frame, FILE *output_file) {
    
    // 一帧画面Y分量所占字节数
    size_t size_y = frame->width * frame->height;
    
    // 实际需要的缓冲区大小  YUV420p是1.5倍的像素个数字节大小
    size_t size_buf = frame->width * frame->height * 15 / 10;
    char *frame_buf = (char *)calloc(size_buf, sizeof(char));
    
    // AVFrame保存一帧画面的信息，data[0]存Y分量，data[1]存U分量，data[2]存V分量。其中，图像每一行Y、U、V数据的大小分别是linesize[0]、linesize[1]、linesize[2]，AVFrame->data[x]除了实际的图像数据，还有一些填充数据是不需要的，所以linesize可能大于width。
    for(int i = 0; i < frame->height; i++)
        memcpy(frame_buf + frame->width * i, frame->data[0]  + frame->linesize[0] * i, frame->width);

    for(int j = 0; j < frame->height / 2; j++)
        memcpy(frame_buf + frame->width / 2 * j + size_y, frame->data[1]  + frame->linesize[1] * j, frame->width / 2);

    for(int k = 0; k < frame->height / 2; k++)
        memcpy(frame_buf + frame->width / 2 * k  + size_y * 5 / 4, frame->data[2]  + frame->linesize[2] * k, frame->width / 2);
    
    fwrite(frame_buf, size_buf, 1, output_file);
}
