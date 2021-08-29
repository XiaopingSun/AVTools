//
//  RTPMediainfo.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/8/28.
//

/*
 RTP（Real-time Transport Protocal，实时传输协议）是一种裕兴在 OSI 应用层的协议，通常基于 UDP 协议，但也支持 TCP 协议。他提供了端到端的实时传输数据的功能，但不包含资源与留存、不保证实时传输质量。
 RTP 协议分为两种子协议，分别是 RTP Data Transfer Protocal 和 RTP Control Protocal。前者用来传输实时数据，后者则是 RTCP，可以提供实时传输过程中的统计信息（如网络延迟、丢包率等）。
 
 RTP 包分为两部分，分别是 header 和 payload，header 包含了实时音视频的同步信息，payload 则承载了具体的音视频数据。
 RTP Header 最小为 12 字节，可能包含可选字段，字段定义如下：
        字段名                                           比特大小                                     描述
       Version                                                2                        表示 RTP 协议的版本，目前版本是 2
     P（Padding）                                          1                    表示 RTP 包末尾是否有 Padding Bytes，且 Padding Bytes 的最后一个 Byte 表示 Byte。
                                                                                        Padding可以被用来填充数据块，比如加密算法可能会用到。
    X（Extension）                                        1                    表示是否有头部扩展，头部扩展可以用来存储信息，比如视频旋转角度。
  CC（CSRC Count）                                     4                    表示下文的 CSRC 数量，最多只能有15个 CSRC。
     M（Marker）                                          1                    表示当前数据是否与应用程序有某种特殊的相关性。
                                                                                        比如传输的一些私有数据，或者数据中的某些标志位具有特殊的作用。
 PT（Payload Type）                                    7                    表示 Payload 的数据类型，音视频的默认映射格式参考 https://datatracker.ietf.org/doc/html/rfc3551#page-32
  Sequence Number                                      16                    递增的序列号，用于标记每一个被发送的 RTP 包，接收方可以根据序列号按顺序重新组包，以及识别是否有丢包。
                                                                                        序列号的初始值应当是随机的（不可预测的），从而增加明文攻击。
      Timestamp                                           32                    时间戳，接收方根据其来回放音视频。这里时间戳的初始值也是随机选取的，是一种相对时间戳。
                                                                                        时间戳的间隔由传输的数据类型（或具体的应用场景）确定，比如音频通常以125微秒（8kHz）的时钟频率进行采样，而视频则以90kHz的时钟频率采样。
 SSRC (Synchronization source)                     32                    同步源标识符，相同 RTP 会话中的 SSRC 是唯一的，且生成的 SSRC 也需要保持随机。
                                                                                       尽管多个源选中同一个标识符的概率很低，但具体实现时仍然需要避免这种情况发生，避免碰撞。
 CSRC (Contributing source)                     可选  CC * 4            在 MCU 混流时使用，表示混流出的新的音视频流的 SSRC 是由哪些源 SSRC 贡献的。
 Extension Header                                      可选                   头部扩展，包含了音视频的一些额外信息，比如视频旋转角度。
 
 *注：程序只分析不含可选字段的 RTP 包。
 */

#include "RTPMediainfo.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "CPrint.h"

#define LOCAL_IP_ADDR "127.0.0.1"
#define RECV_BUFFER_SIZE 10240

#pragma pack(1)
typedef struct RTP_FIXED_HEADER {
    /* Byte 0*/
    unsigned char CC: 4;
    unsigned char X: 1;
    unsigned char P: 1;
    unsigned char Version: 2;
    /* Byte 1*/
    unsigned char PT: 7;
    unsigned char M: 1;
    /* Byte 2, 3*/
    unsigned short Seq_No;
    /* Byte 4 - 7*/
    uint32_t Timestamp;
    /* Byte 8 - 11*/
    uint32_t SSRC;
} RTP_FIXED_HEADER;

typedef struct RTCP_FIXED_HEADER {
    /* Byte 0*/
    unsigned char RC: 5;
    unsigned char P: 1;
    unsigned char Version: 2;
    /* Byte 1*/
    unsigned char PT;
    /* Byte 2, 3*/
    unsigned short length;
} RTCP_FIXED_HEADER;

