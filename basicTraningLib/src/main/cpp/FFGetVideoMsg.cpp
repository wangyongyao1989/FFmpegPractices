//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/8/15.
//

#include "FFGetVideoMsg.h"

string FFGetVideoMsg::getVideoMsg(const char *videoPath) {

    LOGI("videoPath=== %s", videoPath);
    fmt_ctx = avformat_alloc_context();
    // 打开音视频文件
    int ret = avformat_open_input(&fmt_ctx, videoPath, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", videoPath);
        videoMsg = "Can't open file %s.";
        return videoMsg;
    }
    LOGI("Success open input_file %s.\n", videoPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        videoMsg = "Can't find stream information.";
        return videoMsg;
    }
    LOGI("Success find stream information.\n");
    const AVInputFormat *iformat = fmt_ctx->iformat;
    int64_t duration = fmt_ctx->duration;
    LOGI("format name is %s.\n", iformat->name);
    LOGI("duration is %d.\n", duration);

    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "打开视频获取信息 : ");
    strcat(strBuffer, "\n format name is : ");
    strcat(strBuffer, iformat->name);
    videoMsg = strBuffer;
    // 关闭音视频文件
    avformat_close_input(&fmt_ctx);

    return videoMsg;
}

FFGetVideoMsg::FFGetVideoMsg() {

}

FFGetVideoMsg::~FFGetVideoMsg() {
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }
}
