//
//  MP4Mediainfo.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/4/27.
//

/* mp4封装格式常用box类型解析
 【Ⅰ ftyp】：
        ftyp是整个文件的第一个box，用来指示文件应用级别的信息。该box有且只有1个，并且只能被包含在文件层，而不能被其他box包含。该box应该被放在文件的最开始，指示文件的相关信息。
         box依次包含4字节的box size，4字节的box类型，4字节的major brand，4字节的minor version，最后部分是compatible brands。
        ftyp类型参考：http://www.ftyps.com/
 
 【Ⅰ moov】：
        moov也是MP4文件中必须有但是只存在一个的box，这个box里面一般存的是媒体文件的元数据，包含子box，自己更像是一个分界标识。
        所谓的媒体元数据主要包含类似SPS、PPS的编解码参数信息，还有音视频的时间戳等信息。对于MP4还有一个重要的采样表stbl信息，这里面定义了采样Sample、Chunk、Track的映射关系，是MP4能够进行随机拖动和播放的关键。
        box依次包含4字节的box size，4字节的box类型，之后即是第一个子box。
 
    【Ⅱ mvhd】：
            mvhd也是全文见唯一的box，一般处于moov的第一个子box，这个box对整个媒体文件所包含的媒体数据进行全面的描述。其中包含媒体的创建和修改时间，默认音量、色域、时长等信息。
            box依次包含以下信息：
                - box size                  4字节            box的大小，一般为108
                - box type                 4字节            'mvhd'
                - version                   1字节            box的版本，0或1，一般为0
                - flags                      3字节            根据各个box类型定义是0还是1，一般为0
                - creation time           4字节            创建时间（相对于UTC时间1904-01-01零点的秒数）
                - modification time      4字节            修改时间（相对于UTC时间1904-01-01零点的秒数）
                - time scale               4字节            文件媒体在1秒时间内的刻度值，可以理解为1秒长度的时间单元数，这个只用来计算了该Mp4文件的长度，但是没有参与PTS和DTS的计算。
                - duration                  4字节           该影片的播放时长，该值除以time scale字段即可以得到该影片的总时长，单位秒。
                - rate                       4字节            推荐播放速率，高16位和低16位分别为小数点整数部分和小数部分，即[16.16] 格式，该值为1.0（0x00010000）表示正常前向播放。
                - volume                    4字节            与rate类似，[8.8] 格式，1.0（0x0100）表示最大音量
                - reserved                 10字节          保留位
                - matrix                    36字节          视频变换矩阵，该矩阵定义了此文件中两个坐标空间的映射关系。
                - pre-defined             24字节          预览相关的信息，一般填充0。
                - next track id           4字节            下一个Track使用的id号，通过该值减去1可以判断当前文件的Track数量。
                
    【Ⅱ iods】：
            iods是非必须box，解析时直接跳过即可。注意该Box也是Full Box意味着Header里面有1字节的Version和3字节的Flag字段。定义的内容应该是Audio和Video ProfileLevel方面的描述。但是现在没有用。
            box依次包含4字节的box size，4字节的box类型，1字节的version，3字节的flag，之后是实际信息。
 
    【Ⅱ udta】：
            udta是非必须box，用来存放用户自定义数据，是一个Container Box。
            box依次包含4字节的box size，4字节的box类型，之后是子box。
 
    【Ⅱ trak】：
            trak定义了媒体文件中媒体中一个Track的信息，媒体文件中可以有多个Track，每个Track具有自己独立的时间和空间的信息，可以进行独立操作。
            每个Track Box都需要有一个Tkhd Box和Mdia Box，其它的Box都是可选择的。
            box依次包含4个字节的box size，4字节的box类型，之后即是第一个子box。
 
        【Ⅲ tkhd】：
                tkhd描述了该Track的媒体整体信息，包括时长、图像的宽度和高度等，同时该Box是Full Box即Box Header后面有Box Header Version一字节和Box Header Flag三字节字段。
                box依次包含以下信息：
                    - box size                  4字节                 box的大小，一般为92字节
                    - box type                 4字节                 'tkhd'
                    - version                   1字节                 box版本，0或1，一般为0
                    - flags                      3字节                 参考http://wikil.lwwhome.cn:28080/?p=613
                    - creation time           4字节                 创建时间（相对于UTC时间1904-01-01零点的秒数）
                    - modification time      4字节                 修改时间（相对于UTC时间1904-01-01零点的秒数）
                    - track ID                  4字节                 唯一标识该Track的一个非零值
                    - reserved                 4字节                 保留位
                    - duration                  4字节                 该Track的播放时长，需要和时间戳进行一起计算，然后得到播放的时间坐标。
                    - reserved                  8字节                保留位
                    - layer                      2字节                 视频层级，值小的在上层
                    - alternate group        2字节                 track分组信息，一般默认为0，表示该track和其它track没有建立群组关系。
                    - volume                     2字节                播放此track的音量，1.0为正常音量
                    - reserved                  2字节                保留位
                    - matrix                     36字节              视频变换矩阵
                    - width                      4字节                如果该Track为Video Track，则表示图像的宽度（16.16浮点表示）
                    - height                     4字节                如果该Track为Video Track，则表示图像的高度（16.16浮点表示）
            
        【Ⅲ edts】：
                edts作用是使某个track的时间戳产生偏移，mp4文件中不一定都含有这个box。
                box依次包含4字节的box size，4字节的box type，之后是子box  elst的数据。
 
            【Ⅳ elst】：
                    elst可包含多个Entry，具体结构如下：
                    - box size                 4字节                box的大小   edts size减去8字节
                    - box type                4字节                'elst'
                    - version                  1字节                一般为0
                    - flag                       3字节               一般为0
                    - entry count             4字节               Entry个数
                    
                    Entry字段：
                    - segment duration     4字节               表示该edit段的时长，以Movie Header Box（mvhd）中的timescale为单位,即 segment_duration/timescale = 实际时长（单位s）
                    - media time              4字节               表示该edit段的起始时间，以track中Media Header Box（mdhd）中的timescale为单位。如果值为-1(FFFFFF)，表示是空edit，一个track中最后一个edit不能为空。
                    - media rate integer    2字节               edit段的速率为0的话，edit段相当于一个”dwell”，即画面停止。画面会在media_time点上停止segment_duration时间。否则这个值始终为1。
                    - media rate fraction   2字节              一般为0
 
        【Ⅲ mdia】：
                mdia是Container Box，里面包含子Box，一般必须有mdhd、hdlr、minf，包含当前Track媒体头信息、媒体句柄以及媒体信息。
                box依次包含4字节的box size，4字节的box type，之后是子box。
 
            【Ⅳ mdhd】：
                    mdhd是Full Box，即包含version和flag字段。该Box里面主要定义了该Track的媒体头信息，其中我们最关心的两个字段是Time scale和Duration，分别表示了该Track的时间戳和时长信息，这个时间戳信息也是PTS和DTS的单位。
                    box依次包含以下信息：
                        - box size                  4字节                 box的大小，一般为32字节
                        - box type                 4字节                 'mdhd'
                        - version                   1字节                 box版本，0或1，一般为0
                        - flags                      3字节                 该Box该字段填充0
                        - creation time           4字节                 创建时间（相对于UTC时间1904-01-01零点的秒数）
                        - modification time      4字节                 修改时间（相对于UTC时间1904-01-01零点的秒数）
                        - timescale                4字节                 当前Track的时间计算单位
                        - duration                 4字节                  该Track的播放时长，需要参考前面的timescale计算
                        - language                 2字节                 媒体语言码。最高位为0，后面15位为3个字符（见ISO 639-2/T标准中定义）
                        - quality                   2字节                  媒体的回放质量
 
             【Ⅳ hdlr】:
                    hdlr是Full Box，即包含version和flag字段。该Box解释了媒体的播放过程信息，用来设置不同Track的处理方式，标识了该Track的类型。该box也可以被包含在meta box（meta）中。
                    box依次包含以下信息：
                        - box size                  4字节                 box的大小，不定大小
                        - box type                 4字节                 'hdlr'
                        - version                   1字节                 box版本，0或1，一般为0
                        - flags                      3字节                 该Box该字段填充0
                        - handle type             4字节                 Handle 的类型  'mhlr':media handles 'dhlr':data handle 实际处理时默认为0即可
                        - hand sub type          4字节                 视频 'vide'   音频 'soun'
                        - reserved                 12字节               保留位
                        - component name       可变                  这个component的名字，其实这里就是你给该Track起的名字，打包时填写一个有意义字符串就可以。
 
              【Ⅳ minf】：
                    minf是Container Box，一般含有三个必须的子box，媒体信息头vmhd或smhd，数据信息dinf，采样表stbl。
                    box依次包含4个字节的box size，4字节的box类型，之后即是子box。
 
                  【Ⅴ vmhd 】：
                        该box为Full Box意味着多了四字节的version和Flag字段。
                        box依次包含以下信息：
                        - box size                  4字节                 box的大小，一般为20字节
                        - box type                 4字节                 'vmhd'
                        - version                   1字节                 box版本，0或1，一般为0
                        - flags                      3字节                 一般为1
                        - graphics mode         2字节                 视频合成模式，为0时拷贝原始图像，否则与opcolor进行合成，一般默认0x00 00即可
                        - opcolors                 6字节                 颜色值，RGB颜色值，｛red，green，blue｝，一般默认0x00 00 00 00 00 00
 
                  【Ⅴ smhd 】：
                        该box为Full Box意味着多了四字节的version和Flag字段。
                        box依次包含以下信息：
                        - box size                  4字节                 box的大小，一般为16字节
                        - box type                 4字节                 'smhd'
                        - version                   1字节                 box版本，0或1，一般为0
                        - flags                      3字节                 一般为0
                        - balance                  4字节                 音频的均衡用来控制计算机的两个扬声器的声音混合效果，一般值是0
 
                   【Ⅴ dinf】：
                          dinf是一个Container Box，用来定位媒体信息，一般会包含一个dref即data reference box，dref下面会有若干个url box或者也叫urn box，这些box组成一个表，用来定位Track的数据。Track可以被分成若干个段，每一段都可以根据url或者urn指向的地址来获取数据，sample描述中会用这些片段的序号将这些片段组成一个完整的Track，一般情况下当数据完全包含在文件中，url和urn的字符串是空的。
                          这个box存在的意义就是允许mp4文件的媒体数据分开最后还能进行恢复合并操作，但是实际上，Track的数据都保存在文件中，所以该字段的重要性还体现不出来。
                          box依次包含4个字节的box size，4字节的box类型，之后即是子box。
 
                        【Ⅵ dref】：
                                dref用来定义当前Track各个段的URI，Full Box。
                                box依次包含以下信息：
                                - box size                  4字节                 box的大小
                                - box type                 4字节                 'dref'
                                - version                   1字节                 box版本，0或1，一般为0
                                - flags                      3字节                 一般为0
                                - entry count             4字节                 Data Reference数目
                                - url or urn list          4字节                 url还是urn
 
                              【Ⅶ url】：
                                        这里直接把url box作为dref的box data进行分析，包含以下信息：
                                        - box size                  4字节                 box的大小
                                        - box type                 4字节                 'url '
                                        - version                   1字节                 一般为0
                                        - flags                      3字节                 值为1则表明“url”中的字符串为空，表示Track数据已包含在文件中，所以Url的Url Box Data部分为空。
                         
                    【Ⅴ stbl】：
                            stbl全称Sample Table，包含了Track中media媒体的所有时间和索引，利用这个容器的Sample信息，就可以定位Sample的媒体时间、类型、大小以及其相邻的Sample。
                            其一般要包含下列子Box：
                            - stsd：即Sample Description   采样描述容器
                            - stts：即Time To Sample        采样时间容器
                            - stss：即Sync Sample            采样同步容器
                            - stsc：即Sample To Chunk      Chunk采样容器
                            - stsz：即Sample Size             采样大小容器
                            - stco：即Chunk Offset           Chunk偏移容器
                            stbl同样是Container Box，包含4字节的box size和4字节的box type，之后即是第一个子box。
 
                        【Ⅵ stsd】：
                                stsd是一个Container Box，存储了编码类型的初始化解码器需要的信息，与Track Type有关。
                                box依次包含以下信息：
                                - box size                                             4字节               box的大小
                                - box type                                            4字节               'stsd'
                                - version                                              1字节               box版本，0或1，一般为0
                                - flags                                                 3字节               一般为0
                                - sample description entry count              4字节               Sample Description数目
                                - sample description entry                       可变                Sample Description的实际内容
 
                                通常在stsd下还会有两级box，视频Track常见的是子box avc1和2级子box avcC，音频Track常见的是子box mp4a和2级子box esds：
                                
                              【Ⅶ avc1】：
                                      avc1记录了视频编解码相关的信息，是一个Container Box。
                                      box依次包含以下信息：
                                      - box size                                       4字节                 box的大小
                                      - box type                                      4字节                 'avc1'
                                      - reserved                                      4字节                 保留位
                                      - reserved                                      2字节                 保留位
                                      - data reference ID                          2字节                 引用参考dref box
                                      - code stream version                       4字节                 一般为0
                                      - reserved                                      12字节               保留位
                                      - width                                           2字节                图像宽度
                                      - height                                          2字节                图像高度
                                      - horizontal resolution                       4字节                默认值一般都是72dpi（前两个字节整数部分，后面两个字节小数部分）
                                      - vertical resolution                          4字节                默认值一般都是72dpi（前两个字节整数部分，后面两个字节小数部分）
                                      - reserved                                       4字节                保留位
                                      - frame count                                  2字节                 每采样点图像的帧数，有些情况下每个采样点有多帧，默认值一般是0x0001
                                      - compressor name                           32字节               默认填充32字节0即可
                                      - depth                                           2字节                默认值为0x0018
                                      - reserved                                       2字节                默认值为0xffff
                                      - avcc/esds/hvcc                              可变                 h264：avcC   h265：hvcC
                                      - btrt                                              20字节              描述解码器需要的Buffer大小和码率相关信息
                        
                                    【Ⅷ avcC】：
                                            avcC包含SPS、PPS等信息，音视频编解码参数。
                                            box依次包含以下信息：
                                            - box size                                       4字节                 box的大小
                                            - box type                                      4字节                 'avcC'
                                            - configuration version                     1字节                 默认0x01
                                            - avc profile indication                     1字节                 H264编码配置：0x64表示baseline   0x4d表示main
                                            - profile compatibility                      1字节                 一般为0
                                            - avc level indication                        1字节                 profile level
                                            - reserved                                      6bit                  保留位 填1
                                            - length size minus one                     2bit                   读出的每个packet的前几字节代表数据大小
                                            - reserved                                      3bit                  保留位 填1
                                            - number of sps                               5bit                  SPS个数，一般为1
                                            - sps length                                    2字节                SPS长度
                                            - sps                                            sps length           SPS
                                            - reserved                                      3bit                  保留位 填0
                                            - number of pps                               5bit                  PPS个数，一般为1
                                            - pps length                                    2字节                PPS长度
                                            - pps                                            pps length           PPS
 
                               【Ⅶ mp4a】：
                                        mp4a记录了音频编解码相关的信息，是一个Container Box。
                                        box依次包含以下信息：
                                        - box size                                       4字节                 box的大小
                                        - box type                                      4字节                 'mp4a'
                                        - reserved                                      4字节                 保留位 填0
                                        - reserved                                      2字节                 保留位 填0
                                        - data reference ID                          2字节                 引用参考dref box
                                        - version                                        2字节                 一般为0
                                        - revision level                                2字节                 一般为0
                                        - reserved                                      4字节                 一般为0
                                        - channel count                               2字节                 音频通道个数
                                        - sample size                                  2字节                 音频采样位深
                                        - pre defined                                  2字节                 一般为0
                                        - reserved                                      2字节                 一般为0
                                        - sample rate                                  4字节                音频采样率，取高位16bit，因此需右移16bit
 
                                      【Ⅷ esds】：
                                              esds主要包含音频的编码信息和音频码率信息。
                                              box依次包含以下信息：
                                              - box size                                                      4字节                 box的大小
                                              - box type                                                     4字节                 'esds'
                                              - version                                                       1字节                 一般为0
                                              - flag                                                            3字节                一般为0
                                              - es description(ed) tag                                   1字节                基本流描述标记，默认0x03
                                              - ed tag size                                                  4字节                表示后面还有多少字节  一般取最低位字节表示size
                                              - ed track id                                                  2字节                原始音频流的id
                                              - ed flag                                                       1字节                默认为0
                                              - decoder config descriptor(dcd) tag                  1字节                解码配置参数描述标记，默认0x04
                                              - dcd tag size                                                4字节                表示dcd长度
                                              - dcd mpeg-4 audio                                         1字节                一般为0x40  mpeg-4 aac
                                              - dcd audio stream                                          1字节               按照标准或计算得到  此处一般默认0x15
                                              - dcd buffersize db                                          3字节                建议的解码器缓存大小
                                              - dcd max bitrate                                            4字节                音频数据最大码率
                                              - dcd avg bitrate                                            4字节                音频数据平均码率
                                              - decoder specific info description(dsid) tag        1字节                解码规格标记，默认0x05
                                              - dsid tag size                                               4字节                表示dsid长度
                                              - dsid audio specific config(asc)                        2字节                音频规格数据，bit级详细参数如下：
                                                    - asc object type                                      5bit                  00010表示采用的音频编码规格为AAC LC
                                                    - asc frequency index                                4bit                  0101表示采样率是44100？
                                                    - asc channel configuration                         4bit                  0010表示左右双声道
                                                    - asc frame length flag                               1bit                 0代表每个包的大小为1024字节  也就是一帧音频的大小
                                                    - asc depends on core order                        1bit                 0
                                                    - asc extension flag                                   1bit                 0
 
                                              * 注：编码规格可查 https://wiki.multimedia.cx/index.php?title=MPEG-4_Audio
                                                        esds最后还有一个id为6的tag，ffmpeg定义为MP4SLDescrTag，待之后补全。
 
                        【Ⅵ stts】：
                                stts是Sample Number和解码时间DTS之间的映射表，是一个Full Box。
                                box依次包含以下信息：
                                - box size                     4字节                 box的大小
                                - box type                    4字节                 'stts'
                                - version                      1字节                 一般为0
                                - flag                           3字节                一般为0
                                - entry count                 4字节                描述了下面sample count和sample delta组成的二元组信息个数
                                - sample count              4字节                 连续相同时间长度为sample delta的sample个数
                                - sample delta               4字节                 以timescale为单位的时间长度  如果所有的sample delta值相同则说明是固定帧率
 
                                通过这个表格，我们可以找到任何时间的Sample。stts的表格分别包含连续的样点数目Sample Count和样点相对时间差值Sample Delta，即表格中每个条目提供了在同一个时间偏移量里面连续的Sample序号以及Sample偏移量。这里的相对时间差单位由该Track的mdhd描述的时间单位为准，可以参考上文mdhd的解析。
                                当然用这个表格只是能找到当前时间的Sample序号，但是该Sample的长度和指针还要靠其他表格获取。
                                Sample Delta可以简单理解为采样点Sample之间的duration，因此各duration相加应为Track的总时长，即sample count * sample delta / mdhd的timescale = track duration。
                                如果表格内只有一项，说明是固定帧率，帧率的计算通过mdhd里的timescale / sample delta的值得到，
                                根据stts可以尖酸出每个Sample的DTS，其中sample delta为该sample的DTS相对于上一个sample的差值。比如entry_count = 1，sample_count = 5，sample_delta = 1024时，5个sample的DTS将依次为0 1024 2048 3072 4096。
 
                        【Ⅵ ctts】：
                                ctts保存了每个sample的composition time和decode time之间的差值，这里通过composition time就可以计算出sample的PTS。
                                音频Track是没有ctts的，视频Track如果编码不含B帧，PTS默认等于PTS，所以也不含ctts。ctts是一个Full Box。
                                box依次包含以下信息：
                                - box size                     4字节                 box的大小
                                - box type                    4字节                 'ctts'
                                - version                      1字节                 一般为0
                                - flag                           3字节                一般为0
                                - entry count                 4字节                描述下面二元组的个数  这里的数量可能会小于stts里的sample数量  因为可能有连续的sample的CT偏移量相同
                                - sample count               4字节                连续相同offset的sample个数
                                - sample offset              4字节                CT和DT之间的偏移量
 
                                结合stts和ctts的几组数据计算DTS和PTS：
                                Sample Num              0                 1                 2                 3                 4                 5
                                Sample Type             I                  P                 B                 P                 B                 P
                                Sample Delta            512              512             512             512             512              512
                                Sample Offset          1024             1536           512             1536           512              2560
                                DTS                        0                  512            1024            1536           2048            2560
                                PTS                        1024            2048           1536            3072           2560            5120
                                PTS（首帧归0）       0                  1024           512             2048           1536            4096
                                Video Eye                0                  1024           512             2048           1536            4096
 
                                观察上表可发现：
                                解码顺序：0 - 1 - 2 - 3 - 4 - 5    I - P - B - P - B - P
                                渲染顺序：0 - 2 - 1 - 4 - 3 - 5    I - B - P - B - P - P
 
                        【Ⅵ stss】：
                                stss存储了关键帧在Samples中的位置序号，音频Track不包含stss。stss是一个Full Box。
                                box依次包含以下信息：
                                - box size                     4字节                 box的大小
                                - box type                    4字节                 'stss'
                                - version                      1字节                 一般为0
                                - flag                           3字节                一般为0
                                - entry count                 4字节                关键帧个数
                                - sample number            4字节                 关键帧sample序号
 
                        【Ⅵ stsc】：
                                stsc主要描述Trunk的信息，每个Track划分成多个Trunk，Trunk包含多个Sample，stsc就是描述Trunk和Sample的映射关系及在Track中的分布情况，是一个Full Box。
                                box依次包含以下信息：
                                - box size                              4字节                 box的大小
                                - box type                             4字节                 'stsc'
                                - version                               1字节                 一般为0
                                - flag                                   3字节                  一般为0
                                - entry count                         4字节                 以下三元组信息的个数
                                - first chunk                          4字节                 每一个entry开始的chunk序号
                                - sample per chunk                 4字节                 每一个chunk里包含多少个sample
                                - sample description index       4字节                  每一个sample的描述，一般为1
 
                                三元组详细解释：
                                first chunk：具有相同采样点sample和sample_description_index的chunk中，第一个chunk的索引值,也就是说该chunk索引值一直到下一个索引值之间的所有chunk都具有相同的sample个数，同时这些sample的描述description也一样。
                                sample per chunk：上面所有chunk的sample个数
                                sample description index：描述采样点的采样描述项的索引值，范围为1到样本描述表中的表项数目。
                                这3个字段实际上决定了一个MP4中有多少个Chunk，每个Chunk有多少个Sample。
                                比如entry count = 1，另first chunk = 1，sample per chunk = 1，sample description index = 1，说明从第一个Chunk到最后一个Chunk都包含相同的Sample数量1，并且共享一个Sample Description，因此Chunk数量与Sample数量一致，等于stts的entry count。
 
                        【Ⅵ stsz】：
                                stsz包含了媒体中全部Sample的个数和一张给出每个Sample大小的表，这个box相对来说体积比较大，是一个Full Box。
                                box依次包含以下信息：
                                - box size                              4字节                 box的大小
                                - box type                             4字节                 'stsz'
                                - version                               1字节                 一般为0
                                - flag                                   3字节                  一般为0
                                - sample size                         4字节                  指定默认的Sample大小，如果每个Sample大小不相等，则这个字段值为0，每个Sample大小存在下表中。
                                - sample count                       4字节                  该Track中所有Sample的数量
                                - entry size                           4字节                  每个Sample的大小
 
                        【Ⅵ stco】：
                                stco存储了Chunk Offset，表示每个Chunk在文件中的位置。stco是一个Full Box。
                                box依次包含以下信息：
                                - box size                              4字节                 box的大小
                                - box type                             4字节                 'stsz'
                                - version                               1字节                 一般为0
                                - flag                                   3字节                  一般为0
                                - entry count                         4字节                 Chunk的总数量
                                - chunk offset                        4字节                 每个Chunk在文件中的位置

                                * 注：chunk offset是该Track在MP4文件中的位置偏移，不是在mdat中的位置偏移。拿一个测试文件举例来看，第一个Chunk的偏移是64003，用工具查看mdat在当前文件中的偏移量是63995，加上mdat的box size（4字节）和 box type（4字节），刚好是第一个Chunk的偏移量64003。
                                另 如果视频文件过大，有可能造成chunk offset超过4字节的限制。所以针对这种情况会额外有一个co64的box，功效等价于stco，只是chunk offset的有效位数不同。
             
 【Ⅰmdat】：
        mdat是存储音视频数据的box，要从这个box解封装出真实的媒体数据。所有媒体数据统一存放在mdat中，没有同步字，没有分隔符，只能根据索引进行访问。
        mdat的位置比较灵活，可以位于moov之前，也可以位于moov之后，但必须和stbl中的信息保持一致。
        box依次包含4个字节的box size，4字节的box类型，之后即是实际数据。
        如果box的前四个字节如果是0x00000001，表示需要使用box type之后的8个字节指示实际大小，即0x00000001 + box type（4bytes）+ box size（8bytes）。
             
  【Ⅰfree】：
        free中的内容不是必须的，可有可无，被删除后不会对播放产生任何影响。类似的box还有skip。
        box依次包含4字节的box size，4字节的box类型，之后是box包含的实际信息。
 */

