//
// Created by 王耀 on 2025/8/16.
//

#ifndef FFMPEGPRACTICE_FFWRITEMEDIAFILTER_H
#define FFMPEGPRACTICE_FFWRITEMEDIAFILTER_H

#include "BasicCommon.h"
#include "string"

using namespace std;

class FFWriteMediaFilter {

private:
    AVFormatContext *fmt_ctx = nullptr;

    AVFilterContext *buffersrc_ctx = nullptr; // 输入滤镜的实例
    AVFilterContext *buffersink_ctx = nullptr; // 输出滤镜的实例
    AVFilterGraph *filter_graph = nullptr; // 滤镜图

    string init_filter(AVStream *video_stream, AVCodecContext *video_decode_ctx, const char *filters_desc);

public:

    FFWriteMediaFilter();

    ~FFWriteMediaFilter();

    string writeMediaFilter(const char *);

};


#endif //FFMPEGPRACTICE_FFWRITEMEDIAFILTER_H
