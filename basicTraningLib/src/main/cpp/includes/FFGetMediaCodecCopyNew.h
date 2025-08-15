//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/8/15.
//

#ifndef FFMPEGPRACTICE_FFGETMEDIACODECCOPYNEW_H
#define FFMPEGPRACTICE_FFGETMEDIACODECCOPYNEW_H
#include "BasicCommon.h"
#include "string"

using namespace std;

/**
 * 练习四：把音视频流中的编码参数复制给解码器实例参数。
 */
class FFGetMediaCodecCopyNew {


private:
    AVFormatContext *fmt_ctx = nullptr;
    string infoMsg;

public:
    string getMediaCodecCopy(const char *);

};


#endif //FFMPEGPRACTICE_FFGETMEDIACODECCOPYNEW_H
