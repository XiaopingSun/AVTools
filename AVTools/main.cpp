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
#include "H264Decoder.h"
#include "AACDecoder.h"
#include "MP4Demuxer.h"
#include "FLVDemuxer.h"
#include "TSDemuxer.h"
#include "MP4Mediainfo.h"
#include "FLVMediainfo.h"
#include "TSMediainfo.h"
#include "RGBToBMP.h"

// module list
// converter
#define RGB_TO_BMP   "RGBToBMP"


// player
#define RAW_VIDEO      "RawVideo"
#define RAW_AUDIO       "RawAudio"

// decoder
#define H264_DECODER   "H264Decoder"
#define AAC_DECODER     "AACDecoder"

// demuxer
#define MP4_DEMUXER     "MP4Demuxer"
#define FLV_DEMUXER      "FLVDemuxer"
#define TS_DEMUXER       "TSDemuxer"

// mediainfo
#define MP4_MEDIAINFO    "MP4Mediainfo"
#define FLV_MEDIAINFO     "FLVMediainfo"
#define TS_MEDIAINFO      "TSMediainfo"

const char *version = "v1.0.0";

const char *function_list[] = {
    RGB_TO_BMP,
    RAW_VIDEO,
    RAW_AUDIO,
    H264_DECODER,
    AAC_DECODER,
    MP4_DEMUXER,
    FLV_DEMUXER,
    TS_DEMUXER,
    MP4_MEDIAINFO,
    FLV_MEDIAINFO,
    TS_MEDIAINFO
};

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'},
    {"version", no_argument, NULL, ','}
};

static void showToolHelp() {
    printf("AVTools Module List:\n\n");
    printf("    - RawVideo: A Raw Video Format Player. Support RGB And YUV.\n\n");
    printf("    - RGBToBMP: Convert RGB To BMP. Only Support RGB24.\n\n");
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
                raw_to_bmp_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, RAW_VIDEO)) {
                raw_video_parse_cmd(argc, argv);
            } else if (0 == strcmp(arg, RAW_AUDIO)) {
                
            } else if (0 == strcmp(arg, H264_DECODER)) {
                
            } else if (0 == strcmp(arg, AAC_DECODER)) {
                
            } else if (0 == strcmp(arg, MP4_DEMUXER)) {
                
            } else if (0 == strcmp(arg, FLV_DEMUXER)) {
                
            } else if (0 == strcmp(arg, TS_DEMUXER)) {
                
            } else if (0 == strcmp(arg, MP4_MEDIAINFO)) {
                
            } else if (0 == strcmp(arg, FLV_MEDIAINFO)) {
                
            } else if (0 == strcmp(arg, TS_MEDIAINFO)) {
                
            }
        } else {
            printf("no such module %s\n", argv[1]);
        }
    }
    
    return 0;
}