static void parse(short port, const char *output_url);

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - RTP/MPEG2-TS\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -p:   Listen Port\n");
    printf("  -o:   Output Path\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools RTPMediainfo -p 8081 -o output.ts\n\n");
    printf("Push RTP/MPEG2-TS With FFMPEG:\n\n");
    printf("  ffmpeg -re -i input.ts -f rtp_mpegts udp://127.0.0.1:8081\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void rtp_parser_parse_cmd(int argc, char *argv[]) {
    int option = 0;   // getopt_long的返回值，返回匹配到字符的ascii码，没有匹配到可读参数时返回-1
    short port = 8081;   // 默认监听 8081 端口
    const char *output_url = NULL;  // 输出路径
    
    while (EOF != (option = getopt_long(argc, argv, "p:o:", tool_long_options, NULL))) {
        switch (option) {
            case '`':
                show_module_help();
                return;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'o':
                output_url = optarg;
                break;
            case '?':
                printf("Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
                return;
                break;
            default:
                break;
        }
    }
    
    if (output_url == NULL) {
        printf("RTPMediaInfo Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    parse(port, output_url);
}

/**
 * Parse
 * @param port            Listen Port
 * @param output_url    Output File Path
 */
static void parse(short port, const char *output_url) {
    
    int udp_server_sock = 0;
    struct sockaddr_in loc_addr = {};
    struct sockaddr_in remote_addr = {};
    socklen_t remote_addr_len = 0;
    
    int ret = 0;
    int index = 0;
    FILE *output_file = NULL;
    FILE *myout = stdout;
    
    unsigned char *recv_data = NULL;
    char payload_type_str[10] = {0};
    
    RTP_FIXED_HEADER rtp_header = {};
    RTCP_FIXED_HEADER rtcp_header = {};
    
    size_t rtp_header_len = 0;
    size_t rtcp_header_len = 0;
    
        
    // 创建套接字  指定类型为ipv4 && UDP
    udp_server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_server_sock > 0) {
        printf("UDP Socket Create Success.\n");
    } else {
        printf("UDP Socket Create Failed.\n");
        goto __END;
    }
    
    memset(&loc_addr, 0, sizeof(loc_addr));
    // 设置协议族
    loc_addr.sin_family = AF_INET;
    // 设置端口
    loc_addr.sin_port = htons(port);
    // 设置ip地址
    loc_addr.sin_addr.s_addr = inet_addr(LOCAL_IP_ADDR);
    // 绑定
    ret = bind(udp_server_sock, (const struct sockaddr *)&loc_addr, sizeof(loc_addr));
    
    if (ret == 0) {
        printf("UDP Socket Bind Success.\n");
    } else {
        printf("UDP Socket Bind Failed.\n");
        goto __END;
    }
    
    // 打开输出文件
    output_file = fopen(output_url, "wb+");
    if (!output_file) {
        printf("Could Not Open Output File.\n");
        goto __END;
    }
    
    printf("Listen %s:%d...\n", LOCAL_IP_ADDR, port);
    
    bzero(&remote_addr, sizeof(remote_addr));
    remote_addr_len = sizeof(sockaddr_in);
    recv_data = (unsigned char *)calloc(RECV_BUFFER_SIZE, sizeof(unsigned char));
    
    while (1) {
        // 接受 UDP 数据   每次读取一个 RTP 包
        ssize_t pkt_size = recvfrom(udp_server_sock, recv_data, RECV_BUFFER_SIZE, 0, (struct sockaddr *)&remote_addr, &remote_addr_len);
        if (pkt_size > 0) {
#if 1 /*1: dump rtp payload  0: dump rtp packet*/
            if (recv_data[1] == 0xc8 || recv_data[1] == 0xc9) {
                // RTCP Header
                rtcp_header_len = sizeof(RTCP_FIXED_HEADER);
                memcpy((void *)&rtcp_header, recv_data, rtcp_header_len);
                
                switch (rtcp_header.PT) {
                    case 200: sprintf(payload_type_str, "SR"); break;
                    case 201: sprintf(payload_type_str, "RR"); break;
                    default: sprintf(payload_type_str, "Other(%d)", rtp_header.PT); break;
                }
                
                fprintf(myout, "[RTCP Pkt] %5d| %15s:%5hu| %10s| %10u| %5d| %5zd|\n", index, inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port), payload_type_str, ntohl(rtp_header.Timestamp), ntohs(rtp_header.Seq_No), pkt_size);
            } else {
                // RTP Header
                rtp_header_len = sizeof(RTP_FIXED_HEADER);
                memcpy((void *)&rtp_header, recv_data, rtp_header_len);
                
                // RFC3551
                switch (rtp_header.PT) {
                    case 18: sprintf(payload_type_str, "Audio"); break;
                    case 31: sprintf(payload_type_str, "H.261"); break;
                    case 32: sprintf(payload_type_str, "MPV"); break;
                    case 33: sprintf(payload_type_str, "MP2T"); break;
                    case 34: sprintf(payload_type_str, "H.263"); break;
                    case 96: sprintf(payload_type_str, "H.264"); break;
                    default: sprintf(payload_type_str, "Other(%d)", rtp_header.PT); break;
                }
                
                fprintf(myout, "[RTP Pkt]  %5d| %15s:%5hu| %10s| %10u| %5d| %5zd|\n", index, inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port), payload_type_str, ntohl(rtp_header.Timestamp), ntohs(rtp_header.Seq_No), pkt_size);
                
                // RTP Data
                // 这里只当 payload type 为 MP2T 时写入本地   H264 和 AAC 在 RTP 中的打包不是 NALU 和 ADTS 的方式  需特殊解析后才能播放  暂不处理
                if (rtp_header.PT == 33) {
                    unsigned char *rtp_data = recv_data + rtp_header_len;
                    size_t rtp_data_size = pkt_size - rtp_header_len;
                    fwrite(rtp_data, rtp_data_size, 1, output_file);
                }
            }
#else
            fprintf(myout, "[UDP Pkt] %5d| %5zd|\n", index, pkt_size);
            fwrite(recv_data, pkt_size, 1, output_file);
#endif
            index++;
        }
    }
    
__END:
    if (udp_server_sock > 0) {
        shutdown(udp_server_sock, 0);
        printf("UDP Socket Shut Down.\n");
    }
    
    if (output_file) {
        fclose(output_file);
    }
    
    if (recv_data) {
        free(recv_data);
    }
}
