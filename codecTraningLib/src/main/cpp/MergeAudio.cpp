//
// Created by wangyao on 2025/8/17.
//

#include "includes/MergeAudio.h"

string
MergeAudio::mergeAudio(const char *srcVideoPath, const char *srcAudioPath, const char *destPath) {
// 打开视频文件
    int ret = avformat_open_input(&video_fmt_ctx, srcVideoPath, NULL, NULL);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", srcVideoPath);
        mergeAudioInfo = mergeAudioInfo + "\n Can't open file:" + string(srcVideoPath);
        return mergeAudioInfo;
    }
    LOGI("Success open input_file %s.\n", srcVideoPath);
    mergeAudioInfo = mergeAudioInfo + "\n Success open input_file " + string(srcVideoPath);
    // 查找视频文件中的流信息
    ret = avformat_find_stream_info(video_fmt_ctx, NULL);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        mergeAudioInfo = mergeAudioInfo + "\n Can't find stream information";
        return mergeAudioInfo;
    }
    AVStream *src_video = NULL;
    // 找到视频流的索引
    int video_index = av_find_best_stream(video_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index >= 0) {
        src_video = video_fmt_ctx->streams[video_index];
    } else {
        LOGE("Can't find video stream.\n");
        mergeAudioInfo = mergeAudioInfo + "\n Can't find video stream.";
        return mergeAudioInfo;
    }
    AVFormatContext *audio_fmt_ctx = NULL; // 输入文件的封装器实例
    // 打开音频文件
    ret = avformat_open_input(&audio_fmt_ctx, srcAudioPath, NULL, NULL);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", srcAudioPath);
        mergeAudioInfo = mergeAudioInfo + "\n Can't open file:" + string(srcAudioPath);
        return mergeAudioInfo;
    }
    LOGI("Success open input_file %s.\n", srcAudioPath);
    mergeAudioInfo = mergeAudioInfo + "\n Success open input_file:" + string(srcAudioPath);

    // 查找音频文件中的流信息
    ret = avformat_find_stream_info(audio_fmt_ctx, NULL);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        mergeAudioInfo = mergeAudioInfo + "\n Can't find stream information.";
        return mergeAudioInfo;
    }
    AVStream *src_audio = NULL;
    // 找到音频流的索引
    int audio_index = av_find_best_stream(audio_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index >= 0) {
        src_audio = audio_fmt_ctx->streams[audio_index];
    } else {
        LOGE("Can't find audio stream.\n");
        mergeAudioInfo = mergeAudioInfo + "\n Can't find audio stream.";
        return mergeAudioInfo;
    }

    AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
    // 分配音视频文件的封装实例
    ret = avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, destPath);
    if (ret < 0) {
        LOGE("Can't alloc output_file %s.\n", destPath);
        mergeAudioInfo = mergeAudioInfo + "\n Can't alloc output_file:" + string(destPath);
        return mergeAudioInfo;
    }
    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, destPath, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        LOGE("Can't open output_file %s.\n", destPath);
        mergeAudioInfo = mergeAudioInfo + "\n Can't open output_file:" + string(destPath);
        return mergeAudioInfo;
    }
    LOGI("Success open output_file %s.\n", destPath);
    mergeAudioInfo = mergeAudioInfo + "\n Success open output_file:" + string(destPath);
    AVStream *dest_video = nullptr;
    if (video_index >= 0) { // 源文件有视频流，就给目标文件创建视频流
        dest_video = avformat_new_stream(out_fmt_ctx, nullptr); // 创建数据流
        // 把源文件的视频参数原样复制过来
        avcodec_parameters_copy(dest_video->codecpar, src_video->codecpar);
        // 如果后面有对视频帧转换时间基，这里就无需复制时间基
        //dest_video->time_base = src_video->time_base;
        dest_video->codecpar->codec_tag = 0;
    }
    AVStream *dest_audio = NULL;
    if (audio_index >= 0) { // 源文件有音频流，就给目标文件创建音频流
        dest_audio = avformat_new_stream(out_fmt_ctx, NULL); // 创建数据流
        // 把源文件的音频参数原样复制过来
        avcodec_parameters_copy(dest_audio->codecpar, src_audio->codecpar);
        dest_audio->codecpar->codec_tag = 0;
    }
    ret = avformat_write_header(out_fmt_ctx, NULL); // 写文件头
    if (ret < 0) {
        LOGE("write file_header occur error %d.\n", ret);
        mergeAudioInfo = mergeAudioInfo + "\n write file_header occur error:" + to_string(ret);
        return mergeAudioInfo;
    }
    LOGI("Success write file_header.\n");
    mergeAudioInfo = mergeAudioInfo + "\n Success write file_header.";

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    int64_t last_video_pts = 0; // 上次的视频时间戳
    int64_t last_audio_pts = 0; // 上次的音频时间戳
    while (1) {
        // av_compare_ts函数用于比较两个时间戳的大小（它们来自不同的时间基）
        if (last_video_pts == 0 || av_compare_ts(last_video_pts, dest_video->time_base,
                                                 last_audio_pts, dest_audio->time_base) <= 0) {
            while ((ret = av_read_frame(video_fmt_ctx, packet)) >= 0) { // 轮询视频包
                if (packet->stream_index == video_index) { // 找到一个视频包
                    break;
                }
            }
            if (ret == 0) {
                // av_packet_rescale_ts会把数据包的时间戳从一个时间基转换为另一个时间基
                av_packet_rescale_ts(packet, src_video->time_base, dest_video->time_base);
                packet->stream_index = 0; // 视频流索引
                last_video_pts = packet->pts; // 保存最后一次视频时间戳
            } else {
                LOGI("End video file.\n");
                mergeAudioInfo = mergeAudioInfo + "\n End video file.";
                break;
            }
        } else {
            while ((ret = av_read_frame(audio_fmt_ctx, packet)) >= 0) { // 轮询音频包
                if (packet->stream_index == audio_index) { // 找到一个音频包
                    break;
                }
            }
            if (ret == 0) {
                // av_packet_rescale_ts会把数据包的时间戳从一个时间基转换为另一个时间基
                av_packet_rescale_ts(packet, src_audio->time_base, dest_audio->time_base);
                packet->stream_index = 1; // 音频流索引
                last_audio_pts = packet->pts; // 保存最后一次音频时间戳
            } else {
                LOGI("End audio file.\n");
                mergeAudioInfo = mergeAudioInfo + "\n End audio file.";
                break;
            }
        }
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            LOGE("write frame occur error %d.\n", ret);
            mergeAudioInfo = mergeAudioInfo + "\n write frame occur error:" + to_string(ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_write_trailer(out_fmt_ctx); // 写文件尾
    LOGI("Success merge video and audio file.\n");
    mergeAudioInfo = mergeAudioInfo + "\n Success merge video and audio file.";

    av_packet_free(&packet); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&video_fmt_ctx); // 关闭视频文件
    avformat_close_input(&audio_fmt_ctx); // 关闭音频文件
    return mergeAudioInfo;
}

MergeAudio::~MergeAudio() {

}

MergeAudio::MergeAudio() {

}
