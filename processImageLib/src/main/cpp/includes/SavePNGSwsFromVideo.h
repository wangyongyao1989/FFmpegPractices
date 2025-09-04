//
// Created by wangyao on 2025/9/3.
//

#ifndef FFMPEGPRACTICE_SAVEPNGSWSFROMVIDEO_H
#define FFMPEGPRACTICE_SAVEPNGSWSFROMVIDEO_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;

class SavePNGSwsFromVideo {

private:
    string savePNGInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    thread *mThread = nullptr;

    AVFormatContext *in_fmt_ctx = nullptr; // 输入文件的封装器实例

    AVCodecContext *video_decode_ctx = nullptr;
    AVCodecContext *png_encode_ctx = nullptr;

    AVFormatContext *png_fmt_ctx = nullptr;

    string sSrcPath;
    string sDestPath;

    int save_index = 0;

    int packet_index = -1; // 数据包的索引序号

    char errbuf[1024];

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    void processImageProcedure();

    int decode_video(AVPacket *packet, AVFrame *frame, int save_index);

    int save_png_file(AVFrame *frame, int save_index);

public:
    SavePNGSwsFromVideo(JNIEnv *env, jobject thiz);

    ~SavePNGSwsFromVideo();

    void startWritePNGSws(const char *srcPath, const char *destPath);

};


#endif //FFMPEGPRACTICE_SAVEPNGSWSFROMVIDEO_H
