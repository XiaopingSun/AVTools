//
//  Demuxer.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/4/27.
//

#include "Demuxer.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "libavutil/imgutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
#include "CPrint.h"
}

static AVFormatContext *fmt_ctx = NULL;                                             // format上下文   用于解复用
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;                // 解码器上下文   用于decode
static enum AVPixelFormat pix_fmt;                                                      // 视频帧像素格式
static AVStream *video_stream = NULL, *audio_stream = NULL;                 // stream 区分音视频track
static AVPacket *packet = NULL;                                                         // 解码前一帧数据
static AVFrame *frame = NULL;                                                           // 解码后一帧数据

static FILE *video_output_file = NULL, *audio_output_file = NULL;               // 输出文件路径
static int video_width, video_height = 0;                                               // 视频分辨率宽高
static uint8_t *video_dst_data[4] = {NULL};                                           // 视频帧缓冲区
static int video_dst_linesize[4];                                                           // 视频帧缓冲区长度
static int video_dst_bufsize = 0;                                                          // 视频帧数据整体长度

static int video_stream_index = -1, audio_stream_index = -1;                    // 当前解码的stream在AVFormatContext->streams里的index
static int video_frame_count = 0, audio_frame_count = 0;                        // 音视频帧数量

static void demux(const char *input_url, const char *video_output_url, const char *audio_output_url);
static int open_codec_context(AVFormatContext *fmt_ctx, enum AVMediaType type, AVCodecContext **context, int *stream_index);
static int decode_packet(AVCodecContext *context, AVPacket *packet, AVFrame *frame);
static int output_video_frame(AVFrame *frame);
static int output_audio_frame(AVFrame *frame);
static int get_format_from_sample_fmt(const char **fmt, enum AVSampleFormat sample_fmt);

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - FLV\n");
    printf("  - MP4\n");
    printf("  - TS\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -a:   Output Audio File Path\n");
    printf("  -v:   Output Video File Path\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools Demuxer -i input.flv -a output.pcm -v output.yuv\n\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void demuxer_parse_cmd(int argc, char *argv[]) {
    int option = 0;   // getopt_long的返回值，返回匹配到字符的ascii码，没有匹配到可读参数时返回-1
    const char *input_url = NULL;   // 输入文件路径
    const char *video_output_url = NULL;  // 视频数据输出路径
    const char *audio_output_url = NULL;  // 音频数据输出路径
    
    while (EOF != (option = getopt_long(argc, argv, "i:v:a:", tool_long_options, NULL))) {
        switch (option) {
            case '`':
                show_module_help();
                return;
                break;
            case 'i':
                input_url = optarg;
                break;
            case 'v':
                video_output_url = optarg;
                break;
            case 'a':
                audio_output_url = optarg;
                break;
            case '?':
                printf("Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
                return;
                break;
            default:
                break;
        }
    }
    
    if (NULL == input_url || NULL == video_output_url || NULL == audio_output_url) {
        printf("Demuxer Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    demux(input_url, video_output_url, audio_output_url);
}

/**
 * Start Demuxing
 * @param input_url               Input File Path
 * @param video_output_url     Video Output File Path
 * @param audio_output_url     Audio Output File Path
 */
static void demux(const char *input_url, const char *video_output_url, const char *audio_output_url) {
    
    int ret = 0;
        
    // 使用 AVFormatContext 打开输入文件
    if (avformat_open_input(&fmt_ctx, input_url, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", video_output_url);
        goto __END;
    }
    
    // 检索文件信息
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto __END;
    }
    
    // 查找视频 AVStream  初始化解码器上下文
    if (open_codec_context(fmt_ctx, AVMEDIA_TYPE_VIDEO, &video_dec_ctx, &video_stream_index) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_index];
        video_output_file = fopen(video_output_url, "wb+");
        if (!video_output_file) {
            fprintf(stderr, "Could not open destination file %s\n", video_output_url);
            goto __END;
        }
        
        video_width = video_dec_ctx->width;
        video_height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        ret = av_image_alloc(video_dst_data, video_dst_linesize, video_width, video_height, pix_fmt, 1);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate raw video buffer\n");
            goto __END;
        }
        video_dst_bufsize = ret;
    }
    
    // 查找音频 AVStream  初始化解码器上下文
    if (open_codec_context(fmt_ctx, AVMEDIA_TYPE_AUDIO, &audio_dec_ctx, &audio_stream_index) >= 0) {
        audio_stream = fmt_ctx->streams[audio_stream_index];
        audio_output_file = fopen(audio_output_url, "wb+");
        if (!audio_output_file) {
            fprintf(stderr, "Could not open destination file %s\n", audio_output_url);
            goto __END;
        }
    }
    
    // 输出媒体信息
    av_dump_format(fmt_ctx, 0, input_url, 0);
    
    if (!audio_stream && !video_stream) {
        fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
        goto __END;
    }

    // 初始化AVFrame
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        goto __END;
    }
    
    // 初始化AVPacket
    packet = av_packet_alloc();
    if (!packet) {
        fprintf(stderr, "Could not allocate packet\n");
        goto __END;
    }
    
    while (av_read_frame(fmt_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_index) {
            ret = decode_packet(video_dec_ctx, packet, frame);
        } else if (packet->stream_index == audio_stream_index) {
            ret = decode_packet(audio_dec_ctx, packet, frame);
        }
        av_packet_unref(packet);
        if (ret < 0) {
            break;
        }
    }
    
    // flush the decoders
    if (video_dec_ctx) {
        decode_packet(video_dec_ctx, NULL, frame);
    }
    if (audio_dec_ctx) {
        decode_packet(audio_dec_ctx, NULL, frame);
    }
    
    color_print(COLOR_FT_WHITE, COLOR_BG_NONE, "\nDemuxing succeeded.\n");
    
    if (video_stream) {
        printf("Play the output video file with the command:\n"
               "  - ffplay -f rawvideo -pixel_format %s -video_size %dx%d %s\n",
               av_get_pix_fmt_name(pix_fmt), video_width, video_height, video_output_url);
    }
    
    if (audio_stream) {
        enum AVSampleFormat sfmt = av_get_packed_sample_fmt(audio_dec_ctx->sample_fmt);
        int n_channels = audio_dec_ctx->channels;
        const char *fmt = NULL;
        if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0) {
            goto __END;
        }
        printf("Play the output audio file with the command:\n"
               "  - ffplay -f %s -ac %d -ar %d %s\n",
               fmt, n_channels, audio_dec_ctx->sample_rate, audio_output_url);
    }

