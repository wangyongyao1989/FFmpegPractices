//
// Created by wangyao on 2025/8/16.
//

#ifndef FFMPEGPRACTICE_GETMEDIAMSG_H
#define FFMPEGPRACTICE_GETMEDIAMSG_H

#include "BasicCommon.h"
#include "string"

using namespace std;

class GetMediaMsg {

private:
    string mediaInfo = "";
    AVFormatContext *fmt_ctx = nullptr;

public:
    GetMediaMsg();

    ~GetMediaMsg();

    string getMediaMsg(const char *mediaPath);
};


#endif //FFMPEGPRACTICE_GETMEDIAMSG_H
