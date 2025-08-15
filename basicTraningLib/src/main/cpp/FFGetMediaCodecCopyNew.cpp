//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/8/15.
//

#include "FFGetMediaCodecCopyNew.h"

string FFGetMediaCodecCopyNew::getMediaCodecCopy(const char *cFragPath) {

    fmt_ctx = avformat_alloc_context();
    // 打开音视频文件
    int ret = avformat_open_input(&fmt_ctx, cFragPath, nullptr, nullptr);

    if (ret < 0) {
        LOGE("Can't open file %s.\n", cFragPath);
        infoMsg = "Can't open file";
        return infoMsg.c_str();
    }
    LOGI("Success open input_file %s.\n", cFragPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        infoMsg = "Can't find stream information.";
        return infoMsg.c_str();
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
            infoMsg = "vide_codec not find";
            return infoMsg.c_str();
        }
        AVCodecContext *video_decode_ctx = nullptr; // 视频解码器的实例
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            LOGE("video_decode_ctx is null.\n");
            infoMsg = "video_decode_ctx is null";
            return infoMsg.c_str();
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, video_stream->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr); // 打开解码器的实例
        LOGI("Success open video codec.\n");
        if (ret < 0) {
            LOGE("Can't open video_decode_ctx.\n");
            infoMsg = "Can't open video_decode_ctx.";
            return infoMsg.c_str();
        }

        infoMsg = infoMsg + "视频解码器实例参数 : " + "\n Success copy video parameters_to_context "
                  + "\n video_decode_ctx width= " + to_string(video_decode_ctx->width)
                  + "\n video_decode_ctx height = " + to_string(video_decode_ctx->height)
                  + "\n 双向预测帧（B帧）的最大数量 = " + to_string(video_decode_ctx->max_b_frames)
                  + "\n 视频的像素格式 = " + to_string(video_decode_ctx->pix_fmt)
                  + "\n 每两个关键帧（I帧）间隔多少帧 = " + to_string(video_decode_ctx->gop_size)
                  + "\n video_decode_ctx profile = " + to_string(video_decode_ctx->profile);

        avcodec_close(video_decode_ctx); // 关闭解码器的实例
        avcodec_free_context(&video_decode_ctx); // 释放解码器的实例
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
            infoMsg = "audio_codec not find";
            return infoMsg.c_str();
        }
        AVCodecContext *audio_decode_ctx = nullptr; // 音频解码器的实例
        audio_decode_ctx = avcodec_alloc_context3(audio_codec); // 分配解码器的实例
        if (!audio_decode_ctx) {
            LOGE("audio_decode_ctx is null\n");
            infoMsg = "audio_decode_ctx is null.";
            return infoMsg.c_str();
        }
        // 把音频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(audio_decode_ctx, audio_stream->codecpar);
        ret = avcodec_open2(audio_decode_ctx, audio_codec, nullptr); // 打开解码器的实例
        LOGI("Success open audio codec.\n");
        if (ret < 0) {
            LOGE("audio_decode_ctx is null.\n");
            infoMsg = "audio_decode_ctx is null.";
            return infoMsg.c_str();
        }

        infoMsg =
                infoMsg + "\n 音频解码器实例参数:" + "\n Success copy audio parameters_to_context. "
                + "\n audio_decode_ctx profile = " + to_string(audio_decode_ctx->profile)
                + "\n 音频的采样格式 = " + to_string(audio_decode_ctx->sample_fmt)
                + "\n 音频的采样频率 = " + to_string(audio_decode_ctx->sample_rate)
                + "\n 音频的帧大小 = " + to_string(audio_decode_ctx->frame_size)
                + "\n 音频的码率 = " + to_string(audio_decode_ctx->bit_rate)
                + "\n 音视频的时间基num = " + to_string(audio_decode_ctx->time_base.num)
                + "\n 音视频的时间基den = " + to_string(audio_decode_ctx->time_base.den)
                + "\n 音频的声道布局 = " + to_string(audio_decode_ctx->ch_layout.nb_channels);

        avcodec_close(audio_decode_ctx); // 关闭解码器的实例
        avcodec_free_context(&audio_decode_ctx); // 释放解码器的实例
    }

    avformat_close_input(&fmt_ctx); // 关闭音视频文件

    return infoMsg;
}
