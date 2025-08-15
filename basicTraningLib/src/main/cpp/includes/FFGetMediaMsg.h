//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/8/15.
//

#ifndef FFMPEGPRACTICE_FFGETMEDIAMSG_H
#define FFMPEGPRACTICE_FFGETMEDIAMSG_H

#include "string"
#include "BasicCommon.h"

using namespace std;

/**
 * 练习二：打开一个音视频文件后，打印出视频及音频相关信息
 */
class FFGetMediaMsg {

private:
    AVFormatContext *fmt_ctx = nullptr;
    string mediaMsg;


public:
    FFGetMediaMsg();

    ~FFGetMediaMsg();

    string getMediaMsg(const char *);

};


#endif //FFMPEGPRACTICE_FFGETMEDIAMSG_H
