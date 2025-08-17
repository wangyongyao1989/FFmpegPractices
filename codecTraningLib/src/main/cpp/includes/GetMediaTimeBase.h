//
// Created by 王耀 on 2025/8/17.
//

#ifndef FFMPEGPRACTICE_GETMEDIATIMEBASE_H
#define FFMPEGPRACTICE_GETMEDIATIMEBASE_H
#include "BasicCommon.h"
#include "string"


using namespace std;
class GetMediaTimeBase {

private:
    string timeBaseInfo;
    AVFormatContext *fmt_ctx = nullptr;

public:
    GetMediaTimeBase();

    ~GetMediaTimeBase();

    string getMediaTimeBase(const char * mediaPath);

};


#endif //FFMPEGPRACTICE_GETMEDIATIMEBASE_H
