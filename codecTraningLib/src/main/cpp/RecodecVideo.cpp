//
// Created by wangyao on 2025/8/17.
//

#include "includes/RecodecVideo.h"

RecodecVideo::RecodecVideo() {

}

RecodecVideo::~RecodecVideo() {
    if (in_fmt_ctx) {
        in_fmt_ctx = nullptr;
    }
    if (video_encode_ctx) {
        video_encode_ctx = nullptr;
    }
    video_index = -1;
    audio_index = -1;
    if (src_video) {
        src_video = nullptr;
    }
    if (src_audio) {
        src_audio = nullptr;
    }
    if (dest_video) {
        dest_video = nullptr;
    }
    if (out_fmt_ctx) {
        out_fmt_ctx = nullptr;
    }
    if (video_encode_ctx) {
        video_encode_ctx = nullptr;
    }
}

string RecodecVideo::recodecVideo(const char *srcPath, const char *destPath) {
    if (open_input_file(srcPath) < 0) { // 打开输入文件
        return recodecInfo;
    }
    if (open_output_file(srcPath) < 0) { // 打开输出文件
        return recodecInfo;
    }
    int ret = -1;
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 视频包需要重新编码
            packet->stream_index = 0;
            recode_video(packet, frame); // 对视频帧重新编码
        } else { // 音频包暂不重新编码，直接写入目标文件
            packet->stream_index = 1;
            ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
            if (ret < 0) {
                LOGE("write frame occur error %d.\n", ret);
                recodecInfo = recodecInfo + "\n write frame occur error:" + to_string(ret);
                break;
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    packet->data = nullptr; // 传入一个空包，冲走解码缓存
    packet->size = 0;
    recode_video(packet, frame); // 对视频帧重新编码
    output_video(nullptr); // 传入一个空帧，冲走编码缓存
    av_write_trailer(out_fmt_ctx); // 写文件尾
    LOGI("Success recode file.\n");
    recodecInfo = recodecInfo + "\n Success recode file!!!!!!";

    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avcodec_close(video_encode_ctx); // 关闭视频编码器的实例
    avcodec_free_context(&video_encode_ctx); // 释放视频编码器的实例
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
    return recodecInfo;
}

// 打开输入文件
int RecodecVideo::open_input_file(const char *src_name) {
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, src_name, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", src_name);
        recodecInfo = recodecInfo + "\n Can't open file :" + string(src_name);
        return -1;
    }
    LOGI("Success open input_file %s.\n", src_name);
    recodecInfo = recodecInfo + "\n Success open input_file:" + string(src_name);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        recodecInfo = recodecInfo + "\n Can't find stream information. ";
        return -1;
    }
    // 找到视频流的索引
    video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index >= 0) {
        src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec *) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            LOGE("video_codec not found\n");
            recodecInfo = recodecInfo + "\n video_codec not found. ";
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            LOGE("video_decode_ctx is nullptr\n");
            recodecInfo = recodecInfo + "\n video_decode_ctx is nullptr ";
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr); // 打开解码器的实例
        if (ret < 0) {
            LOGE("Can't open video_decode_ctx.\n");
            recodecInfo = recodecInfo + "\n Can't open video_decode_ctx";
            return -1;
        }
    } else {
        LOGE("Can't find video stream.\n");
        recodecInfo = recodecInfo + "\n Can't find video stream.";
        return -1;
    }
    // 找到音频流的索引
    audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index >= 0) {
        src_audio = in_fmt_ctx->streams[audio_index];
    }
    return 0;
}

// 给视频帧编码，并写入压缩后的视频包
int RecodecVideo::output_video(AVFrame *frame) {
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(video_encode_ctx, frame);
    if (ret < 0) {
        LOGE("send frame occur error %d.\n", ret);
        recodecInfo = recodecInfo + "\n send frame occur error" + to_string(ret);
        return ret;
    }
    while (1) {
        AVPacket *packet = av_packet_alloc(); // 分配一个数据包
        // 从编码器实例获取压缩后的数据包
        ret = avcodec_receive_packet(video_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        } else if (ret < 0) {
            LOGE("encode frame occur error %d.\n", ret);
            recodecInfo = recodecInfo + "\n encode frame occur error:" + to_string(ret);
            break;
        }
        // 把数据包的时间戳从一个时间基转换为另一个时间基
        av_packet_rescale_ts(packet, src_video->time_base, dest_video->time_base);
//        LOGI( "pts=%ld, dts=%ld.\n", packet->pts, packet->dts);
        packet->stream_index = 0;
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            LOGE("write frame occur error %d.\n", ret);
            recodecInfo = recodecInfo + "\n write frame occur error:" + to_string(ret);
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    return ret;
}

// 对视频帧重新编码
int RecodecVideo::recode_video(AVPacket *packet, AVFrame *frame) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0) {
        LOGE("send packet occur error %d.\n", ret);
        recodecInfo = recodecInfo + "\n send packet occur error:" + to_string(ret);
        return ret;
    }
    while (1) {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(video_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        } else if (ret < 0) {
            LOGE("decode frame occur error %d.\n", ret);
            recodecInfo = recodecInfo + "\n decode frame occur error :" + to_string(ret);
            break;
        }
        if (frame->pts == AV_NOPTS_VALUE) { // 对H.264裸流做特殊处理
            double interval = 1.0 / av_q2d(src_video->r_frame_rate);
            frame->pts = count * interval / av_q2d(src_video->time_base);
            count++;
        }
        output_video(frame); // 给视频帧编码，并写入压缩后的视频包
    }
    return ret;
}

