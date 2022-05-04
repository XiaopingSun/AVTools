//
//  FLVMediainfo.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/4/27.
//

/*
 FLV（Flash Video）是Adobe公司设计开发的一种流行的流媒体格式，由于其视频文件体积轻巧、封装简单等特点，使其很适合在互联网上进行应用。此外，FLV可以使用Flash Player进行播放，而Flash Player插件已经安装在全世界绝大部分浏览器上，这使得通过网页播放FLV视频十分容易。目前主流的视频网站如优酷网，土豆网，乐视网等网站无一例外地使用了FLV格式。FLV封装格式的文件后缀通常为“.flv”。
 
 官方文档：https://rtmp.veriskope.com/pdf/video_file_format_spec_v10.pdf

 FLV包含两部分  FLV Header和FLV Body：
 
  - FLV Header（9字节）：
    1.Signature（3字节） 文件表示  总为"FLV"（0x46, 0x4c, 0x56）
    2.Version（1字节） 版本号   目前为0x01
    3.Flags（1字节） 前5位保留  必须为0   第6位标识是否存在音频Tag   第7位保留  必须为0  第8位标识是否存在视频Tag
    4.Headersize（4字节） 从FLV Header开始到FLV Body开始的字节数  版本1中总是9
 
  - FLV Body：由一系列Tag组成  每个Tag前面还包含一个Previous Tag Size字段（4字节）  表示前面一个Tag的大小
 
 Tag包含两部分  Tag Header和Tag Data：
 
  - Tag Header（11字节）：
    1.Type（1字节） 表示Tag类型  包括音频(0x08)  视频(0x09)和script data(0x12)
    2.Datasize（3字节） 表示该Tag Data部分的大小
    3.Timestamp（3字节） 表示该Tag的时间戳
    4.Timestamp_ex（1字节） 表示时间戳的扩展字节  当24位数值不够时  该字节作为最高位将时间戳扩展为32位数值
    5.StreamID（3字节） 表示stream id  总是0
 
 - Tag Data  Tag数据部分  不同类型Tag的data部分结构各不相同
   Audio Tag：
    音频编码类型（4bit）+ 采样率（2bit）+ 采样精度（1bit）+ 声道类型（1bit）+ *data
   Video Tag：
    视频帧类型（4bit）+ 视频编码类型（4bit）+ *data
   Script Tag：
    该类型Tag又通常被称为Metadata Tag，会放一些关于FLV视频和音频的元数据信息如：duration、width、height等。通常该类型Tag会跟在FLV Header后面作为第一个Tag出现，而且只有一个。
    一般来说，该Tag Data结构包含两个AMF包：
    第一个AMF包（13字节）：
        第1个字节表示AMF包类型，一般总是0x02，表示字符串，其他值表示意义请查阅文档。
        第2-3个字节为UI16类型值，表示字符串的长度，一般总是0x000A（“onMetaData”长度）。
        后面字节为字符串数据，一般总为“onMetaData”。
    第二个AMF包：
        第1个字节表示AMF包类型，一般总是0x08，表示数组。
        第2-5个字节为UI32类型值，表示数组元素的个数。
        后面即为各数组元素的封装，数组元素为元素名称和值组成的对。表示方法如下：
        第1-2个字节表示元素名称的长度，假设为L。
        后面跟着为长度为L的字符串。
        第L+3个字节表示元素值的类型。
        后面跟着为对应值，占用字节数取决于值的类型。（0，Number ；1，Boolean；2，String........8，ECMA array........）
    * 由于Script Tag的metadata涉及多种数据格式，这里暂不做解析
 */

#include "FLVMediainfo.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#define TAG_TYPE_AUDIO  0x08
#define TAG_TYPE_VIDEO  0x09
#define TAG_TYPE_SCRIPT 0x12

#pragma pack(1)
typedef struct {
    uint8_t signature[3];
    uint8_t version;
    uint8_t flags;
    uint32_t header_size;
} FLV_HEADER;

#pragma pack(1)
typedef struct {
    uint8_t type;
    uint8_t data_size[3];
    uint8_t timestamp[3];
    uint8_t timestamp_ex;
    uint8_t streamID[3];
} TAG_HEADER;

