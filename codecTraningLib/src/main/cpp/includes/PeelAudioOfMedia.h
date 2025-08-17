//
// Created by 王耀 on 2025/8/17.
//

#ifndef FFMPEGPRACTICE_PEELAUDIOOFMEDIA_H
#define FFMPEGPRACTICE_PEELAUDIOOFMEDIA_H

#include "BasicCommon.h"
#include "string"

using namespace std;

class PeelAudioOfMedia {

private:
    string peelAudioInfo;
    AVFormatContext *in_fmt_ctx = nullptr; // 输入文件的封装器实例

public:
    PeelAudioOfMedia();

    ~PeelAudioOfMedia();

    string peelAudioOfMedia(const char *srcPath, const char *destPath);

};


#endif //FFMPEGPRACTICE_PEELAUDIOOFMEDIA_H
