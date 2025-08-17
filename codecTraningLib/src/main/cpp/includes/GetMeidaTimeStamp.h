//
// Created by 王耀 on 2025/8/17.
//

#ifndef FFMPEGPRACTICE_GETMEIDATIMESTAMP_H
#define FFMPEGPRACTICE_GETMEIDATIMESTAMP_H

#include "BasicCommon.h"
#include "string"

using namespace std;

class GetMeidaTimeStamp {
private:
    string timeStampInfo;
    AVFormatContext *fmt_ctx = nullptr;

public:
    GetMeidaTimeStamp();

    ~GetMeidaTimeStamp();

    string getMediaTimeStamp(const char * meidaPath);

};


#endif //FFMPEGPRACTICE_GETMEIDATIMESTAMP_H
