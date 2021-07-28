//
//  H264Parser.c
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/6/29.
//

#include "H264Parser.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

// Annex B格式的NALU结构：start code (3或4Byte) + nalu header (1Byte) + nalu payload

/*
 NALU Header：forbidden_zero_bit (1bit) + nal_ref_idc (2bit) + nal_unit_type (5bit)
 1、forbidden_zero_bit：
禁止位，初始为0，当网络发现NAL单元有比特错误时可设置该比特为1，以便接收方纠错或丢掉该单元。
 2、nal_ref_idc：
 nal重要性指示，标志该NAL单元的重要性，值越大，越重要，解码器在解码处理不过来的时候，可以丢掉重要性为0的NALU。
 3、nal_unit_type：NALU类型，参考NaluType
 */

#define MAX_BUFFER_LEN  100 * 1024    // 100KB

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

typedef enum {
    NALU_TYPE_UNKNOWN    = 0,   // 位置
    NALU_TYPE_SLICE          = 1,   // 非IDR图像中不采用数据划分的片段
    NALU_TYPE_DPA            = 2,   // 非IDR图像中A类数据划分片段
    NALU_TYPE_DPB            = 3,   // 非IDR图像中B类数据划分片段
    NALU_TYPE_DPC            = 4,   // 非IDR图像中C类数据划分片段
    NALU_TYPE_IDR             = 5,   // IDR图像的片
    NALU_TYPE_SEI             = 6,   // 补充图像信息单元
    NALU_TYPE_SPS            = 7,   // 序列参数集
    NALU_TYPE_PPS            = 8,   // 图像参数集
    NALU_TYPE_AUD            = 9,   // 分界符
    NALU_TYPE_EOSEQ        = 10,  // 序列结束
    NALU_TYPE_EOSTREAM  = 11,   // 码流结束
    NALU_TYPE_FILL            = 12,   // 保留
} NaluType;

typedef enum {
    NALU_PRIORITY_DISPOSABLE = 0,  // 可遗弃
    NALU_PRIORITY_LOW            = 1,  // 低优先级
    NALU_PRIORITY_HIGH           = 2,  // 高优先级
    NALU_PRIORITY_HIGHEST     = 3,   // 最高优先级
} NaluPriority;

typedef struct {
    unsigned int start_code_prefix_len;    // start code字节长度
    unsigned int len;                             // NALU 字节长度  不含start code
    int forbidden_bit;                            // NALU Header字段  禁止位
    int nal_reference_idc;                      // NALU Header字段  重要性标识
    int nal_unit_type;                            // NALU Header字段  类型
    char *buf;                                      // NALU BUFFER
} NALU_t;

