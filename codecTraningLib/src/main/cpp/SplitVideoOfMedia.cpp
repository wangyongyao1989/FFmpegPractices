//
// Created by 王耀 on 2025/8/17.
//

#include "includes/SplitVideoOfMedia.h"

SplitVideoOfMedia::SplitVideoOfMedia() {

}

SplitVideoOfMedia::~SplitVideoOfMedia() {

}

string SplitVideoOfMedia::splitVideoOfMedia(const char *srcPath, const char *destPath) {

    in_fmt_ctx = avformat_alloc_context();

    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, srcPath, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", srcPath);
        splitVideoInfo = "Can't open file:" + string(srcPath);
        return splitVideoInfo;
    }
    LOGI("Success open input_file %s.\n", srcPath);
    splitVideoInfo = splitVideoInfo + "\n Success open input_file :" + string(srcPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        splitVideoInfo = splitVideoInfo + "\n Can't find stream information.";
        return splitVideoInfo;
    }
    AVStream *src_video = nullptr;
    // 找到视频流的索引
    int video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index >= 0) {
        src_video = in_fmt_ctx->streams[video_index];
    } else {
        LOGE("Can't find video stream.\n");
        splitVideoInfo = splitVideoInfo + "\n Can't find video stream.";
        return splitVideoInfo;
    }

    AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
    // 分配音视频文件的封装实例
    ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, destPath);
    if (ret < 0) {
        LOGE("Can't alloc output_file %s.\n", destPath);
        splitVideoInfo = splitVideoInfo + "\n Can't alloc output_file :" + string(destPath);
        return splitVideoInfo;
    }
    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, destPath, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        LOGE("Can't open output_file %s.\n", destPath);
        splitVideoInfo = splitVideoInfo + "\n Can't open output_file :" + string(destPath);
        return splitVideoInfo;
    }
    LOGI("Success open output_file %s.\n", destPath);
    splitVideoInfo = splitVideoInfo + "\n Can't open output_file :" + string(destPath);
    AVStream *dest_video = NULL;
    if (video_index >= 0) { // 源文件有视频流，就给目标文件创建视频流
        dest_video = avformat_new_stream(out_fmt_ctx, NULL); // 创建数据流
        // 把源文件的视频参数原样复制过来
        avcodec_parameters_copy(dest_video->codecpar, src_video->codecpar);
        dest_video->time_base = src_video->time_base;
        dest_video->codecpar->codec_tag = 0;
    }
    ret = avformat_write_header(out_fmt_ctx, NULL); // 写文件头
    if (ret < 0) {
        LOGE("write file_header occur error %d.\n", ret);
        splitVideoInfo = splitVideoInfo + "\n write file_header occur error:" + to_string(ret);
        return splitVideoInfo;
    }
    LOGI("Success write file_header.\n");

    double begin_time = 5.0; // 切割开始时间，单位秒
    double end_time = 15.0; // 切割结束时间，单位秒
    // 计算开始切割位置的播放时间戳
    int64_t begin_video_pts = begin_time / av_q2d(src_video->time_base);
    // 计算结束切割位置的播放时间戳
    int64_t end_video_pts = end_time / av_q2d(src_video->time_base);
    LOGI("begin_video_pts=%d, end_video_pts=%d\n", begin_video_pts,
         end_video_pts);
    splitVideoInfo = splitVideoInfo + "\n begin_video_pts=" + to_string(begin_video_pts)
                     + "\n end_video_pts=" + to_string(end_video_pts);

    // 寻找指定时间戳的关键帧，并非begin_video_pts所处的精确位置，而是离begin_video_pts最近的关键帧
    ret = av_seek_frame(in_fmt_ctx, video_index, begin_video_pts,
                        AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        LOGE("seek video frame occur error %d.\n", ret);
        splitVideoInfo = splitVideoInfo + "\n seek video frame occur error:" + to_string(ret);
        return splitVideoInfo;
    }

    int64_t key_frame_pts = -1; // 关键帧的播放时间戳
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 为视频流
            packet->stream_index = 0; // 视频流默认在第一路
            if (key_frame_pts == -1) { // 保存最靠近begin_video_pts的关键帧时间戳
                key_frame_pts = packet->pts;
            }
            if (packet->pts > key_frame_pts + end_video_pts - begin_video_pts) {
                break; // 比切割的结束时间大，就结束切割
            }
//            LOGI( "packet->pts=%d, packet->dts=%d\n", packet->pts, packet->dts);
            packet->pts = packet->pts - key_frame_pts; // 调整视频包的播放时间戳
            packet->dts = packet->dts - key_frame_pts; // 调整视频包的解码时间戳
            ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
            if (ret < 0) {
                LOGE("write frame occur error %d.\n", ret);
                splitVideoInfo = splitVideoInfo + "\n write frame occur error:" + to_string(ret);
                break;
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    av_write_trailer(out_fmt_ctx); // 写文件尾
    LOGI("Success split video.\n");
    splitVideoInfo = splitVideoInfo + "\n Success split video.";

    av_packet_free(&packet); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件


    return splitVideoInfo;
}
