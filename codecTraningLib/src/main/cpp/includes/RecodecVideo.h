//
// Created by wangyao on 2025/8/17.
//

#ifndef FFMPEGPRACTICE_RECODECVIDEO_H
#define FFMPEGPRACTICE_RECODECVIDEO_H

#include "BasicCommon.h"
#include "string"

using namespace std;

class RecodecVideo {

private:
    string recodecInfo;

    AVFormatContext *in_fmt_ctx = nullptr; // 输入文件的封装器实例
    AVCodecContext *video_decode_ctx = nullptr; // 视频解码器的实例
    int video_index = -1; // 视频流的索引
    int audio_index = -1; // 音频流的索引
    AVStream *src_video = nullptr; // 源文件的视频流
    AVStream *src_audio = nullptr; // 源文件的音频流
    AVStream *dest_video = nullptr; // 目标文件的视频流
    AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
    AVCodecContext *video_encode_ctx = nullptr; // 视频编码器的实例

    char errbuf[1024];

    int count = 0;

    int open_input_file(const char *src_name);

    int open_output_file(const char *dest_name);

    int output_video(AVFrame *frame);

    int recode_video(AVPacket *packet, AVFrame *frame);

public:
    RecodecVideo();

    ~RecodecVideo();

    string recodecVideo(const char *srcPath, const char *destPath);


};


#endif //FFMPEGPRACTICE_RECODECVIDEO_H
