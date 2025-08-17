//
// Created by 王耀 on 2025/8/17.
//

#include "includes/GetMediaTimeBase.h"

GetMediaTimeBase::GetMediaTimeBase() {

}

GetMediaTimeBase::~GetMediaTimeBase() {

}

string GetMediaTimeBase::getMediaTimeBase(const char *mediaPath) {

    fmt_ctx = avformat_alloc_context();
    // 打开音视频文件
    int ret = avformat_open_input(&fmt_ctx, mediaPath, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", mediaPath);
        timeBaseInfo = "Can't open file ";
        return timeBaseInfo;
    }
    LOGI("Success open input_file %s.\n", mediaPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        timeBaseInfo = "Can't find stream information. ";
        return timeBaseInfo;
    }
    LOGI("duration=%d\n", fmt_ctx->duration); // 持续时间，单位微秒
    LOGI("nb_streams=%d\n", fmt_ctx->nb_streams); // 数据流的数量
    LOGI("max_streams=%d\n", fmt_ctx->max_streams); // 数据流的最大数量
    LOGI("video_codec_id=%d\n", fmt_ctx->video_codec_id);
    LOGI("audio_codec_id=%d\n", fmt_ctx->audio_codec_id);
    // 找到视频流的索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    LOGI("video_index=%d\n", video_index);
    if (video_index >= 0) {
        AVStream *video_stream = fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec *) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            LOGE("video_codec not found\n");
            timeBaseInfo = "video_codec not found";
            return timeBaseInfo;
        }
        LOGI("video_codec name=%s\n", video_codec->name);
        AVCodecParameters *video_codecpar = video_stream->codecpar;
        // 计算帧率，每秒有几个视频帧
        int fps = video_stream->r_frame_rate.num / video_stream->r_frame_rate.den;
        //int fps = av_q2d(video_stream->r_frame_rate);
        LOGI("video_codecpar bit_rate=%d\n", video_codecpar->bit_rate);
        LOGI("video_codecpar width=%d\n", video_codecpar->width);
        LOGI("video_codecpar height=%d\n", video_codecpar->height);
        LOGI("video_stream fps=%d\n", fps);
        int per_video = 1000 / fps; // 计算每个视频帧的持续时间
        LOGI("one video frame's duration is %dms\n", per_video);
        // 获取视频流的时间基准
        AVRational time_base = video_stream->time_base;
        LOGI("video_stream time_base.num=%d\n", time_base.num);
        LOGI("video_stream time_base.den=%d\n", time_base.den);

        timeBaseInfo = timeBaseInfo + "视频的比特率=" + to_string(video_codecpar->bit_rate)
                       + "\n 视频画面的宽度=" + to_string(video_codecpar->width)
                       + "\n 视频画面的高度=" + to_string(video_codecpar->height)
                       + "\n 视频的帧率=" + to_string(fps)
                       + "\n 视频时间基准 num =" + to_string(time_base.num)
                       + "\n 视频时间基准 den =" + to_string(time_base.den);
    }
    // 找到音频流的索引
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    LOGI("audio_index=%d\n", audio_index);
    if (audio_index >= 0) {
        AVStream *audio_stream = fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;
        // 查找音频解码器
        AVCodec *audio_codec = (AVCodec *) avcodec_find_decoder(audio_codec_id);
        if (!audio_codec) {
            LOGE("audio_codec not found\n");
            timeBaseInfo = "audio_codec not found";
            return timeBaseInfo;
        }
        LOGI("audio_codec name=%s\n", audio_codec->name);
        AVCodecParameters *audio_codecpar = audio_stream->codecpar;
        LOGI("audio_codecpar bit_rate=%d\n", audio_codecpar->bit_rate);
        LOGI("audio_codecpar frame_size=%d\n", audio_codecpar->frame_size);
        LOGI("audio_codecpar sample_rate=%d\n", audio_codecpar->sample_rate);
        LOGI("audio_codecpar nb_channels=%d\n", audio_codecpar->ch_layout.nb_channels);
        // 计算音频帧的持续时间。frame_size为每个音频帧的采样数量，sample_rate为音频帧的采样频率
        int per_audio = 1000 * audio_codecpar->frame_size / audio_codecpar->sample_rate;
        LOGI("one audio frame's duration is %dms\n", per_audio);
        // 获取音频流的时间基准
        AVRational time_base = audio_stream->time_base;
        LOGI("audio_stream time_base.num=%d\n", time_base.num);
        LOGI("audio_stream time_base.den=%d\n", time_base.den);

        timeBaseInfo = timeBaseInfo + "音频的比特率=" + to_string(audio_codecpar->bit_rate)
                       + "\n 音频帧的大小=" + to_string(audio_codecpar->frame_size)
                       + "\n 音频的采样率=" + to_string(audio_codecpar->sample_rate)
                       + "\n 音频的声道信息=" + to_string(audio_codecpar->ch_layout.nb_channels)
                       + "\n 音频时间基准num =" + to_string(time_base.num)
                       + "\n 音频时间基准 den =" + to_string(time_base.den);
    }
    avformat_close_input(&fmt_ctx); // 关闭音视频文件

    return timeBaseInfo;
}
