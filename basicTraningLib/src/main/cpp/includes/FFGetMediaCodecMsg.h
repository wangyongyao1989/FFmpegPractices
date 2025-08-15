//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/8/15.
//

#ifndef FFMPEGPRACTICE_FFGETMEDIACODECMSG_H
#define FFMPEGPRACTICE_FFGETMEDIACODECMSG_H

#include "BasicCommon.h"
#include "string"

using namespace std;

/**
 * 练习三：打开一个音视频文件后，获取视频及音频的解码ID及解码器名字。
 */
class FFGetMediaCodecMsg {

private:
    AVFormatContext *fmt_ctx;
    string meidaCodecMsg;

public:
    FFGetMediaCodecMsg();

    ~FFGetMediaCodecMsg();

    string getMediaCodecMsg(const char *);

};


#endif //FFMPEGPRACTICE_FFGETMEDIACODECMSG_H
