//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/1.
//

#ifndef FFMPEGPRACTICE_SAVEYUVFROMVIDEO_H
#define FFMPEGPRACTICE_SAVEYUVFROMVIDEO_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;

class SaveYUVFromVideo {

private:

    string saveYUVInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    thread *mThread = nullptr;

//    const char *mSrcPath = nullptr;
//    const char *mDestPath = nullptr;

    string sSrcPath;
    string sDestPath;

    AVFormatContext *in_fmt_ctx = nullptr; // 输入文件的封装器实例
    AVCodecContext *video_decode_ctx = nullptr; // 视频解码器的实例

    AVPacket *packet = nullptr;
    AVFrame *frame = nullptr;

    int packet_index = -1; // 数据包的索引序号

    int save_index = 0;

    char errbuf[1024];

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    static void DoThreading(SaveYUVFromVideo *saveYUV);

    void processImageProcedure();


    int save_yuv_file(AVFrame *frame, int save_index);

    int decode_video(AVPacket *packet, AVFrame *frame, int save_index);

public:

    SaveYUVFromVideo(JNIEnv *env, jobject thiz);

    ~SaveYUVFromVideo();

    void startWriteYUVThread(const char *srcPath, const char *destPath);
};


#endif //FFMPEGPRACTICE_SAVEYUVFROMVIDEO_H
