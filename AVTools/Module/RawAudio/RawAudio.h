//
//  RawAudio.hpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/4/28.
//
// ffmpeg -i 1.mp4 -vn -ar 44100 -ac 2 -f s16le out.pcm

#include <stdio.h>

void raw_audio_parse_cmd(int argc, char *argv[]);
