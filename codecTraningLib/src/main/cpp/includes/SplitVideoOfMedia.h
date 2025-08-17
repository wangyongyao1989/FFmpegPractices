//
// Created by 王耀 on 2025/8/17.
//

#ifndef FFMPEGPRACTICE_SPLITVIDEOOFMEDIA_H
#define FFMPEGPRACTICE_SPLITVIDEOOFMEDIA_H

#include "BasicCommon.h"
#include "string"

using namespace std;

class SplitVideoOfMedia {

private:
    string splitVideoInfo;
    AVFormatContext *in_fmt_ctx = NULL; // 输入文件的封装器实例

public:
    SplitVideoOfMedia();

    ~SplitVideoOfMedia();

    string splitVideoOfMedia(const char *srcPath, const char *destPath);

};


#endif //FFMPEGPRACTICE_SPLITVIDEOOFMEDIA_H
