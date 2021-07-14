//
//  H264Encoder.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/7/14.
//

#include "H264Encoder.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
}

static void encode(const char *input_file_url, const char *output_file_url, int width, int height, int fps, int bit_rate);
static int encode_frame(AVCodecContext *context, AVFrame *frame, AVPacket *packet, FILE *output_file);

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - Source Format: YUV420P \n  - Target Format: H264\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -w:   YUV Width\n");
    printf("  -h:   YUV Height\n");
    printf("  -r:   Frame Rate\n");
    printf("  -b:   Bit Rate\n");
    printf("  -o:   Output File Local Path\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools H264Encoder -i input.yuv -w 720 -h 1280 -r 25 -b 1000000 -o output.h264\n\n");
    printf("Get YUV420P With FFMpeg From Mp4 File:\n\n");
    printf("  ffmpeg -i input.mp4 -an -c:v rawvideo -pix_fmt yuv420p output.yuv\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void h264_encoder_parse_cmd(int argc, char *argv[]) {
    int option = 0;   // getopt_long的返回值，返回匹配到字符的ascii码，没有匹配到可读参数时返回-1
    const char *input_file_url = NULL;   // 输入文件路径
    const char *output_file_url = NULL;   // 输出文件路径
    int width = 0, height = 0, fps = 0, bit_rate = 0;
        
    while (EOF != (option = getopt_long(argc, argv, "i:o:w:h:r:b:", tool_long_options, NULL))) {
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
            case 'w':
                width = atoi(optarg);
                break;
            case 'h':
                height = atoi(optarg);
                break;
            case 'r':
                fps = atoi(optarg);
                break;
            case 'b':
                bit_rate = atoi(optarg);
                break;
            case '?':
                printf("Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
                return;
                break;
            default:
                break;
        }
    }
    
    if (NULL == input_file_url || NULL == output_file_url || 0 == width || 0 == height) {
        printf("H264Decoder: Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    fps = fps == 0 ? 25 : fps;
    bit_rate = bit_rate == 0 ? 1000000 : bit_rate;
    encode(input_file_url, output_file_url, width, height, fps, bit_rate);
}

/**
 * Start Encode
 * @param input_file_url     输入文件路径
 * @param output_file_url     输出文件路径
 * @param width      视频图像的宽
 * @param height     视频图像的高
 * @param fps       帧率
 * @param bit_rate     码率
 */
static void encode(const char *input_file_url, const char *output_file_url, int width, int height, int fps, int bit_rate) {
    
    const AVCodec *codec = NULL;
    AVCodecContext *context = NULL;
    AVFrame *frame = NULL;
    AVPacket *packet = NULL;
    
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    int ret = 0;
    int frame_index = 0;
    size_t bytes_per_frame = 0;
    size_t bytes_read = 0;
    char codec_name[] = "libx264";
    char *buffer = NULL;
    
    // 打开输入文件
    input_file = fopen(input_file_url, "rb");
    if (!input_file) {
        fprintf(stderr, "Could not open input file\n");
        goto __FAIL;
    }
    
    // 打开输出文件
    output_file = fopen(output_file_url, "wb+");
    if (!output_file) {
        fprintf(stderr, "Could not open output file\n");
        goto __FAIL;
    }
    
    // 查找编码器 - libx264
    codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) {
        fprintf(stderr, "Codec '%s' not found\n", codec_name);
        goto __FAIL;
    }
    
    // 创建编码器上下文
    context = avcodec_alloc_context3(codec);
    if (!context) {
        fprintf(stderr, "Could not allocate video codec context\n");
        goto __FAIL;
    }
    
    // 创建AVPacket
    packet = av_packet_alloc();
    if (!packet) {
        fprintf(stderr, "Could not allocate packet\n");
        goto __FAIL;
    }
    
    // 创建AVFrame
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        goto __FAIL;
    }
    
    // 配置编码器参数
    context->bit_rate = bit_rate;
    context->width = width;
    context->height = height;
    context->time_base = (AVRational){1, fps};
    context->framerate = (AVRational){fps, 1};
    context->gop_size = 3;
    context->max_b_frames = 1;
    context->pix_fmt = AV_PIX_FMT_YUV420P;
    
    // 设置h264的私有属性
    if (codec->id == AV_CODEC_ID_H264)
        av_opt_set(context->priv_data, "preset", "slow", 0);
        av_opt_set(context->priv_data, "profile", "main", 0);
        av_opt_set(context->priv_data, "level", "3.1", 0);
    
    // 打开编码器
    ret = avcodec_open2(context, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        goto __FAIL;
    }
    
    // 设置frame参数
    frame->format = context->pix_fmt;
    frame->width = context->width;
    frame->height = context->height;
    
    // frame分配内存
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        goto __FAIL;
    }
    
    // 计算yuv420p一帧的字节大小
    bytes_per_frame = frame->width * frame->height * 15 / 10;
    buffer = (char *)calloc(bytes_per_frame, sizeof(char));

    // 编码
    while (!feof(input_file)) {
        bytes_read = fread(buffer, 1, bytes_per_frame, input_file);
        if (bytes_read != bytes_per_frame) {
            // YUV文件没对齐  跳出循环
            break;
        }
        
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            fprintf(stderr, "Could not writable the frame\n");
            goto __FAIL;
        }
        
        // Y
        for (int i = 0; i < context->height; i++) {
            for (int j = 0; j < context->width; j++) {
                frame->data[0][i * frame->linesize[0] + j] = buffer[i * context->width + j];
            }
        }
        
        // U
        for (int i = 0; i < context->height / 2; i++) {
            for (int j = 0; j < context->width / 2; j++) {
                frame->data[1][i * frame->linesize[1] + j] = buffer[context->width * context->height + i * context->width / 2 + j];
            }
        }
        
        // V
        for (int i = 0; i < context->height / 2; i++) {
            for (int j = 0; j < context->width / 2; j++) {
                frame->data[2][i * frame->linesize[2] + j] = buffer[context->width * context->height * 5 / 4 + i * context->width / 2 + j];
            }
        }
        
        frame->pts = frame_index++;
        encode_frame(context, frame, packet, output_file);
    }
    
    /* flush the encoder */
    encode_frame(context, NULL, packet, output_file);
    
    printf("\nEncode Success!\n\n");
    
__FAIL:
    if (input_file)
        fclose(input_file);
        
    if (output_file)
        fclose(output_file);
    
    if (context)
        avcodec_free_context(&context);
    
    if (packet)
        av_packet_free(&packet);
    
    if (frame)
        av_frame_free(&frame);
}
    
/**
 * Encode Frame
 * @param context     编码器上下文
 * @param frame     编码前数据
 * @param packet      编码后容器
 * @param output_file     输出文件
 * @return success 0   fail -1
 */
static int encode_frame(AVCodecContext *context, AVFrame *frame, AVPacket *packet, FILE *output_file) {
    
    int ret;

    /* send the frame to the encoder */
    if (frame)
        printf("Send frame %3" PRId64"\n", frame->pts);
    
    ret = avcodec_send_frame(context, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        return -1;
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_packet(context, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            return -1;
        }
        
        printf("Write packet %3" PRId64" (size=%5d)\n", packet->pts, packet->size);
        fwrite(packet->data, packet->size, 1, output_file);
        av_packet_unref(packet);
    }
    
    return 0;
}