#include "MP4Mediainfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include "Ap4.h"
#include "Ap4BitStream.h"
#include "Ap4Mp4AudioInfo.h"
#include "Ap4HevcParser.h"

#define BANNER "MP4 File Info - Version 1.3.4\n"\
               "(Bento4 Version " AP4_VERSION_STRING ")\n"\
               "(c) 2002-2017 Axiomatic Systems, LLC"

typedef struct {
    AP4_UI64 sample_count;
    AP4_UI64 duration;
    double bitrate;
} Mediainfo;

static void parse(const char *filename, bool verbose, bool fast, bool show_samples, bool show_sample_data, bool show_layout, bool help);
static void show_file_info(AP4_File& file);
static void show_movie_info(AP4_Movie& movie);
static void show_marlin_tracks(AP4_File& file, AP4_ByteStream& stream, AP4_List<AP4_Track>& tracks, bool show_samples, bool show_sample_data, bool verbose, bool fast);
static void show_tracks(AP4_Movie& movie, AP4_List<AP4_Track>& tracks, AP4_ByteStream& stream, bool show_samples, bool show_sample_data, bool verbose, bool fast);
static void show_sample_layout(AP4_List<AP4_Track>& tracks);
static void show_fragments(AP4_Movie& movie, bool verbose, bool show_sample_data, AP4_ByteStream *stream);
static void show_dcf_info(AP4_File& file);
static void show_track_info(AP4_Movie& movie, AP4_Track& track, AP4_ByteStream& stream, bool show_samples, bool show_sample_data, bool verbose, bool fast);
static void show_protection_scheme_info(AP4_UI32 scheme_type, AP4_ContainerAtom& schi, bool verbose);
static void show_sample(AP4_Track& track, AP4_Sample& sample, AP4_DataBuffer& sample_data, unsigned int index, bool verbose, bool show_sample_data, AP4_AvcSampleDescription *avc_desc);
static void show_sample_description(AP4_SampleDescription& description, bool verbose);
static void scan_media(AP4_Movie& movie, AP4_Track& track, AP4_ByteStream& stream, Mediainfo& info);
static void show_payload(AP4_Atom& atom, bool ascii);
static void show_avc_info(const AP4_DataBuffer& sample_data, AP4_AvcSampleDescription *avc_desc);
static void show_protected_sample_description(AP4_ProtectedSampleDescription& desc, bool verbose);
static void show_mpeg_audio_sample_description(AP4_MpegAudioSampleDescription& mpeg_audio_desc);
static void show_data(const AP4_DataBuffer& data);
static unsigned int read_golomb(AP4_BitStream& bits);

