//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/10.
//

#ifndef FFMPEGPRACTICE_PLAYAUDIOTRACK_H
#define FFMPEGPRACTICE_PLAYAUDIOTRACK_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;

class PlayAudioTrack {
private:
    string playAudioInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    string sAudioPath;

    AVFormatContext *mFmtCtx = nullptr;
    AVCodecContext *mDecodeCtx = nullptr;
    SwrContext *mSwrCtx = nullptr; // 音频采样器的实例
    AVPacket *mPacket = nullptr;
    AVFrame *mFrame = nullptr;

    char errbuf[1024];

    int is_stop = 0; // 是否停止播放。0 不停止；1 停止

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    void JavaAudioFormatCallback(int sample_rate, int nb_channels);

    void JavaAudioBytesCallback(uint8_t *out_audio_data, int size);

    void playAudioProcedure();

    void release();


public:
    PlayAudioTrack(JNIEnv *env, jobject thiz);

    ~PlayAudioTrack();

    void startPlayAudioTrack(const char *srcPath);


    void stopPlayAudioTrack();
};


#endif //FFMPEGPRACTICE_PLAYAUDIOTRACK_H
