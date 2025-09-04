//
// Created by wangyao on 2025/9/4.
//

#ifndef FFMPEGPRACTICE_SAVEIMAGE2VIDEO_H
#define FFMPEGPRACTICE_SAVEIMAGE2VIDEO_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;

class SaveImage2Video {

private:
    string image2VideoInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    string sSrcPath1;
    string sSrcPath2;
    string sDestPath;

    char errbuf[1024];


    AVFormatContext *in_fmt_ctx[2] = {NULL, NULL}; // 输入文件的封装器实例
    AVCodecContext *image_decode_ctx[2] = {NULL, NULL}; // 图像解码器的实例
    int video_index[2] = {-1 - 1}; // 视频流的索引
    AVStream *src_video = NULL; // 源文件的视频流
    AVStream *dest_video = NULL; // 目标文件的视频流
    AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
    AVCodecContext *video_encode_ctx = NULL; // 视频编码器的实例
    struct SwsContext *swsContext[2] = {NULL, NULL}; // 图像转换器的实例
    AVFrame *yuv_frame[2] = {NULL, NULL}; // YUV数据帧
    int packet_index = 0; // 数据帧的索引序号

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    void processImageProcedure();

    int open_input_file(int seq, const char *src_name);

    int open_output_file(const char *dest_name);

    int output_video(AVFrame *frame);

    int init_sws_context(int seq);

    int recode_video(int seq, AVPacket *packet, AVFrame *frame);

public:

    SaveImage2Video(JNIEnv *env, jobject thiz);

    ~SaveImage2Video();

    void startImage2Video(const char *srcPath1, const char *srcPath2, const char *destPath);
};


#endif //FFMPEGPRACTICE_SAVEIMAGE2VIDEO_H
