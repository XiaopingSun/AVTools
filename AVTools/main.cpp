//
//  main.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/4/26.
//

#include <iostream>
#include <getopt.h>
#include "RawVideo.h"
#include "RawAudio.h"
#include "Demuxer.h"
#include "MP4Mediainfo.h"
#include "FLVMediainfo.h"
#include "TSMediainfo.h"
#include "RGBToBMP.h"
#include "RGBToYUV.h"
#include "YUVToRGB.h"
#include "YUVSpliter.h"
#include "RGBSpliter.h"
#include "PCMSpliter.h"
#include "PCM16ToPCM8.h"
#include "PCMToWAV.h"
#include "H264Parser.h"
#include "H264Decoder.h"
#include "H264Encoder.h"
#include "AACDecoder.h"
#include "AACEncoder.h"
#include "RTPMediainfo.h"


extern "C" {
#include "AACParser.h"
}

// module list
// converter
#define RGB_TO_BMP   "RGBToBMP"
#define RGB_TO_YUV    "RGBToYUV"
#define YUV_TO_RGB    "YUVToRGB"
#define PCM16_TO_PCM8   "PCM16ToPCM8"
#define PCM_TO_WAV    "PCMToWAV"

// spliter
#define YUV_SPLITER    "YUVSpliter"
#define RGB_SPLITER   "RGBSpliter"
#define PCM_SPLITER   "PCMSpliter"

// player
#define RAW_VIDEO      "RawVideo"
#define RAW_AUDIO       "RawAudio"

// decoder
#define H264_DECODER   "H264Decoder"
#define AAC_DECODER     "AACDecoder"

// demuxer
#define DEMUXER       "Demuxer"

// mediainfo
#define MP4_MEDIAINFO    "MP4Mediainfo"
#define FLV_MEDIAINFO     "FLVMediainfo"
#define TS_MEDIAINFO      "TSMediainfo"
#define RTP_MEDIAINFO    "RTPMediainfo"

// parser
#define H264_PARSER    "H264Parser"
#define AAC_PARSER     "AACParser"

// decoder
#define H264_DECODER   "H264Decoder"
#define AAC_DECODER    "AACDecoder"

// encoder
#define H264_ENCODER   "H264Encoder"
#define AAC_ENCODER     "AACEncoder"

const char *version = "v1.0.0";

const char *function_list[] = {
    YUV_SPLITER,
    RGB_SPLITER,
    PCM_SPLITER,
    RGB_TO_BMP,
    RGB_TO_YUV,
    YUV_TO_RGB,
    PCM16_TO_PCM8,
    PCM_TO_WAV,
    RAW_VIDEO,
    RAW_AUDIO,
    H264_DECODER,
    AAC_DECODER,
    DEMUXER,
    MP4_MEDIAINFO,
    FLV_MEDIAINFO,
    TS_MEDIAINFO,
    H264_PARSER,
    H264_DECODER,
    H264_ENCODER,
    AAC_PARSER,
    AAC_ENCODER,
    AAC_DECODER,
    RTP_MEDIAINFO
};

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'},
    {"version", no_argument, NULL, ','}
};

static void showToolHelp() {
    printf("AVTools Module List:\n\n");
    printf("    - RawVideo: A Raw Video Format Player. Support RGB And YUV.\n\n");
    printf("    - RawAudio: A Raw Audio Format Player. Support PCM.\n\n");
    printf("    - RGBToBMP: Convert RGB To BMP. Only Support RGB24.\n\n");
    printf("    - RGBToYUV: Convert RGB24 To YUV420P.\n\n");
    printf("    - YUVToRGB: Convert YUV420P To RGB24.\n\n");
    printf("    - PCM16ToPCM8: Convert S16 To U8.\n\n");
    printf("    - PCMToWAV: Convert PCM To WAV. Support U8 S16 S32.\n\n");
    printf("    - YUVSpliter: Spliter YUV420P To Y U V.\n\n");
    printf("    - RGBSpliter: Spliter RGB24 To R G B.\n\n");
    printf("    - PCMSpliter: Spliter 2 Channels PCM To Left & Right.\n\n");
    printf("    - H264Parser: H264 Annexb Nalu Parser.\n\n");
    printf("    - H264Decoder: H264 To YUV420P.\n\n");
    printf("    - H264Encoder: YUV420P To H264.\n\n");
    printf("    - AACParser: AAC ADTS Parser.\n\n");
    printf("    - AACEncoder: S16 PCM To AAC.\n\n");
    printf("    - AACDecoder: AAC To FLTP PCM.\n\n");
    printf("    - FLVMediainfo: FLV Parser.\n\n");
    printf("    - MP4Mediainfo: MP4 Parser.\n\n");
    printf("    - TSMediainfo: MP4 Parser.\n\n");
    printf("    - RTPMediainfo: RTP/MPEG2-TS Parser.\n\n");
    printf("    - Demuxer: Media Demuxer.\n\n");
    printf("Use 'AVTools {Mudule_Name} --help' To Show Detail Usage.\n");
}

static void showToolVersion() {
    printf("AVTools Version %s\n", version);
}

static bool checkModuleExist(char *arg) {
    bool isExisted = false;
    for (int i = 0; i < sizeof(function_list) / sizeof(char*); i++) {
        const char *current_module = function_list[i];
        if (0 == strcmp(current_module, arg)) {
            isExisted = true;
            break;
        }
    }
    return isExisted;
}

int main(int argc, char * argv[]) {
    
    int option = 0;
    int argIndex = 0;
    
    if (argc < 2) {
        printf("Use '--help' To Show The Module List.\n");
    } else if (argc == 2) {
        if (checkModuleExist(argv[1])) {
            printf("Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        } else {
            option = getopt_long(argc, argv, "`,", tool_long_options, &argIndex);
            switch (option) {
                case '`':
                    showToolHelp();
                    break;
                case ',':
                    showToolVersion();
                    break;
                case '?':
                    printf("Use '--help' To Show The Module List.\n");
                    break;
                default:
                    printf("Use '--help' To Show The Module List.\n");
                    break;
            }
        }
    } else {
        char *arg = argv[1];
        if (checkModuleExist(arg)) {
            if (0 == strcmp(arg, RGB_TO_BMP)) {
                rgb_to_bmp_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, RGB_TO_YUV)) {
                rgb_to_yuv_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, YUV_TO_RGB)) {
                yuv_to_rgb_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, PCM16_TO_PCM8)) {
                pcm16_to_pcm8_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, PCM_TO_WAV)) {
                pcm_to_wav_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, YUV_SPLITER)) {
                yuv_spliter_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, RGB_SPLITER)) {
                rgb_spliter_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, PCM_SPLITER)) {
                pcm_spliter_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, RAW_VIDEO)) {
                raw_video_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, RAW_AUDIO)) {
                raw_audio_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, H264_PARSER)) {
                h264_parser_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, H264_DECODER)) {
                h264_decoder_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, H264_ENCODER)) {
                h264_encoder_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, AAC_PARSER)) {
                aac_parser_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, AAC_DECODER)) {
                aac_decoder_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, AAC_ENCODER)) {
                aac_encoder_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, DEMUXER)) {
                demuxer_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, MP4_MEDIAINFO)) {
                mp4_mediainfo_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, FLV_MEDIAINFO)) {
                flv_mediainfo_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, TS_MEDIAINFO)) {
                ts_mediainfo_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, RTP_MEDIAINFO)) {
                rtp_parser_parse_cmd(argc, argv);
            }
        } else {
            printf("no such module %s\n", argv[1]);
        }
    }
    
    return 0;
}