/**
 * Print Module Help
 */
static void show_module_help() {
    printf("Support Format:\n\n");
    printf("  - MP4\n");
    printf("\n");
    printf("Param:\n\n");
    printf("  --verbose:   show extended information when available\n");
    printf("  --show-layout:  show sample layout\n");
    printf("  --show-samples:  show sample details\n");
    printf("  --show-sample-data:  show sample data\n");
    printf("  --fast:  skip some details that are slow to compute\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools MP4Mediainfo input.mp4 --verbose --show-layout\n\n");
}

/**
 * Parse Cmd
 * @param argc     From main.cpp
 * @param argv     From main.cpp
 */
void mp4_mediainfo_parse_cmd(int argc, char *argv[]) {
    
    if (argc < 3) {
        printf("MP4MediaInfo Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        exit(1);
    }
    
    const char *filename = NULL;
    bool verbose = false;
    bool show_samples = false;
    bool show_sample_data = false;
    bool show_layout = false;
    bool fast = false;
    bool help = false;
    
    ++argv;
    
    while (char *arg = *++argv) {
        if (!strcmp(arg, "--verbose")) {
            verbose = true;
        } else if (!strcmp(arg, "--fast")) {
            fast = true;
        } else if (!strcmp(arg, "--show-samples")) {
            show_samples = true;
        } else if (!strcmp(arg, "--show-sample-data")) {
            show_samples = true;
            show_sample_data = true;
        } else if (!strcmp(arg, "--show-layout")) {
            show_layout = true;
        } else if (!strcmp(arg, "--help")) {
            help = true;
        } else {
            if (filename == NULL) {
                filename = arg;
            } else {
                fprintf(stderr, "ERROR: unexpected argument '%s'\n", arg);
                return;
            }
        }
    }
    
    parse(filename, verbose, fast, show_samples, show_sample_data, show_layout, help);
}

/**
 * Start Parse
 * @param filename     mp4 file path
 * @param verbose     if verbose
 * @param fast     if fast
 * @param show_samples     if show samples
 * @param show_sample_data     if show sample data
 * @param show_layout     if show layout
 * @param help     if help
 */
static void parse(const char *filename, bool verbose, bool fast, bool show_samples, bool show_sample_data, bool show_layout, bool help) {
    
    if (help) {
        show_module_help();
        return;
    }
    
    if (filename == NULL) {
        fprintf(stderr, "ERROR: filename missing\n");
        return;
    }
    
    AP4_ByteStream *input = NULL;
    AP4_Result result = AP4_FileByteStream::Create(filename, AP4_FileByteStream::STREAM_MODE_READ, input);
    
    if (AP4_FAILED(result)) {
        fprintf(stderr, "ERROR: cannot open input file %s (%d)\n", filename, result);
        return;
    }
    
    AP4_File *file = new AP4_File(*input, true);
    
    // show ftyp and if fast
    show_file_info(*file);
    
    AP4_Movie *movie = file->GetMovie();
    AP4_FtypAtom *ftyp = file->GetFileType();
    if (movie) {
        // show mvhd info
        show_movie_info(*movie);
        
        AP4_List<AP4_Track>& tracks = movie->GetTracks();
        // if MGSV
        if (ftyp && ftyp->GetMajorBrand() == AP4_MARLIN_BRAND_MGSV) {
            show_marlin_tracks(*file, *input, tracks, show_samples, show_sample_data, verbose, fast);
        } else {
            // show tracks
            show_tracks(*movie, tracks, *input, show_samples, show_sample_data, verbose, fast);
        }
        
        // show v/a layout
        if (show_layout) {
            show_sample_layout(tracks);
        }
        
        // show fragments
        if (movie->HasFragments() && show_samples) {
            show_fragments(*movie, verbose, show_sample_data, input);
        }
    } else {
        if (ftyp && ftyp->GetMajorBrand() == AP4_OMA_DCF_BRAND_ODCF) {
            show_dcf_info(*file);
        } else {
            printf("No movie found in the file\n");
        }
    }
    
    input->Release();
    delete file;
}

/**
 * Show File Info
 * @param file     AP4_File Instance
 */
static void show_file_info(AP4_File& file) {
    AP4_FtypAtom *file_type = file.GetFileType();
    if (file_type == NULL) return;
    char four_cc[5];
    
    AP4_FormatFourChars(four_cc, file_type->GetMajorBrand());
    printf("File:\n");
    printf("  major brand:      %s\n", four_cc);
    printf("  minor version:    %x\n", file_type->GetMinorVersion());
    
    for (unsigned int i = 0; i < file_type->GetCompatibleBrands().ItemCount(); i++) {
        AP4_UI32 cb = file_type->GetCompatibleBrands()[i];
        if (cb == 0) continue;
        AP4_FormatFourChars(four_cc, cb);
        printf("  compatible brand: %s\n", four_cc);
    }
    
    printf("  fast start:       %s\n\n", file.IsMoovBeforeMdat() ? "yes" : "no");
}

/**
 * Show Movie Info
 * @param movie     AP4_Movie Instance
 */
static void show_movie_info(AP4_Movie& movie) {
    printf("Movie:\n");
    printf("  duration:   %lld (media timescale units)\n", movie.GetDuration());
    printf("  duration:   %d (ms)\n", movie.GetDurationMs());
    printf("  time scale: %d\n", movie.GetTimeScale());
    printf("  fragments:  %s\n", movie.HasFragments()?"yes":"no");
    printf("\n");
}

/**
 * Show Marlin Tracks
 * @param file     AP4_File Instance
 * @param stream     AP4_ByteStream Instance
 * @param tracks     AP4_List Instance
 * @param show_samples     bool
 * @param show_sample_data     bool
 * @param verbose    bool
 * @param fast   bool
 */
static void show_marlin_tracks(AP4_File& file, AP4_ByteStream& stream, AP4_List<AP4_Track>& tracks, bool show_samples, bool show_sample_data, bool verbose, bool fast) {
    
    AP4_List<AP4_MarlinIpmpParser::SinfEntry> sinf_entries;
    AP4_Result result = AP4_MarlinIpmpParser::Parse(file, stream, sinf_entries);
    
    if (AP4_FAILED(result)) {
        printf("WARNING: cannot parse Marlin IPMP info\n");
        show_tracks(*file.GetMovie(), tracks, stream, show_samples, show_sample_data, verbose, fast);
        return;
    }
    
    int index = 1;
    for (AP4_List<AP4_Track>::Item *track_item = tracks.FirstItem(); track_item; track_item = track_item->GetNext(), ++index) {
        printf("Track %d:\n", index);
        AP4_Track *track = track_item->GetData();
        show_track_info(*file.GetMovie(), *track, stream, show_samples, show_sample_data, verbose, fast);
        
        for (AP4_List<AP4_MarlinIpmpParser::SinfEntry>::Item *sinf_entry_item = sinf_entries.FirstItem(); sinf_entry_item; sinf_entry_item = sinf_entry_item->GetNext()) {
            AP4_MarlinIpmpParser::SinfEntry *sinf_entry = sinf_entry_item->GetData();
            if (sinf_entry->m_TrackId == track->GetId()) {
                printf("    [ENCRYPTED]\n");
                if (sinf_entry->m_Sinf == NULL) {
                    printf("WARNING: NULL sinf entry for track ID %d\n", track->GetId());
                } else {
                    AP4_ContainerAtom *schi = AP4_DYNAMIC_CAST(AP4_ContainerAtom, sinf_entry->m_Sinf->GetChild(AP4_ATOM_TYPE_SCHI));
                    AP4_SchmAtom *schm = AP4_DYNAMIC_CAST(AP4_SchmAtom, sinf_entry->m_Sinf->GetChild(AP4_ATOM_TYPE_SCHM));
                    if (schm && schi) {
                        AP4_UI32 scheme_type = schm->GetSchemeType();
                        printf("      Scheme Type:    %c%c%c%c\n",
                            (char)((scheme_type>>24) & 0xFF),
                            (char)((scheme_type>>16) & 0xFF),
                            (char)((scheme_type>> 8) & 0xFF),
                            (char)((scheme_type    ) & 0xFF));
                        printf("      Scheme Version: %d\n", schm->GetSchemeVersion());
                        show_protection_scheme_info(scheme_type, *schi, verbose);
                    } else {
                        if (schm == NULL) printf("WARNING: schm atom is NULL\n");
                        if (schi == NULL) printf("WARNING: schi atom is NULL\n");
                    }
                }
            }
        }
    }
    sinf_entries.DeleteReferences();
}

/**
 * Show Tracks
 * @param movie     AP4_Movie Instance
 * @param stream     AP4_ByteStream Instance
 * @param tracks     AP4_List Instance
 * @param show_samples     bool
 * @param show_sample_data     bool
 * @param verbose    bool
 * @param fast   bool
 */
static void show_tracks(AP4_Movie& movie, AP4_List<AP4_Track>& tracks, AP4_ByteStream& stream, bool show_samples, bool show_sample_data, bool verbose, bool fast) {
    
    int index = 1;
    for (AP4_List<AP4_Track>::Item *track_item = tracks.FirstItem(); track_item; track_item = track_item->GetNext(), ++index) {
        printf("Track %d:\n", index);
        show_track_info(movie, *track_item->GetData(), stream, show_samples, show_sample_data, verbose, fast);
    }
}

/**
 * Show Sample Layout
 * @param tracks     AP4_List Instance
 */
static void show_sample_layout(AP4_List<AP4_Track>& tracks) {
    AP4_Array<int> cursors;
    cursors.SetItemCount(tracks.ItemCount());
    for (unsigned int i=0; i<tracks.ItemCount(); i++) {
        cursors[i] = 0;
    }
    
    AP4_Sample sample;
    AP4_UI64 sample_offset = 0;
    AP4_UI32 sample_size = 0;
    AP4_UI64 sample_dts = 0;
    bool sample_is_sync = false;
    bool indicator = true;
    AP4_Track *previous_track = NULL;
    for (unsigned int i = 0;; i++) {
        AP4_UI64 min_offset = (AP4_UI64)(-1);
        int chosen_index = -1;
        AP4_Track *chosen_track = NULL;
        AP4_Ordinal index = 0;
        for (AP4_List<AP4_Track>::Item *track_item = tracks.FirstItem(); track_item; track_item = track_item->GetNext()) {
            AP4_Track *track = track_item->GetData();
            AP4_Result result = track->GetSample(cursors[index], sample);
            if (AP4_SUCCEEDED(result)) {
                if (sample.GetOffset() < min_offset) {
                    chosen_index = index;
                    chosen_track = track;
                    sample_offset = sample.GetOffset();
                    sample_size = sample.GetSize();
                    sample_dts = sample.GetDts();
                    sample_is_sync = sample.IsSync();
                    min_offset = sample_offset;
                }
            }
            index++;
        }
        
        // stop if we've exhausted all samples
        if (chosen_index == -1) break;
        
        // update the cursor for the chsen track
        cursors[chosen_index] = cursors[chosen_index] + 1;
        
        // see if we've changed tracks
        if (previous_track != chosen_track) {
            previous_track = chosen_track;
            indicator = !indicator;
        }
        
        // show the chosen track/sample
        char track_type = ' ';
        switch (chosen_track->GetType()) {
            case AP4_Track::TYPE_AUDIO:     track_type = 'A'; break;
            case AP4_Track::TYPE_VIDEO:     track_type = 'V'; break;
            case AP4_Track::TYPE_HINT:      track_type = 'H'; break;
            case AP4_Track::TYPE_TEXT:      track_type = 'T'; break;
            case AP4_Track::TYPE_SYSTEM:    track_type = 'S'; break;
            case AP4_Track::TYPE_SUBTITLES: track_type = 'U'; break;
            default:                        track_type = ' '; break;
        }
        AP4_UI64 sample_dts_ms = AP4_ConvertTime(sample_dts, chosen_track->GetMediaTimeScale(), 1000);
        printf("%c %08d [%c] (%d)%c size=%6d, offset=%8lld, dts=%lld (%lld ms)\n",
               indicator?'|':' ',
               i,
               track_type,
               chosen_track->GetId(),
               sample_is_sync?'*':' ',
               sample_size,
               sample_offset,
               sample_dts,
               sample_dts_ms);
    }
}

/**
 * Show Fragments
 * @param movie     AP4_Movie Instance
 * @param verbose     bool
 * @param show_sample_data     bool
 * @param stream     AP4_ByteStream Instance
 */
static void show_fragments(AP4_Movie& movie, bool verbose, bool show_sample_data, AP4_ByteStream *stream) {
    stream->Seek(0);
    AP4_LinearReader reader(movie, stream);
    AP4_List<AP4_Track>::Item *track_item = movie.GetTracks().FirstItem();
    AP4_Array<unsigned int> counters;
    while (track_item) {
        reader.EnableTrack(track_item->GetData()->GetId());
        track_item = track_item->GetNext();
        counters.Append(0);
    }
    
    AP4_Sample sample;
    AP4_DataBuffer sample_data;
    AP4_UI32 prev_track_id = 0;
    unsigned int *counter = &counters[0];
    for (;;) {
        AP4_UI32 track_id = 0;
        AP4_Result result = reader.ReadNextSample(sample, sample_data, track_id);
        if (AP4_SUCCEEDED(result)) {
            AP4_Track *track = movie.GetTrack(track_id);
            AP4_SampleDescription *sample_desc = track->GetSampleDescription(sample.GetDescriptionIndex());
            AP4_AvcSampleDescription *avc_desc = AP4_DYNAMIC_CAST(AP4_AvcSampleDescription, sample_desc);
            
            if (track_id != prev_track_id) {
                printf("Track %d:\n", track_id);
                prev_track_id = track_id;
                
                // find the right counter for this track
                unsigned int counter_index = 0;
                for (AP4_List<AP4_Track>::Item *track_item = movie.GetTracks().FirstItem(); track_item; track_item = track_item->GetNext(), ++counter_index) {
                    if (track_item->GetData()->GetId() == track_id) {
                        if (counter_index < counters.ItemCount()) {
                            counter = &counters[counter_index];
                        }
                        break;
                    }
                }
            }
            
            show_sample(*track, sample, sample_data, *counter, verbose, show_sample_data, avc_desc);
            printf("\n");
            
            ++*counter;
        } else {
            break;
        }
    }
}

/**
 * Show Dcf Info
 * @param file     AP4_File Instance
 */
static void show_dcf_info(AP4_File& file) {
    AP4_FtypAtom *ftyp = file.GetFileType();
    if (ftyp == NULL) return;
    printf("OMA DCF File, version=%d\n", ftyp->GetMinorVersion());
    if (ftyp->GetMinorVersion() != 2) return;
    
    AP4_OdheAtom *odhe = AP4_DYNAMIC_CAST(AP4_OdheAtom, file.FindChild("odrm/odhe"));
    if (odhe) {
        printf("Content Type:      %s\n", odhe->GetContentType().GetChars());
    }
    AP4_OhdrAtom *ohdr = AP4_DYNAMIC_CAST(AP4_OhdrAtom, file.FindChild("odrm/odhe/ohdr"));
    if (ohdr) {
        printf("Encryption Method: ");
        switch (ohdr->GetEncryptionMethod()) {
            case AP4_OMA_DCF_ENCRYPTION_METHOD_NULL:    printf("NULL\n");        break;
            case AP4_OMA_DCF_ENCRYPTION_METHOD_AES_CBC: printf("AES-128-CBC\n"); break;
            case AP4_OMA_DCF_ENCRYPTION_METHOD_AES_CTR: printf("AES-128-CTR\n"); break;
            default:                                    printf("%d\n", ohdr->GetEncryptionMethod());
        }
        printf("Padding Scheme:    ");
        switch (ohdr->GetPaddingScheme()) {
            case AP4_OMA_DCF_PADDING_SCHEME_NONE:     printf("NONE\n"); break;
            case AP4_OMA_DCF_PADDING_SCHEME_RFC_2630: printf("RFC 2630\n"); break;
            default:                                  printf("%d\n", ohdr->GetPaddingScheme());
        }
        printf("Content ID:        %s\n", ohdr->GetContentId().GetChars());
        printf("Rights Issuer URL: %s\n", ohdr->GetRightsIssuerUrl().GetChars());
        printf("Textual Headers:\n");
        
        AP4_Size    headers_size = ohdr->GetTextualHeaders().GetDataSize();
        const char* headers = (const char*)ohdr->GetTextualHeaders().GetData();
        while (headers_size) {
            printf("  %s\n", headers);
            AP4_Size header_len = (AP4_Size)strlen(headers);
            headers_size -= header_len+1;
            headers      += header_len+1;
        }
        AP4_GrpiAtom* grpi = AP4_DYNAMIC_CAST(AP4_GrpiAtom, ohdr->GetChild(AP4_ATOM_TYPE_GRPI));
        if (grpi) {
            printf("Group ID:          %s\n", grpi->GetGroupId().GetChars());
        }
    }
}

/**
 * Show Track Info
 * @param movie     AP4_Movie Instance
 * @param track     AP4_Track Instance
 * @param stream     AP4_ByteStream Instance
 * @param show_samples     bool
 * @param show_sample_data    bool
 * @param verbose    bool
 * @param fast    bool
 */
static void show_track_info(AP4_Movie& movie, AP4_Track& track, AP4_ByteStream& stream, bool show_samples, bool show_sample_data, bool verbose, bool fast) {
    
    printf("  flags:        %d", track.GetFlags());
    if (track.GetFlags() & AP4_TRACK_FLAG_ENABLED) {
        printf(" ENABLED");
    }
    if (track.GetFlags() & AP4_TRACK_FLAG_IN_MOVIE) {
        printf(" IN-MOVIE");
    }
    if (track.GetFlags() & AP4_TRACK_FLAG_IN_PREVIEW) {
        printf(" IN-PREVIEW");
    }
    printf("\n");
    printf("  id:           %d\n", track.GetId());
    printf("  type:         ");
    switch (track.GetType()) {
        case AP4_Track::TYPE_AUDIO:     printf("Audio\n");     break;
        case AP4_Track::TYPE_VIDEO:     printf("Video\n");     break;
        case AP4_Track::TYPE_HINT:      printf("Hint\n");      break;
        case AP4_Track::TYPE_SYSTEM:    printf("System\n");    break;
        case AP4_Track::TYPE_TEXT:      printf("Text\n");      break;
        case AP4_Track::TYPE_JPEG:      printf("JPEG\n");      break;
        case AP4_Track::TYPE_SUBTITLES: printf("Subtitles\n"); break;
        default: {
            char hdlr[5];
            AP4_FormatFourChars(hdlr, track.GetHandlerType());
            printf("Unknown [");
            printf("%s", hdlr);
            printf("]\n");
            break;
        }
    }
    printf("  duration: %d ms\n", track.GetDurationMs());
    printf("  language: %s\n", track.GetTrackLanguage());
    printf("  media:\n");
    printf("    sample count: %d\n", track.GetSampleCount());
    printf("    timescale:    %d\n", track.GetMediaTimeScale());
    printf("    duration:     %lld (media timescale units)\n", track.GetMediaDuration());
    printf("    duration:     %d (ms)\n", (AP4_UI32)AP4_ConvertTime(track.GetMediaDuration(), track.GetMediaTimeScale(), 1000));
    if (!fast) {
        Mediainfo media_info;
        scan_media(movie, track, stream, media_info);
        printf("    bitrate (computed): %.3f Kbps\n", media_info.bitrate/1000.0);
        if (movie.HasFragments()) {
            printf("    sample count with fragments: %lld\n", media_info.sample_count);
            printf("    duration with fragments:     %lld\n", media_info.duration);
            printf("    duration with fragments:     %d (ms)\n", (AP4_UI32)AP4_ConvertTime(media_info.duration, track.GetMediaTimeScale(), 1000));
        }
    }
    if (track.GetWidth()  || track.GetHeight()) {
        printf("  display width:  %f\n", (float)track.GetWidth()/65536.0);
        printf("  display height: %f\n", (float)track.GetHeight()/65536.0);
    }
    if (track.GetType() == AP4_Track::TYPE_VIDEO && track.GetSampleCount()) {
        printf("  frame rate (computed): %.3f\n", (float)track.GetSampleCount()/
                                                  ((float)track.GetMediaDuration()/(float)track.GetMediaTimeScale()));
    }
    
    // show all sample descriptions
    AP4_AvcSampleDescription* avc_desc = NULL;
    for (unsigned int desc_index=0;
        AP4_SampleDescription* sample_desc = track.GetSampleDescription(desc_index);
        desc_index++) {
        printf("  Sample Description %d\n", desc_index);
        show_sample_description(*sample_desc, verbose);
        avc_desc = AP4_DYNAMIC_CAST(AP4_AvcSampleDescription, sample_desc);
    }

    // show samples if requested
    if (show_samples) {
        AP4_Sample     sample;
        AP4_DataBuffer sample_data;
        AP4_Ordinal    index = 0;
        while (AP4_SUCCEEDED(track.GetSample(index, sample))) {
            if (avc_desc || show_sample_data) {
                sample.ReadData(sample_data);
            }

            show_sample(track, sample, sample_data, index, verbose, show_sample_data, avc_desc);
            printf("\n");
            index++;
        }
    }
}

/**
 * Scan Media
 * @param movie     AP4_Movie Instance
 * @param track     AP4_Track Instance
 * @param stream     AP4_ByteStream Instance
 * @param info     Mediainfo
 */
static void scan_media(AP4_Movie& movie, AP4_Track& track, AP4_ByteStream& stream, Mediainfo& info) {
    
    AP4_UI64 total_size = 0;
    AP4_UI64 total_duration = 0;
    
    AP4_UI64 position;
    stream.Tell(position);
    stream.Seek(0);
    AP4_LinearReader reader(movie, &stream);
    reader.EnableTrack(track.GetId());
    
    info.sample_count = 0;
    
    AP4_Sample sample;
    if (movie.HasFragments()) {
        AP4_DataBuffer sample_data;
        for(unsigned int i=0; ; i++) {
            AP4_UI32 track_id = 0;
            AP4_Result result = reader.ReadNextSample(sample, sample_data, track_id);
            if (AP4_SUCCEEDED(result)) {
                total_size += sample.GetSize();
                total_duration += sample.GetDuration();
                ++info.sample_count;
            } else {
                break;
            }
        }
    } else {
        info.sample_count = track.GetSampleCount();
        for (unsigned int i=0; i<track.GetSampleCount(); i++) {
            if (AP4_SUCCEEDED(track.GetSample(i, sample))) {
                total_size += sample.GetSize();
            }
        }
        total_duration = track.GetMediaDuration();
    }
    info.duration = total_duration;
    
    double duration_ms = (double)AP4_ConvertTime(total_duration, track.GetMediaTimeScale(), 1000);
    if (duration_ms) {
        info.bitrate = 8.0*1000.0*(double)total_size/duration_ms;
    } else {
        info.bitrate = 0.0;
    }
}

/**
 * Show Protection Scheme Info
 * @param scheme_type     AP4_UI32 Instance
 * @param schi     AP4_ContainerAtom Instance
 * @param verbose     bool
 */
static void show_protection_scheme_info(AP4_UI32 scheme_type, AP4_ContainerAtom& schi, bool verbose) {
    
    if (scheme_type == AP4_PROTECTION_SCHEME_TYPE_IAEC) {
        printf("      iAEC Scheme Info:\n");
        AP4_IkmsAtom* ikms = AP4_DYNAMIC_CAST(AP4_IkmsAtom, schi.FindChild("iKMS"));
        if (ikms) {
            printf("        KMS URI:              %s\n", ikms->GetKmsUri().GetChars());
        }
        AP4_IsfmAtom* isfm = AP4_DYNAMIC_CAST(AP4_IsfmAtom, schi.FindChild("iSFM"));
        if (isfm) {
            printf("        Selective Encryption: %s\n", isfm->GetSelectiveEncryption()?"yes":"no");
            printf("        Key Indicator Length: %d\n", isfm->GetKeyIndicatorLength());
            printf("        IV Length:            %d\n", isfm->GetIvLength());
        }
        AP4_IsltAtom* islt = AP4_DYNAMIC_CAST(AP4_IsltAtom, schi.FindChild("iSLT"));
        if (islt) {
            printf("        Salt:                 ");
            for (unsigned int i=0; i<8; i++) {
                printf("%02x",islt->GetSalt()[i]);
            }
            printf("\n");
        }
    } else if (scheme_type == AP4_PROTECTION_SCHEME_TYPE_OMA) {
        printf("      odkm Scheme Info:\n");
        AP4_OdafAtom* odaf = AP4_DYNAMIC_CAST(AP4_OdafAtom, schi.FindChild("odkm/odaf"));
        if (odaf) {
            printf("        Selective Encryption: %s\n", odaf->GetSelectiveEncryption()?"yes":"no");
            printf("        Key Indicator Length: %d\n", odaf->GetKeyIndicatorLength());
            printf("        IV Length:            %d\n", odaf->GetIvLength());
        }
        AP4_OhdrAtom* ohdr = AP4_DYNAMIC_CAST(AP4_OhdrAtom, schi.FindChild("odkm/ohdr"));
        if (ohdr) {
            const char* encryption_method = "";
            switch (ohdr->GetEncryptionMethod()) {
                case AP4_OMA_DCF_ENCRYPTION_METHOD_NULL:    encryption_method = "NULL";    break;
                case AP4_OMA_DCF_ENCRYPTION_METHOD_AES_CTR: encryption_method = "AES-CTR"; break;
                case AP4_OMA_DCF_ENCRYPTION_METHOD_AES_CBC: encryption_method = "AES-CBC"; break;
                default:                                    encryption_method = "UNKNOWN"; break;
            }
            printf("        Encryption Method: %s\n", encryption_method);
            printf("        Content ID:        %s\n", ohdr->GetContentId().GetChars());
            printf("        Rights Issuer URL: %s\n", ohdr->GetRightsIssuerUrl().GetChars());
            
            const AP4_DataBuffer& headers = ohdr->GetTextualHeaders();
            AP4_Size              data_len    = headers.GetDataSize();
            if (data_len) {
                AP4_Byte*      textual_headers_string;
                AP4_Byte*      curr;
                AP4_DataBuffer output_buffer;
                output_buffer.SetDataSize(data_len+1);
                AP4_CopyMemory(output_buffer.UseData(), headers.GetData(), data_len);
                curr = textual_headers_string = output_buffer.UseData();
                textual_headers_string[data_len] = '\0';
                while(curr < textual_headers_string+data_len) {
                    if ('\0' == *curr) {
                        *curr = '\n';
                    }
                    curr++;
                }
                printf("        Textual Headers: \n%s\n", (const char*)textual_headers_string);
            }
        }
    } else if (scheme_type == AP4_PROTECTION_SCHEME_TYPE_ITUNES) {
        printf("      itun Scheme Info:\n");
        AP4_Atom* name = schi.FindChild("name");
        if (name) {
            printf("        Name:    ");
            show_payload(*name, true);
            printf("\n");
        }
        AP4_Atom* user = schi.FindChild("user");
        if (user) {
            printf("        User ID: ");
            show_payload(*user, false);
            printf("\n");
        }
        AP4_Atom* key = schi.FindChild("key ");
        if (key) {
            printf("        Key ID:  ");
            show_payload(*key, false);
            printf("\n");
        }
        AP4_Atom* iviv = schi.FindChild("iviv");
        if (iviv) {
            printf("        IV:      ");
            show_payload(*iviv, false);
            printf("\n");
        }
    } else if (scheme_type == AP4_PROTECTION_SCHEME_TYPE_MARLIN_ACBC ||
               scheme_type == AP4_PROTECTION_SCHEME_TYPE_MARLIN_ACGK) {
        printf("      Marlin IPMP ACBC/ACGK Scheme Info:\n");
        AP4_NullTerminatedStringAtom* octopus_id = AP4_DYNAMIC_CAST(AP4_NullTerminatedStringAtom, schi.FindChild("8id "));
        if (octopus_id) {
            printf("        Content ID: %s\n", octopus_id->GetValue().GetChars());
        }
    }
    
    if (verbose) {
        printf("    Protection System Details:\n");
        AP4_ByteStream* output = NULL;
        AP4_FileByteStream::Create("-stdout", AP4_FileByteStream::STREAM_MODE_WRITE, output);
        AP4_PrintInspector inspector(*output, 4);
        schi.Inspect(inspector);
        output->Release();
    }
}

/**
 * Show Sample
 * @param track     AP4_Track Instance
 * @param sample     AP4_Sample Instance
 * @param sample_data     AP4_DataBuffer
 * @param index     bool
 * @param verbose     bool
 * @param show_sample_data     bool
 * @param avc_desc     AP4_AvcSampleDescription
 */
static void show_sample(AP4_Track& track, AP4_Sample& sample, AP4_DataBuffer& sample_data, unsigned int index, bool verbose, bool show_sample_data, AP4_AvcSampleDescription *avc_desc) {
    
    printf("[%02d.%06d] size=%6d duration=%6d",
           track.GetId(),
           index+1,
           (int)sample.GetSize(),
           (int)sample.GetDuration());
    if (verbose) {
        printf(" (%6d ms) offset=%10lld dts=%10lld (%10lld ms) cts=%10lld (%10lld ms) [%d]",
               (int)AP4_ConvertTime(sample.GetDuration(), track.GetMediaTimeScale(), 1000),
               sample.GetOffset(),
               sample.GetDts(),
               AP4_ConvertTime(sample.GetDts(), track.GetMediaTimeScale(), 1000),
               sample.GetCts(),
               AP4_ConvertTime(sample.GetCts(), track.GetMediaTimeScale(), 1000),
               sample.GetDescriptionIndex());
    }
    if (sample.IsSync()) {
        printf(" [S] ");
    } else {
        printf("     ");
    }
    if (avc_desc || show_sample_data) {
        sample.ReadData(sample_data);
    }
    if (avc_desc) {
        show_avc_info(sample_data, avc_desc);
    }
    if (show_sample_data) {
        unsigned int show = sample_data.GetDataSize();
        if (!verbose) {
            if (show > 12) show = 12; // max first 12 chars
        }
        
        for (unsigned int i=0; i<show; i++) {
            if (verbose) {
                if (i%16 == 0) {
                    printf("\n%06d: ", i);
                }
            }
            printf("%02x", sample_data.GetData()[i]);
            if (verbose) printf(" ");
        }
        if (show != sample_data.GetDataSize()) {
            printf("...");
        }
    }
}

/**
 * Show Sample Description
 * @param description     AP4_SampleDescription Instance
 * @param verbose     bool
 */
static void show_sample_description(AP4_SampleDescription& description, bool verbose) {
    
    AP4_SampleDescription* desc = &description;
    if (desc->GetType() == AP4_SampleDescription::TYPE_PROTECTED) {
        AP4_ProtectedSampleDescription* prot_desc = AP4_DYNAMIC_CAST(AP4_ProtectedSampleDescription, desc);
        if (prot_desc) {
            show_protected_sample_description(*prot_desc, verbose);
            desc = prot_desc->GetOriginalSampleDescription();
        }
    }
    
    if (verbose) {
        printf("    Bytes: ");
        AP4_Atom* details = desc->ToAtom();
        show_payload(*details, false);
        printf("\n");
        delete details;
    }
    
    char coding[5];
    AP4_FormatFourChars(coding, desc->GetFormat());
    printf(    "    Coding:       %s", coding);
    const char* format_name = AP4_GetFormatName(desc->GetFormat());
    if (format_name) {
        printf(" (%s)\n", format_name);
    } else {
        printf("\n");
    }
    AP4_String codec;
    desc->GetCodecString(codec);
    printf(    "    Codec String: %s\n", codec.GetChars());
    
    switch (desc->GetType()) {
      case AP4_SampleDescription::TYPE_MPEG: {
        // MPEG sample description
        AP4_MpegSampleDescription* mpeg_desc = AP4_DYNAMIC_CAST(AP4_MpegSampleDescription, desc);

        printf("    Stream Type: %s\n", mpeg_desc->GetStreamTypeString(mpeg_desc->GetStreamType()));
        printf("    Object Type: %s\n", mpeg_desc->GetObjectTypeString(mpeg_desc->GetObjectTypeId()));
        printf("    Max Bitrate: %d\n", mpeg_desc->GetMaxBitrate());
        printf("    Avg Bitrate: %d\n", mpeg_desc->GetAvgBitrate());
        printf("    Buffer Size: %d\n", mpeg_desc->GetBufferSize());
        
        if (mpeg_desc->GetObjectTypeId() == AP4_OTI_MPEG4_AUDIO          ||
            mpeg_desc->GetObjectTypeId() == AP4_OTI_MPEG2_AAC_AUDIO_LC   ||
            mpeg_desc->GetObjectTypeId() == AP4_OTI_MPEG2_AAC_AUDIO_MAIN) {
            AP4_MpegAudioSampleDescription* mpeg_audio_desc = AP4_DYNAMIC_CAST(AP4_MpegAudioSampleDescription, mpeg_desc);
            if (mpeg_audio_desc) show_mpeg_audio_sample_description(*mpeg_audio_desc);
        }
        break;
      }

      case AP4_SampleDescription::TYPE_AVC: {
        // AVC specifics
        AP4_AvcSampleDescription* avc_desc = AP4_DYNAMIC_CAST(AP4_AvcSampleDescription, desc);
        const char* profile_name = AP4_AvccAtom::GetProfileName(avc_desc->GetProfile());
        printf("    AVC Profile:          %d", avc_desc->GetProfile());
        if (profile_name) {
            printf(" (%s)\n", profile_name);
        } else {
            printf("\n");
        }
        printf("    AVC Profile Compat:   %x\n", avc_desc->GetProfileCompatibility());
        printf("    AVC Level:            %d\n", avc_desc->GetLevel());
        printf("    AVC NALU Length Size: %d\n", avc_desc->GetNaluLengthSize());
        printf("    AVC SPS: [");
        const char* sep = "";
        for (unsigned int i=0; i<avc_desc->GetSequenceParameters().ItemCount(); i++) {
            printf("%s", sep);
            show_data(avc_desc->GetSequenceParameters()[i]);
            sep = ", ";
        }
        printf("]\n");
        printf("    AVC PPS: [");
        sep = "";
        for (unsigned int i=0; i<avc_desc->GetPictureParameters().ItemCount(); i++) {
            printf("%s", sep);
            show_data(avc_desc->GetPictureParameters()[i]);
            sep = ", ";
        }
        printf("]\n");
        break;
      }

      case AP4_SampleDescription::TYPE_HEVC: {
        // HEVC specifics
        AP4_HevcSampleDescription* hevc_desc = AP4_DYNAMIC_CAST(AP4_HevcSampleDescription, desc);
        const char* profile_name = AP4_HvccAtom::GetProfileName(hevc_desc->GetGeneralProfileSpace(), hevc_desc->GetGeneralProfile());
        printf("    HEVC Profile Space:       %d\n", hevc_desc->GetGeneralProfileSpace());
        printf("    HEVC Profile:             %d", hevc_desc->GetGeneralProfile());
        if (profile_name) printf(" (%s)", profile_name);
        printf("\n");
        printf("    HEVC Profile Compat:      %x\n", hevc_desc->GetGeneralProfileCompatibilityFlags());
        printf("    HEVC Level:               %d.%d\n", hevc_desc->GetGeneralLevel()/30, (hevc_desc->GetGeneralLevel()%30)/3);
        printf("    HEVC Tier:                %d\n", hevc_desc->GetGeneralTierFlag());
        printf("    HEVC Chroma Format:       %d", hevc_desc->GetChromaFormat());
        const char* chroma_format_name = AP4_HvccAtom::GetChromaFormatName(hevc_desc->GetChromaFormat());
        if (chroma_format_name) printf(" (%s)", chroma_format_name);
        printf("\n");
        printf("    HEVC Chroma Bit Depth:    %d\n", hevc_desc->GetChromaBitDepth());
        printf("    HEVC Luma Bit Depth:      %d\n", hevc_desc->GetLumaBitDepth());
        printf("    HEVC Average Frame Rate:  %d\n", hevc_desc->GetAverageFrameRate());
        printf("    HEVC Constant Frame Rate: %d\n", hevc_desc->GetConstantFrameRate());
        printf("    HEVC NALU Length Size:    %d\n", hevc_desc->GetNaluLengthSize());
        printf("    HEVC Sequences:\n");
        for (unsigned int i=0; i<hevc_desc->GetSequences().ItemCount(); i++) {
            const AP4_HvccAtom::Sequence& seq = hevc_desc->GetSequences()[i];
            printf("      {\n");
            printf("        Array Completeness=%d\n", seq.m_ArrayCompleteness);
            printf("        Type=%d", seq.m_NaluType);
            const char* nalu_type_name = AP4_HevcNalParser::NaluTypeName(seq.m_NaluType);
            if (nalu_type_name) {
                printf(" (%s)", nalu_type_name);
            }
            printf("\n");
            const char* sep = "";
            for (unsigned int j=0; j<seq.m_Nalus.ItemCount(); j++) {
                printf("%s        ", sep);
                show_data(seq.m_Nalus[j]);
                sep = "\n";
            }
            printf("\n      }\n");
        }
        break;
      }

      case AP4_SampleDescription::TYPE_AV1: {
        // AV1 specifics
        AP4_Av1SampleDescription* av1_desc = AP4_DYNAMIC_CAST(AP4_Av1SampleDescription, desc);
        const char* profile_name = AP4_Av1cAtom::GetProfileName(av1_desc->GetSeqProfile());
        printf("    AV1 Profile: %d", av1_desc->GetSeqProfile());
        if (profile_name) {
            printf(" (%s)\n", profile_name);
        } else {
            printf("\n");
        }
        printf("    AV1 Level:   %d.%d\n",
               2 + (av1_desc->GetSeqLevelIdx0() / 4),
               av1_desc->GetSeqLevelIdx0() % 4);
        printf("    AV1 Tier:    %d\n", av1_desc->GetSeqTier0());
        break;
      }
      
      case AP4_SampleDescription::TYPE_SUBTITLES: {
        // Subtitles
        AP4_SubtitleSampleDescription* subt_desc = AP4_DYNAMIC_CAST(AP4_SubtitleSampleDescription, desc);
        printf("    Subtitles:\n");
        printf("       Namespace:       %s\n", subt_desc->GetNamespace().GetChars());
        printf("       Schema Location: %s\n", subt_desc->GetSchemaLocation().GetChars());
        printf("       Image Mime Type: %s\n", subt_desc->GetImageMimeType().GetChars());
        break;
      }

      default:
        break;
    }
    
    AP4_AudioSampleDescription* audio_desc =
        AP4_DYNAMIC_CAST(AP4_AudioSampleDescription, desc);
    if (audio_desc) {
        // Audio sample description
        printf("    Sample Rate: %d\n", audio_desc->GetSampleRate());
        printf("    Sample Size: %d\n", audio_desc->GetSampleSize());
        printf("    Channels:    %d\n", audio_desc->GetChannelCount());
    }
    AP4_VideoSampleDescription* video_desc =
        AP4_DYNAMIC_CAST(AP4_VideoSampleDescription, desc);
    if (video_desc) {
        // Video sample description
        printf("    Width:       %d\n", video_desc->GetWidth());
        printf("    Height:      %d\n", video_desc->GetHeight());
        printf("    Depth:       %d\n", video_desc->GetDepth());
    }

    switch (desc->GetFormat()) {
      case AP4_SAMPLE_FORMAT_AC_3: {
        // Dolby Digital specifics
        AP4_Dac3Atom* dac3 = AP4_DYNAMIC_CAST(AP4_Dac3Atom, desc->GetDetails().GetChild(AP4_ATOM_TYPE_DAC3));
        if (dac3) {
            printf("    AC-3 Data Rate: %d\n", dac3->GetDataRate());
            printf("    AC-3 Stream:\n");
            printf("        fscod       = %d\n", dac3->GetStreamInfo().fscod);
            printf("        bsid        = %d\n", dac3->GetStreamInfo().bsid);
            printf("        bsmod       = %d\n", dac3->GetStreamInfo().bsmod);
            printf("        acmod       = %d\n", dac3->GetStreamInfo().acmod);
            printf("        lfeon       = %d\n", dac3->GetStreamInfo().lfeon);
            printf("    AC-3 dac3 payload: [");
            show_data(dac3->GetRawBytes());
            printf("]\n");
        }
        break;
      }

      case AP4_SAMPLE_FORMAT_EC_3: {
        // Dolby Digital Plus specifics
        AP4_Dec3Atom* dec3 = AP4_DYNAMIC_CAST(AP4_Dec3Atom, desc->GetDetails().GetChild(AP4_ATOM_TYPE_DEC3));
        if (dec3) {
            printf("    E-AC-3 Data Rate: %d\n", dec3->GetDataRate());
            for (unsigned int i=0; i<dec3->GetSubStreams().ItemCount(); i++) {
                printf("    E-AC-3 Substream %d:\n", i);
                printf("        fscod       = %d\n", dec3->GetSubStreams()[i].fscod);
                printf("        bsid        = %d\n", dec3->GetSubStreams()[i].bsid);
                printf("        bsmod       = %d\n", dec3->GetSubStreams()[i].bsmod);
                printf("        acmod       = %d\n", dec3->GetSubStreams()[i].acmod);
                printf("        lfeon       = %d\n", dec3->GetSubStreams()[i].lfeon);
                printf("        num_dep_sub = %d\n", dec3->GetSubStreams()[i].num_dep_sub);
                printf("        chan_loc    = %d\n", dec3->GetSubStreams()[i].chan_loc);
            }
            if (dec3->GetFlagEC3ExtensionTypeA()){
                printf("    Dolby Digital Plus with Dolby Atmos: Yes\n");
                printf("    Dolby Atmos Complexity Index: %d\n", dec3->GetComplexityIndexTypeA());
            } else {
                printf("    Dolby Digital Plus with Dolby Atmos: No\n");
            }
            printf("    E-AC-3 dec3 payload: [");
            show_data(dec3->GetRawBytes());
            printf("]\n");
        }
        break;
      }

      case AP4_SAMPLE_FORMAT_AC_4: {
        // Dolby AC-4 specifics
        AP4_Dac4Atom* dac4 = AP4_DYNAMIC_CAST(AP4_Dac4Atom, desc->GetDetails().GetChild(AP4_ATOM_TYPE_DAC4));
        if (dac4) {
            const AP4_Dac4Atom::Ac4Dsi& dsi = dac4->GetDsi();
            unsigned short self_contained = 0;
            if (dsi.ac4_dsi_version == 1) {
                for (unsigned int i = 0; i < dsi.d.v1.n_presentations; i++) {
                    AP4_Dac4Atom::Ac4Dsi::PresentationV1& presentation = dsi.d.v1.presentations[i];
                    if (presentation.presentation_version == 1 || presentation.presentation_version == 2) {
                        printf("    AC-4 Presentation %d:\n", i);

                        char presentation_type [64];
                        char presentation_codec[64];
                        char presentation_lang [64 + 1] = "un";
                        AP4_FormatString(presentation_codec,
                                         sizeof(presentation_codec),
                                         "ac-4.%02x.%02x.%02x",
                                         dsi.d.v1.bitstream_version,
                                         presentation.presentation_version,
                                         presentation.d.v1.mdcompat);
                        for (int sg = 0; sg < presentation.d.v1.n_substream_groups; sg++){
                            if ( presentation.d.v1.substream_groups[sg].d.v1.b_content_type &&
                                (presentation.d.v1.substream_groups[sg].d.v1.content_classifier == 0 ||presentation.d.v1.substream_groups[sg].d.v1.content_classifier == 4) &&
                                 presentation.d.v1.substream_groups[sg].d.v1.b_language_indicator){
                                     memcpy(presentation_lang, presentation.d.v1.substream_groups[sg].d.v1.language_tag_bytes, presentation.d.v1.substream_groups[sg].d.v1.n_language_tag_bytes);
                                     presentation_lang[presentation.d.v1.substream_groups[sg].d.v1.n_language_tag_bytes] = '\0';
                            }
                        }
                        if (presentation.d.v1.b_multi_pid == 0) { self_contained ++; }
                        if (presentation.d.v1.b_presentation_channel_coded == 1) {
                            AP4_FormatString(presentation_type, sizeof(presentation_type), "Channel based");
                            if (presentation.d.v1.dsi_presentation_ch_mode >= 11 && presentation.d.v1.dsi_presentation_ch_mode <= 15) {
                                AP4_FormatString(presentation_type, sizeof(presentation_type), "Channel based immsersive");
                            }
                        } else {
                            AP4_FormatString(presentation_type, sizeof(presentation_type), "Object based");
                        }
                        if (presentation.presentation_version == 2) {
                            printf("        Stream Type = Immersive stereo\n");
                        }else {
                            printf("        Stream Type = %s\n", presentation_type);
                        }
                        printf("        presentation_id = %d\n", presentation.d.v1.b_presentation_id? presentation.d.v1.presentation_id : -1);
                        printf("        Codec String = %s\n", presentation_codec);
                        printf("        presentation_channel_mask_v1 = 0x%x\n", presentation.d.v1.presentation_channel_mask_v1);
                        printf("        Dolby Atmos source = %s\n", presentation.d.v1.dolby_atmos_indicator? "Yes": "No");
                        printf("        Language = %s\n", presentation_lang);
                        printf("        Self Contained = %s\n", presentation.d.v1.b_multi_pid? "No": "Yes");
                    }
                }
            }
            
            printf("    Self Contained: %s\n", (self_contained == dsi.d.v1.n_presentations) ? "Yes": ((self_contained == 0)? "No": "Part"));
            printf("    AC-4 dac4 payload: [");
            show_data(dac4->GetRawBytes());
            printf("]\n");
        }
        break;
      }

      // VPx Specifics
      case AP4_SAMPLE_FORMAT_VP8:
      case AP4_SAMPLE_FORMAT_VP9:
      case AP4_SAMPLE_FORMAT_VP10: {
        AP4_VpccAtom* vpcc = AP4_DYNAMIC_CAST(AP4_VpccAtom, desc->GetDetails().GetChild(AP4_ATOM_TYPE_VPCC));
        if (vpcc) {
            printf("    Profile:                  %d\n", vpcc->GetProfile());
            printf("    Level:                    %d\n", vpcc->GetLevel());
            printf("    Bit Depth:                %d\n", vpcc->GetBitDepth());
            printf("    Chroma Subsampling:       %d\n", vpcc->GetChromaSubsampling());
            printf("    Colour Primaries:         %d\n", vpcc->GetColourPrimaries());
            printf("    Transfer Characteristics: %d\n", vpcc->GetTransferCharacteristics());
            printf("    Matrix Coefficients:      %d\n", vpcc->GetMatrixCoefficients());
            printf("    Video Full Range Flag:    %s\n", vpcc->GetVideoFullRangeFlag() ? "true" : "false");
        }
        break;
      }
    }
    
    // Dolby Vision specifics
    AP4_DvccAtom* dvcc = AP4_DYNAMIC_CAST(AP4_DvccAtom, desc->GetDetails().GetChild(AP4_ATOM_TYPE_DVCC));
    if (dvcc) {
        /* Dolby Vision */
        printf("    Dolby Vision:\n");
        printf("      Version:     %d.%d\n", dvcc->GetDvVersionMajor(), dvcc->GetDvVersionMinor());
        const char* profile_name = AP4_DvccAtom::GetProfileName(dvcc->GetDvProfile());
        if (profile_name) {
            printf("      Profile:     %s\n", profile_name);
        } else {
            printf("      Profile:     %d\n", dvcc->GetDvProfile());
        }
        printf("      Level:       %d\n", dvcc->GetDvLevel());
        printf("      RPU Present: %s\n", dvcc->GetRpuPresentFlag()?"true":"false");
        printf("      EL Present:  %s\n", dvcc->GetElPresentFlag()?"true":"false");
        printf("      BL Present:  %s\n", dvcc->GetBlPresentFlag()?"true":"false");
        printf("      BL Signal Compatibility ID:  %d\n", dvcc->GetDvBlSignalCompatibilityID());
    }
}

/**
 * Show AVC Info
 * @param sample_data     AP4_DataBuffer Instance
 * @param avc_desc     AP4_AvcSampleDescription Instance
 */
static void show_avc_info(const AP4_DataBuffer& sample_data, AP4_AvcSampleDescription *avc_desc) {
    
    const unsigned char* data = sample_data.GetData();
    AP4_Size             size = sample_data.GetDataSize();

    while (size >= avc_desc->GetNaluLengthSize()) {
        unsigned int nalu_length = 0;
        if (avc_desc->GetNaluLengthSize() == 1) {
            nalu_length = *data++;
            --size;
        } else if (avc_desc->GetNaluLengthSize() == 2) {
            nalu_length = AP4_BytesToUInt16BE(data);
            data += 2;
            size -= 2;
        } else if (avc_desc->GetNaluLengthSize() == 4) {
            nalu_length = AP4_BytesToUInt32BE(data);
            data += 4;
            size -= 4;
        } else {
            return;
        }
        if (nalu_length <= size) {
            size -= nalu_length;
        } else {
            size = 0;
        }
        
        switch (*data & 0x1F) {
            case 1: {
                AP4_BitStream bits;
                bits.WriteBytes(data+1, 8);
                read_golomb(bits);
                unsigned int slice_type = read_golomb(bits);
                switch (slice_type) {
                    case 0: printf("<P>");  break;
                    case 1: printf("<B>");  break;
                    case 2: printf("<I>");  break;
                    case 3:    printf("<SP>"); break;
                    case 4: printf("<SI>"); break;
                    case 5: printf("<P>");  break;
                    case 6: printf("<B>");  break;
                    case 7: printf("<I>");  break;
                    case 8:    printf("<SP>"); break;
                    case 9: printf("<SI>"); break;
                    default: printf("<S/%d>", slice_type); break;
                }
                return; // only show first slice type
            }
            
            case 5:
                printf("<I>");
                return;
        }
        
        data += nalu_length;
    }
}

/**
 * Show Protected Sample Description
 * @param desc     AP4_ProtectedSampleDescription Instance
 * @param verbose     bool
 */
static void show_protected_sample_description(AP4_ProtectedSampleDescription& desc, bool verbose) {
    
    printf("    [ENCRYPTED]\n");
    char coding[5];
    AP4_FormatFourChars(coding, desc.GetFormat());
    printf("      Coding:         %s\n", coding);
    AP4_UI32 st = desc.GetSchemeType();
    printf("      Scheme Type:    %c%c%c%c\n",
        (char)((st>>24) & 0xFF),
        (char)((st>>16) & 0xFF),
        (char)((st>> 8) & 0xFF),
        (char)((st    ) & 0xFF));
    printf("      Scheme Version: %d\n", desc.GetSchemeVersion());
    printf("      Scheme URI:     %s\n", desc.GetSchemeUri().GetChars());
    AP4_ProtectionSchemeInfo* scheme_info = desc.GetSchemeInfo();
    if (scheme_info == NULL) return;
    AP4_ContainerAtom* schi = scheme_info->GetSchiAtom();
    if (schi == NULL) return;
    show_protection_scheme_info(desc.GetSchemeType(), *schi, verbose);
}

/**
 * Show Mpeg Audio Sample Description
 * @param mpeg_audio_desc     AP4_MpegAudioSampleDescription Instance
 */
static void show_mpeg_audio_sample_description(AP4_MpegAudioSampleDescription& mpeg_audio_desc) {
    
    AP4_MpegAudioSampleDescription::Mpeg4AudioObjectType object_type =
        mpeg_audio_desc.GetMpeg4AudioObjectType();
    const char* object_type_string = AP4_MpegAudioSampleDescription::GetMpeg4AudioObjectTypeString(object_type);
    printf("    MPEG-4 Audio Object Type: %d (%s)\n", object_type, object_type_string);
    
    // Decoder Specific Info
    const AP4_DataBuffer& dsi = mpeg_audio_desc.GetDecoderInfo();
    if (dsi.GetDataSize()) {
        AP4_Mp4AudioDecoderConfig dec_config;
        AP4_Result result = dec_config.Parse(dsi.GetData(), dsi.GetDataSize());
        if (AP4_SUCCEEDED(result)) {
            printf("    MPEG-4 Audio Decoder Config:\n");
            printf("      Sampling Frequency: %d\n", dec_config.m_SamplingFrequency);
            printf("      Channels: %d\n", dec_config.m_ChannelCount);
            if (dec_config.m_Extension.m_ObjectType) {
                object_type_string = AP4_MpegAudioSampleDescription::GetMpeg4AudioObjectTypeString(
                    dec_config.m_Extension.m_ObjectType);

                printf("      Extension:\n");
                printf("        Object Type: %s\n", object_type_string);
                printf("        SBR Present: %s\n", dec_config.m_Extension.m_SbrPresent?"yes":"no");
                printf("        PS Present:  %s\n", dec_config.m_Extension.m_PsPresent?"yes":"no");
                printf("        Sampling Frequency: %d\n", dec_config.m_Extension.m_SamplingFrequency);
            }
        }
    }
}

/**
 * Show Payload
 * @param atom     AP4_Atom Instance
 * @param ascii     bool
 */
static void show_payload(AP4_Atom& atom, bool ascii) {
    
    AP4_UI64 payload_size = atom.GetSize()-8;
    if (payload_size <= 1024) {
        AP4_MemoryByteStream* payload = new AP4_MemoryByteStream();
        atom.Write(*payload);
        if (ascii) {
            // ascii
            payload->WriteUI08(0); // terminate with a NULL character
            printf("%s", (const char*)payload->GetData()+atom.GetHeaderSize());
        } else {
            // hex
            for (unsigned int i=0; i<payload_size; i++) {
                printf("%02x", (unsigned char)payload->GetData()[atom.GetHeaderSize()+i]);
            }
        }
        payload->Release();
    }
}

/**
 * Show Data
 * @param data     AP4_DataBuffer Instance
 */
static void show_data(const AP4_DataBuffer& data) {
    
    for (unsigned int i=0; i<data.GetDataSize(); i++) {
        printf("%02x", (unsigned char)data.GetData()[i]);
    }
}

/**
 * Read Golomb
 * @param bits     AP4_BitStream Instance
 */
static unsigned int read_golomb(AP4_BitStream& bits) {
    
    unsigned int leading_zeros = 0;
    while (bits.ReadBit() == 0) {
        leading_zeros++;
    }
    if (leading_zeros) {
        return (1 << leading_zeros) - 1 + bits.ReadBits(leading_zeros);
    } else {
        return 0;
    }
}