static void parse(char *url);
static uint32_t reverse_bytes(uint8_t *p, size_t c);

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - FLV\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools FLVMediainfo -i input.flv\n\n");
    printf("Get FLV With FFMpeg From Mp4 File:\n\n");
    printf("   ffmpeg -i video.mp4 -c copy -f flv input.flv\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void flv_mediainfo_parse_cmd(int argc, char *argv[]) {
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
        printf("FLVMediaInfo Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    parse(url);
}

/**
 * Start Parse
 * @param url     flv file path
 */
static void parse(char *url) {
    
    FILE *input_file = NULL;
    FILE *myout = stdout;
    
    FLV_HEADER flv_header = {};
    TAG_HEADER tag_header = {};
    
    uint32_t previous_tag_size = 0;
    
    input_file = fopen(url, "rb");
    if (!input_file) {
        printf("Failed to open input file!");
        goto __FAIL;
    }
    
    // 读取flv header
    fread((char *)&flv_header, 1, sizeof(FLV_HEADER), input_file);
    
    fprintf(myout,"============== FLV Header ==============\n");
    fprintf(myout,"Signature:  %c %c %c\n", flv_header.signature[0], flv_header.signature[1], flv_header.signature[2]);
    fprintf(myout,"Version:    0x %X\n", flv_header.version);
    fprintf(myout,"Flags  :    0x %X\n", flv_header.flags);
    fprintf(myout,"HeaderSize: 0x %X\n", reverse_bytes((uint8_t *)&flv_header.header_size, sizeof(flv_header.header_size)));
    fprintf(myout,"========================================\n");
    
    // 移动指针到header_size之后
    fseek(input_file, reverse_bytes((uint8_t *)&flv_header.header_size, sizeof(flv_header.header_size)), SEEK_SET);
    
    do {
        // getw()函数用于从流中取得整数，其原型如下： int getw(FILE *stream); ... 【返回值】该函数返回从流中读取的整数。 如果文件结束或出错，则返回EOF。
        // 读取前一个tag大小  4字节
        previous_tag_size = getw(input_file);
        
        // 读取tag header
        fread((char *)&tag_header, 1, sizeof(TAG_HEADER), input_file);
        
        uint32_t tag_header_data_size = reverse_bytes(&tag_header.data_size[0], sizeof(tag_header.data_size));
        uint32_t tag_header_timestamp = reverse_bytes(&tag_header.timestamp[0], sizeof(tag_header.timestamp));
        
        char tag_type_str[10];
        switch (tag_header.type) {
            case TAG_TYPE_AUDIO: sprintf(tag_type_str,"AUDIO"); break;
            case TAG_TYPE_VIDEO: sprintf(tag_type_str,"VIDEO"); break;
            case TAG_TYPE_SCRIPT: sprintf(tag_type_str,"SCRIPT"); break;
            default: sprintf(tag_type_str,"UNKNOWN"); break;
        }
        
        if (feof(input_file)) break;
        
        fprintf(myout,"[%6s] %6d %6d |", tag_type_str, tag_header_data_size,tag_header_timestamp);
        
        // 读取tag data header
        switch (tag_header.type) {
            case TAG_TYPE_AUDIO:
            {
                uint8_t audio_data_first_byte = fgetc(input_file);
                uint32_t x = 0;
                char audio_tag_str[100] = {0};
                
                // 读取音频编码类型（4bit）
                strcat(audio_tag_str, "| ");
                x = audio_data_first_byte & 0xF0;
                x = x >> 4;
                switch (x) {
                    case 0: strcat(audio_tag_str,"Linear PCM, platform endian"); break;
                    case 1: strcat(audio_tag_str,"ADPCM"); break;
                    case 2: strcat(audio_tag_str,"MP3"); break;
                    case 3: strcat(audio_tag_str,"Linear PCM, little endian"); break;
                    case 4: strcat(audio_tag_str,"Nellymoser 16-kHz mono"); break;
                    case 5: strcat(audio_tag_str,"Nellymoser 8-kHz mono"); break;
                    case 6: strcat(audio_tag_str,"Nellymoser"); break;
                    case 7: strcat(audio_tag_str,"G.711 A-law logarithmic PCM"); break;
                    case 8: strcat(audio_tag_str,"G.711 mu-law logarithmic PCM"); break;
                    case 9: strcat(audio_tag_str,"reserved"); break;
                    case 10: strcat(audio_tag_str,"AAC"); break;
                    case 11: strcat(audio_tag_str,"Speex"); break;
                    case 14: strcat(audio_tag_str,"MP3 8-Khz"); break;
                    case 15: strcat(audio_tag_str,"Device-specific sound"); break;
                    default: strcat(audio_tag_str,"UNKNOWN"); break;
                }
                
                // 读取音频采样率（2bit）
                strcat(audio_tag_str, "| ");
                x = audio_data_first_byte & 0x0C;
                x = x >> 2;
                switch (x) {
                    case 0: strcat(audio_tag_str,"5.5-kHz"); break;
                    case 1: strcat(audio_tag_str,"1-kHz"); break;
                    case 2: strcat(audio_tag_str,"22-kHz"); break;
                    case 3: strcat(audio_tag_str,"44-kHz"); break;
                    default: strcat(audio_tag_str,"UNKNOWN"); break;
                }
                
                // 读取音频采样精度（1bit）
                strcat(audio_tag_str, "| ");
                x = audio_data_first_byte & 0x02;
                x = x >> 1;
                switch (x) {
                    case 0: strcat(audio_tag_str,"8Bit"); break;
                    case 1: strcat(audio_tag_str,"16Bit"); break;
                    default: strcat(audio_tag_str,"UNKNOWN"); break;
                }
                
                // 读取声道类型（1bit）
                strcat(audio_tag_str, "| ");
                x = audio_data_first_byte & 0x01;
                switch (x) {
                    case 0: strcat(audio_tag_str,"Mono"); break;
                    case 1: strcat(audio_tag_str,"Stereo"); break;
                    default: strcat(audio_tag_str,"UNKNOWN"); break;
                }
                fprintf(myout,"%s",audio_tag_str);
                
                // 文件seek到Tag末尾
                fseek(input_file, tag_header_data_size - 1, SEEK_CUR);
                break;
            }
                
            case TAG_TYPE_VIDEO:
            {
                uint8_t video_data_first_byte = fgetc(input_file);
                uint8_t x = 0;
                char video_tag_str[100] = {0};
                
                // 读取视频帧类型（4bit）
                strcat(video_tag_str, "| ");
                x = video_data_first_byte & 0xF0;
                x = x >> 4;
                switch (x) {
                    case 1: strcat(video_tag_str,"key frame"); break;
                    case 2: strcat(video_tag_str,"inter frame"); break;
                    case 3: strcat(video_tag_str,"disposable inter frame"); break;
                    case 4: strcat(video_tag_str,"generated keyframe"); break;
                    case 5: strcat(video_tag_str,"video info/command frame"); break;
                    default: strcat(video_tag_str,"UNKNOWN"); break;
                }
                
                // 读取视频编码类型（4bit）
                strcat(video_tag_str, "| ");
                x = video_data_first_byte & 0x0F;
                switch (x) {
                    case 1: strcat(video_tag_str,"JPEG (currently unused)"); break;
                    case 2: strcat(video_tag_str,"Sorenson H.263"); break;
                    case 3: strcat(video_tag_str,"Screen video"); break;
                    case 4: strcat(video_tag_str,"On2 VP6"); break;
                    case 5: strcat(video_tag_str,"On2 VP6 with alpha channel"); break;
                    case 6: strcat(video_tag_str,"Screen video version 2"); break;
                    case 7: strcat(video_tag_str,"AVC"); break;
                    default: strcat(video_tag_str,"UNKNOWN"); break;
                }
                fprintf(myout,"%s",video_tag_str);
                
                // 文件seek到Tag末尾
                fseek(input_file, tag_header_data_size - 1, SEEK_CUR);
                break;
            }
                
            case TAG_TYPE_SCRIPT:
            {
                // 暂不做解析
                // 文件seek到Tag末尾
                fseek(input_file, tag_header_data_size, SEEK_CUR);
                break;
            }
                
            default:
                // 文件seek到Tag末尾
                fseek(input_file, tag_header_data_size, SEEK_CUR);
                break;
        }
        
        fprintf(myout,"\n");
        
    } while (!feof(input_file));
    
__FAIL:
    if (input_file) {
        fclose(input_file);
    }
}

/**
 * reverse_bytes - turn a BigEndian byte array into a LittleEndian integer
 * @param p     integer start pointer
 * @param c     bytes of integer
 * @return       LE integer
 */
static uint32_t reverse_bytes(uint8_t *p, size_t c) {
    uint32_t r = 0;
    for (int i = 0; i < c; i++) {
        r |= *(p + i) << (((c - 1) * 8) - 8 * i);
    }
    return r;
}
