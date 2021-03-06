//
//  AACDecoder.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/4/27.
//

#include "AACDecoder.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "libavcodec/avcodec.h"
}

#define INBUF_SIZE  20480
#define AUDIO_REFILL_THRESH 4096

static void decode(const char *input_file_url, const char *output_file_url);
static int decode_packet(AVCodecContext *context, AVFrame *frame, AVPacket *packet, FILE *output_file);
static void save_as_aac(AVCodecContext *context, AVFrame *frame, FILE *output_file);
static int get_format_from_sample_fmt(const char **fmt, AVSampleFormat sample_fmt);

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

const static char *sample_formats[] = {
    "AV_SAMPLE_FMT_NONE",
    "AV_SAMPLE_FMT_U8",         ///< unsigned 8 bits
    "AV_SAMPLE_FMT_S16",         ///< signed 16 bits
    "AV_SAMPLE_FMT_S32",         ///< signed 32 bits
    "AV_SAMPLE_FMT_FLT",         ///< float
    "AV_SAMPLE_FMT_DBL",         ///< double

    "AV_SAMPLE_FMT_U8P",         ///< unsigned 8 bits, planar
    "AV_SAMPLE_FMT_S16P",        ///< signed 16 bits, planar
    "AV_SAMPLE_FMT_S32P",        ///< signed 32 bits, planar
    "AV_SAMPLE_FMT_FLTP",        ///< float, planar
    "AV_SAMPLE_FMT_DBLP",        ///< double, planar
    "AV_SAMPLE_FMT_S64",         ///< signed 64 bits
    "AV_SAMPLE_FMT_S64P",        ///< signed 64 bits, planar

    "AV_SAMPLE_FMT_NB"           ///< Number of sample formats. DO NOT USE if linking dynamically
};

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - Source Format: AAC \n  - Target Format: FLTP PCM\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -o:   Output File Local Path\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools AACDecoder -i input.aac -o output.pcm\n\n");
    printf("Get AAC With FFMpeg From Mp4 File:\n\n");
    printf("   ffmpeg -i video.mp4 -vn -acodec copy raw.aac\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void aac_decoder_parse_cmd(int argc, char *argv[]) {
    int option = 0;   // getopt_long???????????????????????????????????????ascii??????????????????????????????????????????-1
    const char *input_file_url = NULL;   // ??????????????????
    const char *output_file_url = NULL;   // ??????????????????
        
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
        printf("AACDecoder: Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    decode(input_file_url, output_file_url);
}

/**
 * Start Decode
 * @param input_file_url     ??????????????????
 * @param output_file_url     ??????????????????
 */
static void decode(const char *input_file_url, const char *output_file_url) {
    
    const AVCodec *codec = NULL;
    AVCodecContext *context = NULL;
    AVCodecParserContext *parser = NULL;
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    AVSampleFormat sample_format;
    
    FILE *input_file = NULL;
    FILE *output_file = NULL;
    
    const char *fmt = NULL;
    int ret = 0;
    size_t data_size = 0;
    
    // ?????????buffer
    uint8_t input_buf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    // ????????????
    uint8_t *data = NULL;
    
    // ????????????????????????
    input_file = fopen(input_file_url, "rb");
    if (!input_file) {
        fprintf(stderr, "Could not open input file %s.\n", input_file_url);
        goto __FAIL;
    }
    
    output_file = fopen(output_file_url, "wb+");
    if (!output_file) {
        fprintf(stderr, "Could not open output file %s.\n", output_file_url);
        goto __FAIL;
    }
    
    // ??????fdk_aac?????????
    codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    if (!codec) {
        fprintf(stderr, "Codec not found.\n");
        goto __FAIL;
    }
    
    // ?????????parser
    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "Parser not found.\n");
        goto __FAIL;
    }
    
    // ???????????????????????????
    context = avcodec_alloc_context3(codec);
    if (!context) {
        fprintf(stderr, "Could not allocate audio codec context.\n");
        goto __FAIL;
    }
    
    // ?????????AVPacket
    packet = av_packet_alloc();
    if (!packet) {
        fprintf(stderr, "Could not allocate AVPacket instance.\n");
        goto __FAIL;
    }
    
    // ?????????AVFrame
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate AVFrame instance.\n");
        goto __FAIL;
    }
    
    // ???????????????
    ret = avcodec_open2(context, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec.\n");
        goto __FAIL;
    }
    
    while (!feof(input_file)) {
        
        // ???input_file???????????????input_buf  ?????????INBUF_SIZE   ??????20k
        data_size = fread(input_buf, 1, INBUF_SIZE, input_file);
        if (0 == data_size) {
            break;
        }
        
        data = input_buf;
        while (data_size > 0) {
            ret = av_parser_parse2(parser, context, &packet->data, &packet->size, data, (int)data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (ret < 0) {
                fprintf(stderr, "Error while parsing.\n");
                goto __FAIL;
            }
            data += ret;
            data_size -= ret;
            
            // ??????????????????AVPacket?????????????????????
            if (packet->size > 0) {
                ret = decode_packet(context, frame, packet, output_file);
                if (ret < 0) {
                    goto __FAIL;
                }
            }
            
            // ????????????data_size??????4KB  ???????????????seek?????????data??????????????????INBUF_SIZE
            if (data_size > 0 && data_size < AUDIO_REFILL_THRESH) {
                fseek(input_file, -data_size, SEEK_CUR);
                break;
            }
        }
    }
    
    // flush
    decode_packet(context, frame, NULL, output_file);
    
    sample_format = context->sample_fmt;
    // ?????????planer  ???????????????packed???format??????
    if (av_sample_fmt_is_planar(sample_format)) {
        sample_format = av_get_packed_sample_fmt(sample_format);
    }
    
    if (get_format_from_sample_fmt(&fmt, sample_format) < 0) {
        goto __FAIL;
    }
    
    printf("\nDecode Success!\n");
    printf("Run 'ffplay -f %s -ac %d -ar %d %s'\n", fmt, context->channels, context->sample_rate, output_file_url);
    
__FAIL:
    if (input_file) {
        fclose(input_file);
    }
    
    if (output_file) {
        fclose(output_file);
    }
    
    if (parser) {
        av_parser_close(parser);
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
 * @param context   ??????????????????
 * @param frame    ???????????????
 * @param packet   ???????????????
 * @param output_file   ????????????
 * @return success 0   fail -1
 */
static int decode_packet(AVCodecContext *context, AVFrame *frame, AVPacket *packet, FILE *output_file) {
    
    int ret = 0;
    
    // AVPacket ???????????????
    ret = avcodec_send_packet(context, packet);
    if (ret < 0) {
        fprintf(stderr, "Error submitting the packet to the decoder.\n");
        return -1;
    }
    
    while (ret >= 0) {
        // ???????????????????????????
        ret = avcodec_receive_frame(context, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            fprintf(stderr, "Error during decoding.\n");
            return -1;
        }
        
        printf("Saving Frame: number %6d   frame_size  %d   sample_rate  %d   sample_format %s  channel_count %d\n", context->frame_number, context->frame_size, context->sample_rate, sample_formats[context->sample_fmt + 1], context->channels);
        // ????????????
        save_as_aac(context, frame, output_file);
    }
    
    return 0;
}

/**
 * Save As AAC
 * @param context   ???????????????
 * @param frame   ???????????????
 * @param output_file    ????????????
 */
static void save_as_aac(AVCodecContext *context, AVFrame *frame, FILE *output_file) {
    
    // ?????????????????????????????????
    int data_size = av_get_bytes_per_sample(context->sample_fmt);
    
    // ??????frame?????????????????????planer??????packed
    if (av_sample_fmt_is_planar(context->sample_fmt)) {
        // planer: ??????channel??????????????????  frame->data[0]???frame->data[1]...
        for (int i = 0; i < frame->nb_samples; i++) {
            for (int j = 0; j < context->channels; j++) {
                fwrite(frame->data[j] + data_size * i, 1, data_size, output_file);
            }
        }
    } else {
        // packed: ??????channel???frame->data[0]???????????????
        fwrite(frame->data[0], 1, data_size * frame->nb_samples * frame->channels, output_file);
    }
}

/**
 * Get Format Description
 * @param fmt   output format description
 * @param sample_fmt   source format
 * @return 0: success  -1: no such format for ffplay
 */
static int get_format_from_sample_fmt(const char **fmt, AVSampleFormat sample_fmt) {
    
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
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