int RecodecVideo::open_output_file(const char *dest_name) {
    // 分配音视频文件的封装实例
    int ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, dest_name);
    if (ret < 0) {
        LOGE("Can't alloc output_file %s.\n", dest_name);
        recodecInfo = recodecInfo + "\n Can't alloc output_file :" + string(dest_name);
        return -1;
    }
    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, dest_name, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        LOGE("Can't open output_file %s.\n", dest_name);
        recodecInfo = recodecInfo + "\n Can't open output_file:" + string(dest_name);
        return -1;
    }
    LOGI("Success open output_file %s.\n", dest_name);
    recodecInfo = recodecInfo + "\n Success open output_file :" + string(dest_name);
    if (video_index >= 0) { // 创建编码器实例和新的视频流
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频编码器
        AVCodec *video_codec = (AVCodec *) avcodec_find_encoder(video_codec_id);
        if (!video_codec) {
            LOGE("video_codec not found\n");
            recodecInfo = recodecInfo + "\n video_codec not found .";
            return -1;
        }
        video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编码器的实例
        if (!video_encode_ctx) {
            LOGE("video_encode_ctx is null\n");
            recodecInfo = recodecInfo + "\n video_encode_ctx is null";
            return -1;
        }
        // 把源视频流中的编解码参数复制给编码器的实例
        avcodec_parameters_to_context(video_encode_ctx, src_video->codecpar);
        // 注意：帧率和时间基要单独赋值，因为avcodec_parameters_to_context没复制这两个参数
        video_encode_ctx->framerate = src_video->r_frame_rate;
        // framerate.num值过大，会导致视频头一秒变灰色
        if (video_encode_ctx->framerate.num > 60) {
            video_encode_ctx->framerate = (AVRational) {25, 1}; // 帧率
        }
        video_encode_ctx->time_base = src_video->time_base;
        video_encode_ctx->gop_size = 12; // 关键帧的间隔距离
        //video_encode_ctx->max_b_frames = 0; // 0表示不要B帧
        // AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
        if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        ret = avcodec_open2(video_encode_ctx, video_codec, nullptr); // 打开编码器的实例
        if (ret < 0) {
            LOGE("Can't open video_encode_ctx.\n");
            av_strerror(ret, errbuf, sizeof(errbuf)); // 将错误码转换为字符串
            LOGE("avcodec_open2失败：%s\n", errbuf);
            recodecInfo = recodecInfo + "\n avcodec_open2失败：" + string(errbuf);
            return -1;
        }
        dest_video = avformat_new_stream(out_fmt_ctx, nullptr); // 创建数据流
        // 把编码器实例的参数复制给目标视频流
        avcodec_parameters_from_context(dest_video->codecpar, video_encode_ctx);
        // 如果后面有对视频帧转换时间基，这里就无需复制时间基
        //dest_video->time_base = src_video->time_base;
        dest_video->codecpar->codec_tag = 0;
    }
    if (audio_index >= 0) { // 源文件有音频流，就给目标文件创建音频流
        AVStream *dest_audio = avformat_new_stream(out_fmt_ctx, nullptr); // 创建数据流
        // 把源文件的音频参数原样复制过来
        avcodec_parameters_copy(dest_audio->codecpar, src_audio->codecpar);
        dest_audio->codecpar->codec_tag = 0;
    }
    ret = avformat_write_header(out_fmt_ctx, nullptr); // 写文件头
    if (ret < 0) {
        LOGE("write file_header occur error %d.\n", ret);
        recodecInfo = recodecInfo + "\n write file_header occur error :" + to_string(ret);
        av_strerror(ret, errbuf, sizeof(errbuf)); // 将错误码转换为字符串
        recodecInfo = recodecInfo + "\n avformat_write_header 失败：" + string(errbuf);
        return -1;
    }
    LOGI("Success write file_header.\n");
    recodecInfo = recodecInfo + "\n Success write file_header.";
    return 0;
}


