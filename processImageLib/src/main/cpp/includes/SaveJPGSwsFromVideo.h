//
// Created by wangyao on 2025/9/3.
//

#ifndef FFMPEGPRACTICE_SAVEJPGSWSFROMVIDEO_H
#define FFMPEGPRACTICE_SAVEJPGSWSFROMVIDEO_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;


class SaveJPGSwsFromVideo {
private:
    string saveJPGInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    AVFormatContext *in_fmt_ctx = nullptr; // 输入文件的封装器实例

    AVCodecContext *video_decode_ctx = nullptr;
    AVCodecContext *jpg_encode_ctx = nullptr;

    AVFormatContext *jpg_fmt_ctx = nullptr;


    string sSrcPath;
    string sDestPath;

    int save_index = 0;

    int packet_index = -1; // 数据包的索引序号

    char errbuf[1024];

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    void processImageProcedure();

    int decode_video(AVPacket *packet, AVFrame *frame, int save_index);

    int save_jpg_file(AVFrame *frame, int save_index);

public:
    SaveJPGSwsFromVideo(JNIEnv *env, jobject thiz);

    ~SaveJPGSwsFromVideo();

    void startWriteJPGSws(const char *srcPath, const char *destPath);
};


#endif //FFMPEGPRACTICE_SAVEJPGSWSFROMVIDEO_H
