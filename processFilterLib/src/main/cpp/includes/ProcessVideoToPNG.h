//
// Created by WANGYAO on 2025/9/8.
//

#ifndef FFMPEGPRACTICE_PROCESSVIDEOTOPNG_H
#define FFMPEGPRACTICE_PROCESSVIDEOTOPNG_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;

class ProcessVideoToPNG {

private:
    string videoFilterInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    string sSrcPath;
    string sOutPath;
    string sFilterCmd;

    char errbuf[1024];

    const char *total_frames; // 总帧数
    const char *filters_desc = "";

    AVFormatContext *in_fmt_ctx = nullptr; // 输入文件的封装器实例
    AVCodecContext *video_decode_ctx = nullptr; // 视频解码器的实例
    int video_index = -1; // 视频流的索引
    AVStream *src_video = nullptr; // 源文件的视频流
    AVStream *dest_video = nullptr; // 目标文件的视频流
    AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
    AVCodecContext *video_encode_ctx = nullptr; // 视频编码器的实例

    AVFilterContext *buffersrc_ctx = nullptr; // 输入滤镜的实例
    AVFilterContext *buffersink_ctx = nullptr; // 输出滤镜的实例
    AVFilterGraph *filter_graph = nullptr; // 滤镜图


    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    void processVideoFilterProcedure();

    int open_input_file(const char *src_name, const char *colorspace);

    int init_filter(const char *filters_desc);

    int open_output_file(const char *dest_name);

    int output_video(AVFrame *frame);

    int recode_video(AVPacket *packet, AVFrame *frame, AVFrame *filt_frame);

public:
    ProcessVideoToPNG(JNIEnv *env, jobject thiz);

    ~ProcessVideoToPNG();

    void startProcessVideoToPNG(const char *srcPath, const char *outPath,
                                const char *filterCmd);

};


#endif //FFMPEGPRACTICE_PROCESSVIDEOTOPNG_H
