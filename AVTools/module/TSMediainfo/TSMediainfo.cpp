//
//  TSMediainfo.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/4/27.
//

/*
 TS是一种音视频封装格式，全程为MPEG2-TS，其中TS即 “Transport Stream” 的缩写。
 先简要介绍一下什么是MPEG2-TS：
 DVD的音视频个是为MPEG2-PS，全程是Program Stream。而TS的全程则是Transport Stream。MPEG2-PS主要应用于存储具有固定时长的节目，如DVD电影，而MPEG-TS则主要应用于实时传送的节目，比如实时广播的电视节目。二者的主要区别举例来说，比如将DVD上的VOB文件的前面一节cut掉，那么就会导致整个文件无法解码，而电视节目是任何时候打开电视机都能解码收看的。
 所以MPEG2-TS格式的特点就是要求从视频流的任一片段开始都是可以独立解码的。
 可以看出，TS格式主要用于直播的码流结构，具有很好的容错能力。通常TS流的后缀是.ts、.mpg或者.mpeg，多数播放器支持这种格式的播放。TS流中不包含快速seek的机制，只能通过协议层实现seek。HLS协议是基于TS流实现的。
 
 TS文件由许多定长的TS Packet组成，长度一般为188字节，日本的DVB-S广播系统采用192个字节的TS Packet，美国采用204个字节的TS Packet，多加了16字节的前向纠错编码（FEC）。
 TS Packet可以包含Header、Adaptation Field和Payload两部分，Header的结构如下：
                      语法                               比特大小                             描述
                  sync_byte                                 8                       同步字节  固定为0x47
        transport_error_indircator                    1                       传输错误指示位  置1时  表示传送包中至少有一个不可纠正的错误位
      payload_unit_start_indircation                 1                       负载单元开始标识  该字段用来表示TS包的有效净荷带有PES包或者PSI数据的情况
                                                                                         为1时表示是PES/PSI的第一个TS包且开始1字节保留对于承载PES包或PSI数据的传输流包，它具有标准含义。
                                                                                         - 当传输流包有效载荷包含PES包数据时，payload_unit_start_indicator 具有以下意义：‘1’指示此传输流包的有效载荷应随着PES 包的首字节开始，‘0’指示在此传输流包中无任何PES包将开始。若payload_unit_start_indicator 设置为‘1’，则一个且仅有一个PES 包在此传输流包中起始。这也适用于stream_type 6 的专用流;
                                                                                         - 当传输流包有效载荷包含PSI 数据时，payload_unit_start_indicator 具有以下意义：若传输流包承载PSI分段的首字节，则payload_unit_start_indicator 值必为1，指示此传输流包的有效载荷的首字节承载pointer_field。若传输流包不承载PSI 分段的首字节，则payload_unit_start_indicator 值必为‘0’，指示在此有效载荷中不存在pointer_field。参阅2.4.4.1 和2.4.4.2。这也适用于stream_type 5 的专用流;
                                                                                         - 对空包而言，payload_unit_start_indicator 必须设置为‘0’。
            transport_priority                           1                        传输优先级  表明该包比同个PID的但未置位的TS包有更高的优先级
                      pid                                     13                      负载类型标识  如果净荷是PAT  则PID固定为0x00  0x1FFF为空包
       transport_scrambling_control                 2                        传输加扰控制位  表示TS流分组有效负载的加密模式  空包为0x00  如果传输包包头中包括调整字段  不应被加密  其他取值含义是用户自定义的
          adaptation_field_control                     2                        自适应调整域控制位  00是保留值  01负载中只有有效载荷  10负载中只有自适应字段  11先有自适应字段 后有有效载荷
             continuity_counter                          4                        连续计数器  随着每一个具有相同PID的TS流分组而增加  当它达到最大值后又回复到0  范围为0~15
 
 TS Packet的调整字段Adaptation Field。在 MPEG2-TS 中，为了传送打包后长度不足 188 字节（包括包头）的不完整 TS Packet，或者为了在系统层插入节目时钟参考 PCR 字段，需要在 TS 包中插入可变长度字段的调整字段。
 调整字段包括对较高层次的解码功能有用的相关信息，调整字段的格式基于采用若干标识符，以表示该字段的某些特定扩展是否存在。
 调整字段由 1B 调整字段长度、不连续指示器、随机存取器、PCR 标志符、基本数据流优先级指示器、拼接点标志符、传送专用数据标志、调整字段扩展标志以及有相应标志符的字段组成。
  调整字段是一个可变长的域，是由 TS Header 中的 adaption_field_control（调整字段控制）来标识的。当利用包头的信息将各基本比特流提取出来后，调整字段提供基本比特流解码所需的同步及时序等功能，以及编辑节目所需的各种机制，如本地解码插入等。
 Adaptation Field的结构如下（https://www.cnblogs.com/jimodetiantang/p/9140808.html）：
 
        第一层Adaptation Field：
                    语法                                 比特大小                                  描述
        adaptation_field_length                         8                         指定了其后的 adaptation field 的字节数
        discontinuity_indicator                          1                         为 1 指示当前的 TS Packet 的不连续状态为 true；为 0则当前的 TS packet 的不连续状态为 false
       random_access_indicator                        1                         指示当前的 TS packet 和后续可能具有相同 PID 的 TS packet 包含一些支持此时可以随意访问的信息
 elementary_stream_priority_indicator             1                         指示在具有相同 PID 的 packet 中  在该 TS packet 的负载内携带的 ES 数据（即音视频数据）的优先级
                  pcr_flag                                   1                          为 1 指示 adaptation field 包含两部分编码的 PCR 字段；为 0 指示 adaptation field 没有包含任何的 PCR 字段
                 opcr_flag                                  1                          为 1 指示 adaptation field 包含两部分编码的 OPCR 字段；为 0 指示 adaptation field 没有包含任何 OPCR 字段
           splicing_point_flag                            1                          为 1 指示 splice_countdown 将会在相关 adaptation field 中指定的拼接点出现；为 0 指示 splice_countdown 在 adaptation field 中不存在
      transport_private_data_flag                    1                          为 1 指示 adaptation field 包含一个或多个 private_data 字节；为 0 指示 adaptation field 中没有包含任何的 private_data 字节
    adaptation_field_extension_flag                  1                          为 1 表示有 adaptation field 扩展出现；为 0 表示 adaptation field 中不存在 adaptation field 扩展
              optional_fields                             可选                        可选描述  第二层套娃入口

                第二层optional_fields：
                                语法                              比特大小（包含占位比特）                            描述
                                 pcr                                              48                                program_clock_reference_base[33bit] + reserved[6bit] + program_clock_reference_extension[9bit]
                                opcr                                             48                                original_program_clock_reference_base[33bit] + reserved[6bit] + original_program_clock_reference_extension[9bit]
                          splicing_point                                        8                                 splice_countdown[8bit]
                    transport_private_data                               可选                              transport_private_data_length[8bit] + private_data_byte[transport_private_data_length * 8bit]
                adaption_field_extension_length                          8                                adaption_field_extension_length[8bit]
                              ltw_flag                                            1                                为 1 表示有 ltw_offset 字段存在
                      piecewise_rate_flag                                    1                                为 1 表示有 piecewise_rate 字段存在
                     seamless_splice_flag                                    1                                为 1 表示有 splice_type 和 DTS_next_au
                              reserved                                           5                                保留位
                          optional_fields                                     可选                              可选描述  第三层套娃入口
 
                        第三层optional_fields：
                                        语法                             比特大小（包含占位比特）                             描述
                                         ltw                                              16                                 ltw_valid_flag[1bit] + ltw_offset[15bit]
                                  piecewise_rate                                     24                                 reserved[2bit] + piecewise_rate[22bit]
                                  seamless_splice                                    40                                 splice_type[4bit] + dts_next_au[32...30] + '1' + dts_next_au[29...15] + '1' + dts_next_au[14...0] + '1'
 
 TS Packet的净荷Payload所传输的信息包括两种类型：节目专用信息PSI，音视频的PES包及辅助数据。
 节目专用信息PSI（Program Specific Information）：
        在TS流中传输的主要有四类表格，其中包含解复用和显示节目相关的信息。节目信息的结构性的描述如下：
            -   节目关联表Program Association Table (PAT) 0x0000
            -   节目映射表Program Map Tables (PMT)
            -   条件接收表Conditional Access Table (CAT) 0x0001
            -   网络信息表Network Information Table（NIT）0x0010
            -   业务描述表Service Descriptor Table（SDT）0x0011
            -   传输流描述表Transport Stream Description Table（TSDT）0x02
 
        PAT：
            PAT表用MPEG指定的PID（00）标明，通常用PID=0表示。它的主要作用是针对复用的每一路传输流，提供传输流中包含哪些节目、节目的编号以及对应节目的节目映射表（PMT）的位置，即PMT的TS包的包标识符（PID）的值，同时还提供网络信息表（NIT）的位置，即NIT的TS包的包标识符（PID）的值。
            其具体结构如下：
 
                         语法                                   比特大小                                  描述
                       table_id                                     8                             标识一个TS PSI分段内容是节目关联分段、条件访问分段还是节目映射分段  对于PAT置为0x00
            section_syntax_indicator                         1                             段语法标志位  固定为1
                          '0'                                         1                             固定为0
                      reserved                                    2                             保留位'11'
                  section_length                                12                            分段长度字段  section_length之后到CRC_32字段的字节数
              transport_stream_id                            16                            该字节充当标签  标识网络内此传输流有别于任何其他路复用流  其值由用户规定
                      reserved                                    2                             保留位'11'
                 version_number                                5                             PAT的版本号  一旦PAT有变化  版本号加1
             current_next_indicator                          1                             为1表示当前传送的PAT表可以使用  若为0则要等待下一个表
                 section_number                                8                             表明该TS包属于PAT的第几个分段  分段号从0开始  因为PAT可以描述很多PMT信息  所以长度可能比较长
             last_section_number                              8                            表明该PAT的最大分段数目  一般情况都是一个PAT表由一个TS包传输
 
        === Start Loop ===
                program_number                                16                           节目号
                     reserved                                      3                            保留位'111'
       network_id/program_map_PID                       13                           节目号为0x0000时  表示这是NIT的PID；节目号为其他时   表示这是PMT的PID
        === End Loop ===
 
                     CRC_32                                       32                          载荷最后4个字节  CRC校验
 
        PMT：
            节目映射表指明该节目包含的内容，即该节目由哪些流组成，这些流的类型（音频、视频、数据），以及组成该节目的流的位置，即对应的TS包的PID值，每路节目的节目时钟参考（PCR）字段的位置。
            其具体结构如下：
 
                         语法                                   比特大小                                  描述
                       table_id                                     8                             标识一个TS PSI分段内容是节目关联分段、条件访问分段还是节目映射分段  对于PMT置为0x02
            section_syntax_indicator                         1                             段语法标志位  固定为1
                          '0'                                         1                             固定为0
                      reserved                                    2                             保留位'11'
                  section_length                                12                            分段长度字段  section_length之后到CRC_32字段的字节数
                 program_number                              16                            频道号码  表示当前的PMT关联到的频道
                      reserved                                    2                             保留位'11'
                  version_number                                5                            PMT的版本号  如果PMT内容有更新  则它会递增1通知解复用程序需要重新接收节目信息
             current_next_indicator                          1                             该字段置为1时  表示当前传送的program_map_section可用；置为0时  表明该传送的段不能使用  下一个表分段才能有效
                  section_number                                8                            分段编号  该字段一般总是置为0x00
              last_section_number                             8                             分段数量  该字段一般总是置为0x00
                      reserved                                    3                             保留位'111'
                      PCR_PID                                   13                            PCR(节目参考时钟)所在TS分组的PID
                      reserved                                    4                             保留位'1111'
              program_info_length                            12                           节目信息长度  之后的是N个描述符结构  一般可以忽略掉  这个字段就代表描述符总的长度  单位是Bytes
 
        === Start Loop ===
                    stream_type                                 8                             流类型  标志是Video还是Audio还是其他数据  https://blog.csdn.net/u013898698/article/details/78530143
                      reserved                                    3                             保留位'111'
                  elementary_pid                               13                            表明负载该PES流的PID值
                      reserved                                    4                             保留位'1111'
                  es_info_length                                12                            表明跟随其后描述相关节目元素的字节数  默认一般为0  不描述  直接进到下一次循环
                     descriptor                            es_info_length                   如果 es_info_length 不为0  会有descriptor  长度为 es_info_length 字节
        === End Loop ===
 
                       crc_32                                     32                           载荷最后4个字节  CRC校验
 
        SDT：
             机顶盒先调整高频头到一个固定的频率(如498MHZ)，如果此频率有数字信号，则COFDM芯片(如MT352)会自动把TS流数据传送给MPEG- 2 decoder。 MPEG-2 decoder先进行数据的同步，也就是等待完整的Packet的到来.然后循环查找是否出现PID== 0x0000的Packet，如果出现了，则马上进入分析PAT的处理，获取了所有的PMT的PID。接着循环查找是否出现PMT，如果发现了，则自动进入PMT分析，获取该频段所有的频道数据并保存。如果没有发现PAT或者没有发现PMT，说明该频段没有信号，进入下一个频率扫描。
 
             在解析TS流的时候，首先寻找PAT表，根据PAT获取所有PMT表的PID；再寻找PMT表，获取该频段所有节目数据并保存。这样，只需要知道节目的PID就可以根据PacketHeade给出的PID过滤出不同的Packet，从而观看不同的节目。这些就是PAT表和PMT表之间的关系。而由于PID是一串枯燥的数字，用户不方便记忆、且容易输错，所以需要有一张表将节目名称和该节目的PID对应起来，DVB设计了SDT表来解决这个问题。 该表格标志一个节目的名称，并且能和PMT中的PID联系起来，这样用户就可以通过直接选择节目名称来选择节目了。

             SDT可以提供的信息包括:
                (1) 该节目是否在播放中
                (2) 该节目是否被加密
                (3) 该节目的名称
 
 音视频的PES（Packet Elemental Stream）：
        PES即Packetized Elementary Stream（分组的ES），也就是对编码器原始数据的第一次打包，在这个过程中将ES流分组、打包、加入包头信息等操作。
        PES数据结构比较复杂，下面套娃开始：
 
        第一层pes：
                        语法                                     比特大小                                     描述
         packet_start_code_prefix                            24                               PES的起始码  三字节  默认0x000001
                    stream_id                                       8                                流id  音频取值（0xc0-0xdf）通常为0xc0；视频取值（0xe0-0xef）通常为0xe0
              pes_packet_length                                 16                              用于存储PES包的长度  包括此字段后的可选包头optional_pes_header和负载pes_packet_data的长度
             optional_pes_header                              可选                             可选包头  第二层optional_pes_header套娃入口
               pes_packet_data                                 可选                             pes包载荷  第二层pes_packet_data套娃入口
 
                第二层optional_pes_header：
                                语法                                     比特大小                                     描述
                                '10'                                           2                                默认规定
                    pes_scrambling_control                             2                               加密模式  00 未加密  01 或 10 或 11 有用户定义
                            pes_priority                                    1                               有效负载的优先级  该字段为 1 的 PES packet 的优先级比为 0 的 PES packet 高
                   data_alignment_indicator                           1                                数据定位指示器
                              copyright                                     1                                版本信息  1 为有版权  0 无版权
                        original_or_copy                                  1                               原始或备份  1 为 原始  0 为备份
                          pts_dts_flags                                    2                               10 表示 PES 头部有 PTS 字段  11 表示有 PTS 和 DTS 字段  00 表示都没有  10 被禁止
                             escr_flag                                       1                               1 表示 PES 头部有 ESCR 字段  0 则无该 ESCR 字段
                           es_rate_flag                                     1                               1 表示 PES 头部有 ES_rate 字段  0 表示没有
                     dsm_trick_mode_flag                                1                               1 表示有一个 8bit 的 track mode 字段  0 表示没有
                  additional_copy_info_flag                             1                              1 表示有 additional_copy_info 字段  0 表示没有
                           pes_crc_flag                                    1                               1 表示 PES 包中有 CRC 字段  0 表示没有
                       pes_extension_flag                                1                               1 表示 PES 头部中有 extension 字段存在  0 表示没有
                    pes_header_data_length                            8                               指定在 PES 包头部中可选头部字段和任意的填充字节所占的总字节数  可选字段的内容由上面的 7 个 flag 来控制
                          optional_fields                                 可选                             可选字段描述  第三层optional_fields套娃入口
 
                        第三层optional_fields：
                                        语法                           比特大小（包含占位比特）                     描述
                                    pts & dts                         '10': 40bit  '11': 80bit              如果pts_dts_flags是'10'  则 '0010' + PTS[32...30] + '1' + PTS[29...15] + '1' + PTS[14...0] + '1'
                                                                                                                        如果pts_dts_flags是'11'  则 '0011' + PTS[32...30] + '1' + PTS[29...15] + '1' + PTS[14...0] + '1' + '0001' + DTS[32...30] + '1' + DTS[29...15] + '1' + DTS[14...0] + '1'
                                        escr                                       48bit                         Reserved 2bit + escr_base[32...30] + '1' + escr_base[29...15] + '1' + escr_base[14...0] + '1' + escr_extensiom[8...0] + '1'
                                     es rate                                      24bit                         '1' + es_rate[21...0] + '1'
                                 dsm_trick_mode                                8bit                          前三位是track_mode_control  根据track_mode_control的取值决定后5位代表的字段意义  暂时忽略
                              additional_copy_info                             8bit                          '1' + additional_copy_info[6...0]
                                      pes_crc                                     16bit                         previous_pre_packet_crc[15...0]
                                  pes_extension                                 可选                          来吧 第四层套娃入口 包含5个bit的flag 和optional fields
 
                                第四层pes_extension：
                                                语法                          比特大小                         描述
                                    pes_private_data_flag                   1                      1 表示 PES 头部有 pes_private_data 字段  0 表示没有
                                    pack_header_field_flag                  1                      1 表示 PES 头部有 pack_header_field 字段  0 表示没有
                        program_packet_sequence_counter_flag       1                       1 表示 PES 头部有 program_packet_sequence_counter 字段  0 表示没有
                                        p-std_buffer_flag                     1                       1 表示 PES 头部有 p-std_buffer 字段  0 表示没有
                                             reserved                            3                       保留位'111'
                                    pes_extension_flag_2                    1                       1 表示 PES 头部有 pes_extension_2 字段  0 表示没有
                                        optional_fields                       可选                     可选字段描述  第五层optional_fields套娃入口
 
                                        第五层optional_fields：
                                                            语法                          比特大小（包含占位比特）                         描述
                                                    pes_private_data                                128                                  私有数据
                                                    pack_header_field                                 8                                    pack_field_length：指示 pack_header() 的大小；pack_header()
                                        program_packet_sequence_counter                      8                                    '1' + program_packet_sequence_counter[7bit] + '1' + MPEG1_MPEG2_identifier[1bit] + original_stuff_length[6bit]
                                                        p-std_buffer                                    16                                   '01' + p-std_buffer_scale[1bit] + p-std_buffer_size[13bit]
                                                    pes_extension_2                                  可选                                 终极套娃
            
                                                第六层pes_extension_2：
                                                                  语法                         比特大小                          描述
                                                                marker                             1                          占位符'1'
                                                    pes_extension_field_length               7                          pes扩展数据长度
                                                     stream_id_extension_flag                1                          0 表示 PES 头部有  stream_id_extension 字段 1标识没有
                                                        stream_id_extension                    7                          stream id extension  stream_id_extension_flag为0才有
                                                                reserved                          可选                        如果stream_id_extension_flag为0  reserved长度为pes_extension_field_length * 8
                        
                第二层pes_packet_data：
                    pes_packet_data存放的是编码后的音视频ES流，下面分别以h264和aac举例说明码流结构：
 
                        视频（H264）：封装格式 H264 Annexb，即在每一个 NALU 前都需要插入 0x00000001（4字节，首次开始打包该视频帧为 ES 时插入 4 字节）或 0x000001（3 字节，普通情况下插入该分隔符）。打包 es 层数据时 pes 头和 es 数据之间要加入一个 AUD（type=9） 的 NALU，payload是一个随意的字节；关键帧 slice 前必须要加入 SPS（type=7） 和 PPS（type=8）的 NALU，而且是紧邻的。
                
                         音频（AAC）：封装格式 ADTS，即 ADTS Header（7或9字节）+ ADTS Payload。
 
                         *注：H264 Annexb 和 ADTS 的解析参考 H264Parser、AACParser
 */

