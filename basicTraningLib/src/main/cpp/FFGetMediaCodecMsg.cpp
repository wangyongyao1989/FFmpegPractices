//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/8/15.
//

#include "FFGetMediaCodecMsg.h"

FFGetMediaCodecMsg::FFGetMediaCodecMsg() {

}

FFGetMediaCodecMsg::~FFGetMediaCodecMsg() {
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx); // 关闭音视频文件
    }
}

string FFGetMediaCodecMsg::getMediaCodecMsg(const char *cFragPath) {
    LOGI("getMediaCodecMsg =%c\n");
    fmt_ctx = avformat_alloc_context();
    // 打开音视频文件
    int ret = avformat_open_input(&fmt_ctx, cFragPath, nullptr, nullptr);

    if (ret < 0) {
        LOGE("Can't open file %s.\n", cFragPath);
        meidaCodecMsg = "Can't open file";
        return meidaCodecMsg;
    }
    LOGI("Success open input_file %s.\n", cFragPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        meidaCodecMsg = "Can't find stream information.";
        return meidaCodecMsg;
    }
    LOGI("Success find stream information.\n");
    // 格式化输出文件信息
    av_dump_format(fmt_ctx, 0, cFragPath, 0);
    // 找到视频流的索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    LOGI("video_index=%d\n", video_index);
    if (video_index >= 0) {
        AVStream *video_stream = fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        AVCodec *video_codec = (AVCodec *) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            LOGE("vide_codec not find.\n");
            meidaCodecMsg = "vide_codec not find";
            return meidaCodecMsg;
        }
        meidaCodecMsg = meidaCodecMsg + "视频解码信息 : " + "\n video codec id= " +
                        to_string(video_codec->id)
                        + "\n video codec name= " + video_codec->name
                        + "\n video codec type = " + to_string(video_codec->type);
    }

    // 找到音频流的索引
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    LOGI("audio_index=%d\n", audio_index);

    if (audio_index >= 0) {
        AVStream *audio_stream = fmt_ctx->streams[audio_index];
        //查找音频解码器
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;

        const AVCodec *audio_codec = (AVCodec *) avcodec_find_decoder(audio_codec_id);
        if (!audio_codec) {
            LOGE("audio_codec not find.\n");
            meidaCodecMsg = "audio_codec not find";
            return meidaCodecMsg.c_str();
        }
//        LOGI("audio_codec long_name =%c\n", to_string(audio_codec->type));

        meidaCodecMsg = meidaCodecMsg + "\n 音频解码器" + "\n audio_codec id = " +
                        to_string(audio_codec->id)
                        + "\n audio_codec name = " + audio_codec->name
                        + "\n audio_codec type = " + to_string(audio_codec->type);
    }

    avformat_close_input(&fmt_ctx); // 关闭音视频文件

    return meidaCodecMsg;
}
