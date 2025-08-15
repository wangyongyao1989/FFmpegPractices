//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/8/15.
//

#include "includes/FFGetMediaMsg.h"

FFGetMediaMsg::FFGetMediaMsg() {

}

FFGetMediaMsg::~FFGetMediaMsg() {
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx); // 关闭音视频文件
    }
}

string FFGetMediaMsg::getMediaMsg(const char *cFragPath) {
    // 打开音视频文件
    fmt_ctx = avformat_alloc_context();

    int ret = avformat_open_input(&fmt_ctx, cFragPath, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", cFragPath);
        return nullptr;
    }
    LOGI("Success open input_file %s.\n", cFragPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        return nullptr;
    }
    LOGI("Success find stream information.\n");
    // 格式化输出文件信息
    av_dump_format(fmt_ctx, 0, cFragPath, 0);
    string infoMsg;
    infoMsg = infoMsg + "视频信息 : "
              + "\n duration = " + to_string(fmt_ctx->duration) // 持续时间，单位微秒
              + "\n bit_rate = " + to_string(fmt_ctx->bit_rate) // 比特率，单位比特每秒
              + "\n nb_streams = " + to_string(fmt_ctx->nb_streams) // 数据流的数量
              + "\n max_streams = " + to_string(fmt_ctx->max_streams);  // 数据流的最大数量
    // 找到视频流的索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    LOGI("video_index=%d\n", video_index);
    if (video_index >= 0) {
        AVStream *video_stream = fmt_ctx->streams[video_index];
        infoMsg = infoMsg + "\n video_stream index = " + to_string(video_stream->start_time)
                  + "\n video_stream start_time= " + to_string(video_stream->start_time)
                  + "\n video_stream duration = " + to_string(video_stream->duration);
    }
    // 找到音频流的索引
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    LOGI("audio_index=%d\n", audio_index);
    if (audio_index >= 0) {
        AVStream *audio_stream = fmt_ctx->streams[audio_index];
        infoMsg = infoMsg + "\n audio_stream index = " + to_string(audio_stream->index)
                  + "\n audio_stream start_time = " + to_string(audio_stream->start_time)
                  + "\n audio_stream nb_frames = " + to_string(audio_stream->duration);
    }

    avformat_close_input(&fmt_ctx); // 关闭音视频文件

    return infoMsg;
}