static void parse(char *url);
static int get_annexb_nalu(NALU_t *nalu, FILE *h264_bit_stream);
static int find_start_code_type1(unsigned char *buf);
static int find_start_code_type2(unsigned char *buf);

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - H264 Annexb\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools H264Parser -i input.h264\n\n");
    printf("Get Raw H264 With FFMpeg From Mp4 File:\n\n");
    printf("   ffmpeg -i video.mp4 -c copy -bsf: h264_mp4toannexb -f h264 raw.h264\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void h264_parser_parse_cmd(int argc, char *argv[]) {
    
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
        printf("H264Parser Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    parse(url);
}

/**
 * Parse
 * @param url    h264 file path
 */
static void parse(char *url) {
    
    NALU_t *nalu = NULL;                         // NALU实例
    FILE *h264_bit_stream = NULL;           // h264输入文件
    FILE *myout = stdout;                         // 标准输出
    int nal_num = 0;                                // NALU数量
    int nal_length = 0;                             // NALU字节长度  包含start code
    int nal_offset = 0;                           // 每个NALU之间的偏移量
    char type_str[20] = {0};                   // NALU TYPE
    char idc_str[20] = {0};                     // NALU IDC

    // 打开输入文件
    h264_bit_stream = fopen(url, "rb+");
    if (h264_bit_stream == NULL) {
        printf("Open File Error.\n");
        return;
    }
    
    // 初始化NALU实例
    nalu = (NALU_t *)calloc(1, sizeof(NALU_t));
    if (nalu == NULL) {
        printf("Alloc Nalu Error.\n");
        return;
    }
    
    // 给NALU的buffer分配空间  默认100KB
    nalu->buf = (char *)calloc(MAX_BUFFER_LEN, sizeof(char));
    if (nalu->buf == NULL) {
        free(nalu);
        printf("Alloc Nalu Buffer Error.\n");
        return;
    }
    
    printf("-----+-------- NALU Table ------+---------+\n");
    printf(" NUM |    POS  |    IDC |  TYPE |   LEN   |\n");
    printf("-----+---------+--------+-------+---------+\n");

    while (!feof(h264_bit_stream)) {
        // 读取NALU信息  返回单元长度
        if ((nal_length = get_annexb_nalu(nalu, h264_bit_stream)) == 0) {
            break;
        }
        switch (nalu->nal_unit_type) {
            case NALU_TYPE_UNKNOWN: sprintf(type_str,"UNKNOWN"); break;
            case NALU_TYPE_SLICE: sprintf(type_str,"SLICE"); break;
            case NALU_TYPE_DPA: sprintf(type_str,"DPA"); break;
            case NALU_TYPE_DPB: sprintf(type_str,"DPB"); break;
            case NALU_TYPE_DPC: sprintf(type_str,"DPC"); break;
            case NALU_TYPE_IDR: sprintf(type_str,"IDR"); break;
            case NALU_TYPE_SEI: sprintf(type_str,"SEI"); break;
            case NALU_TYPE_SPS: sprintf(type_str,"SPS"); break;
            case NALU_TYPE_PPS: sprintf(type_str,"PPS"); break;
            case NALU_TYPE_AUD: sprintf(type_str,"AUD"); break;
            case NALU_TYPE_EOSEQ: sprintf(type_str,"EOSEQ"); break;
            case NALU_TYPE_EOSTREAM: sprintf(type_str,"EOSTREAM"); break;
            case NALU_TYPE_FILL: sprintf(type_str,"FILL"); break;
            default:
                sprintf(type_str,"");
                break;
        }
        
        switch (nalu->nal_reference_idc >> 5) {  // eg: 0x01100000 => 0x00000011
            case NALU_PRIORITY_DISPOSABLE: sprintf(idc_str,"DISPOS"); break;
            case NALU_PRIORITY_LOW: sprintf(idc_str,"LOW"); break;
            case NALU_PRIORITY_HIGH: sprintf(idc_str,"HIGH"); break;
            case NALU_PRIORITY_HIGHEST: sprintf(idc_str,"HIGHEST"); break;
            default:
                sprintf(idc_str,"");
                break;
        }
        
        fprintf(myout, "%5d| %8d| %7s| %6s| %8d|\n", nal_num, nal_offset, idc_str, type_str, nalu->len);
        nal_offset = nal_offset + nal_length;
        nal_num++;
    }
    
    if (nalu) {
        if (nalu->buf) {
            free(nalu->buf);
            nalu->buf = NULL;
        }
        free(nalu);
        nalu = NULL;
    }
}

/**
 * Get Nalu Info
 * @param nalu    current instance of NALU_t
 * @param h264_bit_stream   input file
 */
static int get_annexb_nalu(NALU_t *nalu, FILE *h264_bit_stream) {
    
    int current_pos = 0;                   // 当前指针滑块位置
    int rewind = 0;                          // 需要回退文件指针的字节数
    unsigned char *buf = NULL;          // buffer缓冲区
    bool is_start_code_type1 = false;   // start code匹配0x000001
    bool is_start_code_type2 = false;    // start code匹配0x00000001
    bool next_start_code_found = false;       // 是否找到下一个start code
    
    // 临时buffer缓冲区分配内存
    if ((buf = (unsigned char *)calloc(MAX_BUFFER_LEN, sizeof(char))) == NULL) {
        printf("Get Annexb Nalu: Could Not Allocate Buffer Memory.\n");
        return 0;
    }
    
    // 读取前三个字节
    if (3 != fread(buf, 1, 3, h264_bit_stream)) {
        free(buf);
        printf("Get Annexb Nalu: Could Not Read Three Byte From Input File.\n");
        return 0;
    }
    
    // start code type 1？
    is_start_code_type1 = find_start_code_type1(buf);
    if (is_start_code_type1) {
        current_pos = 3;
        nalu->start_code_prefix_len = 3;
    } else {
        // 如果不是  读取第四个字节
        if (1 != fread(buf + 3, 1, 1, h264_bit_stream)) {
            free(buf);
            printf("Get Annexb Nalu: Could Not Read A Byte From Input File.\n");
            return 0;
        }
        // start code type 2？
        is_start_code_type2 = find_start_code_type2(buf);
        if (is_start_code_type2) {
            current_pos = 4;
            nalu->start_code_prefix_len = 4;
        } else {
            free(buf);
            printf("Get Annexb Nalu: Could Not Find Start Code.\n");
            return 0;
        }
    }
    
    // bool置0
    is_start_code_type1 = false;
    is_start_code_type2 = false;
    
    // 寻找下一个start code
    while (!next_start_code_found) {
        if (feof(h264_bit_stream)) {
            // 文件读完  保存最后一个NALU信息
            nalu->len = (current_pos - 1) - nalu->start_code_prefix_len;
            memcpy(nalu->buf, &buf[nalu->start_code_prefix_len], nalu->len);
            nalu->forbidden_bit = nalu->buf[0] & 0x80;           // 0x10000000
            nalu->nal_reference_idc = nalu->buf[0] & 0x60;     // 0x01100000
            nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;          // 0x00011111
            free(buf);
            return current_pos - 1;
        }
        // 每次从文件读取一个字节  判断是否是start code
        buf[current_pos++] = fgetc(h264_bit_stream);
        is_start_code_type2 = find_start_code_type2(&buf[current_pos - 4]);
        if (!is_start_code_type2) {
            is_start_code_type1 = find_start_code_type1(&buf[current_pos - 3]);
        }
        next_start_code_found = is_start_code_type1 || is_start_code_type2;
    }
    
    // 将文件指针seek回第二个start code之前，用于下一个NALU的计算
    rewind = is_start_code_type1 ? -3 : -4;
    if (0 != fseek(h264_bit_stream, rewind, SEEK_CUR)) {
        free(buf);
        printf("Get Annex Nalu: Could Not Fseek In The Bit Stream File.\n");
        return 0;
    }
    
    // 保存当前NALU信息
    nalu->len = (current_pos + rewind) - nalu->start_code_prefix_len;
    memcpy(nalu->buf, &buf[nalu->start_code_prefix_len], nalu->len);
    nalu->forbidden_bit = nalu->buf[0] & 0x80;          // 0x10000000
    nalu->nal_reference_idc = nalu->buf[0] & 0x60;    // 0x01100000
    nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;         // 0x00011111
    free(buf);
    
    return current_pos + rewind;
}

/**
 * Find Start Code With 0x000001
 * @param buf    指针上下文
 */
static int find_start_code_type1(unsigned char *buf) {
    if (buf[0] != 0 || buf[1] != 0 || buf[2] != 1) {
        return 0;
    } else {
        return 1;
    }
}

/**
 * Find Start Code With 0x00000001
 * @param buf    指针上下文
 */
static int find_start_code_type2(unsigned char *buf) {
    if (buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 1) {
        return 0;
    } else {
        return 1;
    }
}
