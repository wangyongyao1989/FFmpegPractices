//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/8/15.
//

#include "FFWriteMediaToMp4.h"

FFWriteMediaToMp4::FFWriteMediaToMp4() {

}

FFWriteMediaToMp4::~FFWriteMediaToMp4() {

}

int FFWriteMediaToMp4::writeMediaToMp4(const char *cOutPath) {
    // 1.分配音视频文件的封装实例
    int ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, cOutPath);
    if (ret < 0) {
        LOGE("Can't alloc output_file %s.\n", cOutPath);
        return -1;
    }
    // 2.打开输出流
    ret = avio_open(&out_fmt_ctx->pb, cOutPath, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        LOGE("Can't open output_file %s.\n", cOutPath);
        return -1;
    }
    LOGI("Success open output_file %s.\n", cOutPath);


    // 3.查找编码器
    AVCodec *video_codec = (AVCodec *) avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!video_codec) {
        LOGE("AV_CODEC_ID_H264 not found\n");
        return -1;
    }
    AVCodecContext *video_encode_ctx = nullptr;
    video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编解码器的实例
    if (!video_encode_ctx) {
        LOGE("video_encode_ctx is null\n");
        return -1;
    }
    video_encode_ctx->width = 320; // 视频画面的宽度
    video_encode_ctx->height = 240; // 视频画面的高度
    // 4.创建指定编码器的数据流
    AVStream *video_stream = avformat_new_stream(out_fmt_ctx, video_codec);
    // 5.把编码器实例中的参数复制给数据流
    avcodec_parameters_from_context(video_stream->codecpar, video_encode_ctx);
    video_stream->codecpar->codec_tag = 0; // 非特殊情况都填0

    ret = avformat_write_header(out_fmt_ctx, nullptr); // 写文件头
    if (ret < 0) {
        LOGE("write file_header occur error %d.\n", ret);
        return -1;
    }
    LOGI("Success write file_header.\n");
    av_write_trailer(out_fmt_ctx); // 写文件尾
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例

    return 0;
}
