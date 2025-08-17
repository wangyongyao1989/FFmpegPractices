//
// Created by 王耀 on 2025/8/17.
//

#include "includes/PeelAudioOfMedia.h"

PeelAudioOfMedia::PeelAudioOfMedia() {

}

PeelAudioOfMedia::~PeelAudioOfMedia() {

}

string PeelAudioOfMedia::peelAudioOfMedia(const char *srcPath, const char *destPath) {

    in_fmt_ctx = avformat_alloc_context();

    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, srcPath, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", srcPath);
        peelAudioInfo = "Can't open file :" + string(srcPath);
        return peelAudioInfo;
    }
    LOGI("Success open input_file %s.\n", srcPath);
    peelAudioInfo = peelAudioInfo + "\n Success open input_file :" + string(srcPath);

    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        peelAudioInfo = peelAudioInfo + "\n Can't find stream information.";
        return peelAudioInfo;
    }
    AVStream *src_video = nullptr;
    // 找到视频流的索引
    int video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index >= 0) {
        src_video = in_fmt_ctx->streams[video_index];
    } else {
        LOGE("Can't find video stream.\n");
        peelAudioInfo = peelAudioInfo + "\n Can't find video stream.";
        return peelAudioInfo;
    }

    AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
    // 分配音视频文件的封装实例
    ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, destPath);
    if (ret < 0) {
        LOGE("Can't alloc output_file %s.\n", destPath);
        peelAudioInfo = peelAudioInfo + "\n Can't alloc output_file:" + string(destPath);
        return peelAudioInfo;
    }
    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, destPath, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        LOGE("Can't open output_file %s.\n", destPath);
        peelAudioInfo = peelAudioInfo + "\n Can't open output_file :" + string(destPath);
        return peelAudioInfo;
    }
    LOGI("Success open output_file %s.\n", destPath);
    peelAudioInfo = peelAudioInfo + "\n Success open output_file :" + string(destPath);
    if (video_index >= 0) { // 源文件有视频流，就给目标文件创建视频流
        AVStream *dest_video = avformat_new_stream(out_fmt_ctx, nullptr); // 创建数据流
        // 把源文件的视频参数原样复制过来
        avcodec_parameters_copy(dest_video->codecpar, src_video->codecpar);
        dest_video->time_base = src_video->time_base;
        dest_video->codecpar->codec_tag = 0;
    }
    ret = avformat_write_header(out_fmt_ctx, nullptr); // 写文件头
    if (ret < 0) {
        LOGE("write file_header occur error %d.\n", ret);
        peelAudioInfo = peelAudioInfo + "\n write file_header occur error :" + to_string(ret);
        return peelAudioInfo;
    }
    LOGI("Success write file_header.\n");
    peelAudioInfo = peelAudioInfo + "\n Success write file_header.";

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 为视频流
            packet->stream_index = 0; // 视频流默认在第一路
            ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
            if (ret < 0) {
                LOGE("write frame occur error %d.\n", ret);
                peelAudioInfo = peelAudioInfo + "\n write frame occur error :" + to_string(ret);
                break;
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_write_trailer(out_fmt_ctx); // 写文件尾
    LOGI("Success peel audio.\n");
    peelAudioInfo = peelAudioInfo + "\n Success peel audio.";
    av_packet_free(&packet); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件

    return peelAudioInfo;
}
