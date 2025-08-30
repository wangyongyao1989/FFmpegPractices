//
// Created by wangyao on 2025/8/17.
//

#ifndef FFMPEGPRACTICE_MERGEAUDIO_H
#define FFMPEGPRACTICE_MERGEAUDIO_H

#include "BasicCommon.h"
#include "string"

using namespace std;

class MergeAudio {

private:
    string mergeAudioInfo;
    AVFormatContext *video_fmt_ctx = NULL; // 输入文件的封装器实例

public:
    MergeAudio();

    ~MergeAudio();

    string mergeAudio(const char *srcVideoPath, const char *srcAudioPath, const char *destPath);

};


#endif //FFMPEGPRACTICE_MERGEAUDIO_H