#include "TSMediainfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

extern "C" {
#include "TSParser.h"
}

static void parse(char *url);

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - MPEG-TS\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools TSMediainfo -i input.ts\n\n");
    printf("Get TS With FFMpeg From Mp4 File:\n\n");
    printf("   ffmpeg -i input.mp4 -codec: copy -bsf:v h264_mp4toannexb -start_number 0 -hls_time 10 -hls_list_size 0 -f hls output.m3u8\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void ts_mediainfo_parse_cmd(int argc, char *argv[]) {
    
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
        printf("TSMediaInfo Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
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
    size_t bytes_read = 0;
    uint8_t packet_buffer[TS_PACKET_SIZE];
    int n_packets = 0;

    TSParser tsParser;
    memset(&tsParser, 0, sizeof(TSParser));

    input_file = fopen(url, "rb");
    if(!input_file) {
        printf("Error opening the stream\nSyntax: tsunpacket FileToParse.ts\n");
        return;
    }

    while(1) {
        bytes_read = fread(packet_buffer, 1, TS_PACKET_SIZE, input_file);
        if(packet_buffer[0] == TS_DISCONTINUITY) {
            printf("Discontinuity detected!\n");
        } else if (bytes_read < TS_PACKET_SIZE) {
            printf("End of file!\n");
            break;
        } else if(packet_buffer[0] == TS_SYNC) {
            parseTSPacket(&tsParser, packet_buffer, bytes_read);
            n_packets++;
        }
    }

    printf("Number of packets found: %d\n", n_packets);

    fclose(input_file);
    freeParserResources(&tsParser);
}