__END:
    if (video_output_file) {
        fclose(video_output_file);
    }
    
    if (audio_output_file) {
        fclose(audio_output_file);
    }
    
    if (video_dec_ctx) {
        avcodec_free_context(&video_dec_ctx);
    }
    
    if (audio_dec_ctx) {
        avcodec_free_context(&audio_dec_ctx);
    }
    
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }
    
    if (packet) {
        av_packet_free(&packet);
    }
    
    if (frame) {
        av_frame_free(&frame);
    }
        
    av_free(video_dst_data[0]);
}

/**
 * 根据 AVFormatContext 找到音视频的 AVStream 并初始化 AVCodecContext
 * @param fmt_ctx                 AVFormatContext
 * @param type                     媒体类型
 * @param context                 解码器上下文指针的地址
 * @param stream_index         stream 在 AVFormatContext中的下标
 * @return ret
 */
static int open_codec_context(AVFormatContext *fmt_ctx, enum AVMediaType type, AVCodecContext **context, int *stream_index) {
    
    int ret, inner_stream_index = 0;
    AVStream *stream = NULL;
    AVCodec *codec = NULL;
    AVDictionary *opts = NULL;
    
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file.\n", av_get_media_type_string(type));
        return ret;
    } else {
        inner_stream_index = ret;
        stream = fmt_ctx->streams[inner_stream_index];
        
        // 通过 AVStream 的 codec id 查找解码器
        codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!codec) {
            fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        
        // 初始化 codec context
        *context = avcodec_alloc_context3(codec);
        if (!*context) {
            fprintf(stderr, "Failed to allocate the %s codec context\n", av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }
        
        // Copy解码参数  AVStream -> AVCodecContext
        if ((ret = avcodec_parameters_to_context(*context, stream->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(type));
            return ret;
        }
        
        // 根据上下文 context 初始化解码器
        if ((ret = avcodec_open2(*context, codec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
            return ret;
        }
        *stream_index = inner_stream_index;
    }
    return 0;
}

/**
 * AVPacket 解码 AVFrame
 * @param context                 解码器上下文指针的地址
 * @param packet                  AVPacket Instance
 * @param frame                   AVFrame Instance
 * @return ret
 */
static int decode_packet(AVCodecContext *context, AVPacket *packet, AVFrame *frame) {
    
    int ret = 0;
    
    // 将 AVPacket 输入编码器
    ret = avcodec_send_packet(context, packet);
    if (ret < 0) {
        fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
        return ret;
    }
    
    // 尝试从编码器取出解码好的 AVFrame
    while (ret >= 0) {
        ret = avcodec_receive_frame(context, frame);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                return 0;
            
            fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
            return ret;
        }
        
        // 将 AVFrame 写入 output file
        if (context->codec->type == AVMEDIA_TYPE_VIDEO) {
            ret = output_video_frame(frame);
        } else {
            ret = output_audio_frame(frame);
        }
        
        av_frame_unref(frame);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

/**
 * Video Frame 写入本地文件
 * @param frame                   Video Frame
 * @return ret
 */
static int output_video_frame(AVFrame *frame) {
    
    if (frame->width != video_width || frame->height != video_height || frame->format != pix_fmt) {
        /* To handle this change, one could call av_image_alloc again and
         * decode the following frames into another rawvideo file. */
        fprintf(stderr, "Error: Width, height and pixel format have to be "
                "constant in a rawvideo file, but the width, height or "
                "pixel format of the input video changed:\n"
                "old: width = %d, height = %d, format = %s\n"
                "new: width = %d, height = %d, format = %s\n",
                video_width, video_height, av_get_pix_fmt_name(pix_fmt),
                frame->width, frame->height,
                av_get_pix_fmt_name((AVPixelFormat)frame->format));
        return -1;
    }
    
    printf("video_frame n:%d coded_n:%d\n", video_frame_count++, frame->coded_picture_number);
    /* copy decoded frame to destination buffer:
     * this is required since rawvideo expects non aligned data */
    av_image_copy(video_dst_data, video_dst_linesize, (const uint8_t **)(frame->data), frame->linesize, pix_fmt, video_width, video_height);
    
    /* write to rawvideo file */
    fwrite(video_dst_data[0], 1, video_dst_bufsize, video_output_file);
    return 0;
}

/**
 * Audio Frame 写入本地文件
 * @param frame                   Audio Frame
 * @return ret
 */
static int output_audio_frame(AVFrame *frame) {
    
    printf("audio_frame n:%d nb_samples:%d pts:%s\n", audio_frame_count++, frame->nb_samples, av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));
    
    // 获取每个采样的字节大小
    int data_size = av_get_bytes_per_sample((AVSampleFormat)frame->format);
    
    // 判断frame数据保存方式是planer还是packed
    if (av_sample_fmt_is_planar((AVSampleFormat)frame->format)) {
        // planer: 多个channel数据分开存储  frame->data[0]、frame->data[1]...
        for (int i = 0; i < frame->nb_samples; i++) {
            for (int j = 0; j < frame->channels; j++) {
                fwrite(frame->data[j] + data_size * i, 1, data_size, audio_output_file);
            }
        }
    } else {
        // packed: 所有channel在frame->data[0]中交替存储
        fwrite(frame->data[0], 1, data_size * frame->nb_samples * frame->channels, audio_output_file);
    }
    return 0;
}

/**
 * 获取 AVSampleFormat 在ffmpeg命令行中的描述
 * @param fmt                   输出描述字符
 * @param sample_fmt        AVSampleFormat Instance
 * @return ret
 */
static int get_format_from_sample_fmt(const char **fmt, enum AVSampleFormat sample_fmt) {
    
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt;
        const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;
    
    for (int i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }
    
    fprintf(stderr, "sample format %s is not supported as output format\n", av_get_sample_fmt_name(sample_fmt));
    return -1;
}
