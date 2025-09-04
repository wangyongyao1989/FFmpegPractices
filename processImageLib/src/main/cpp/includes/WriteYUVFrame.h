//
// Created by WANGYAO on 2025/8/31.
//

#ifndef FFMPEGPRACTICE_WRITEYUVFRAME_H
#define FFMPEGPRACTICE_WRITEYUVFRAME_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;

class WriteYUVFrame {

private:
    string writeYUVInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    thread *mThread = nullptr;

    AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
    AVStream *dest_video = nullptr; // 目标文件的视频流
    AVCodecContext *video_encode_ctx = nullptr; // 视频编码器的实例
    AVFrame *mFrame = nullptr;

    const char *mDestPath;

    char errbuf[1024];

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    static void DoThreading(WriteYUVFrame *writeYUV);

    int open_output_file(const char *dest_name);

    int output_video(AVFrame *frame);

    void processImageProcedure();


public:

    WriteYUVFrame(JNIEnv *env, jobject thiz);

    ~WriteYUVFrame();

    void startWriteYUVThread(const char *destPath);

};


#endif //FFMPEGPRACTICE_WRITEYUVFRAME_H
