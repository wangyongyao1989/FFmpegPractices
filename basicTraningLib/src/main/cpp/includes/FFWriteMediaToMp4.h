//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/8/15.
//

#ifndef FFMPEGPRACTICE_FFWRITEMEDIATOMP4_H
#define FFMPEGPRACTICE_FFWRITEMEDIATOMP4_H

#include "BasicCommon.h"
#include "string"

/**
 * 练习五：写入一个音视频文件的封装实例
 */
class FFWriteMediaToMp4 {

private:
    AVFormatContext *out_fmt_ctx;

public:
    FFWriteMediaToMp4();

    ~ FFWriteMediaToMp4();

    int writeMediaToMp4(const char *);
};


#endif //FFMPEGPRACTICE_FFWRITEMEDIATOMP4_H
