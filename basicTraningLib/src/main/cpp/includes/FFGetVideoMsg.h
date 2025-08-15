//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/8/15.
//

#ifndef FFMPEGPRACTICE_FFGETVIDEOMSG_H
#define FFMPEGPRACTICE_FFGETVIDEOMSG_H

#include "string"
#include "BasicCommon.h"

using namespace std;

/**
 * 练习一：打开关闭一个音视频流，并获取其内部的信息
 */
class FFGetVideoMsg {

private:
    AVFormatContext *fmt_ctx;
    string videoMsg;

public:

    FFGetVideoMsg();

    ~FFGetVideoMsg();

    string getVideoMsg(const char *);

};


#endif //FFMPEGPRACTICE_FFGETVIDEOMSG_H
