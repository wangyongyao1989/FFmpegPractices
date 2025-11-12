// Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/11.
//

#ifndef FFMPEGPRACTICE_PLAYOPENSL_H
#define FFMPEGPRACTICE_PLAYOPENSL_H

#include <jni.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "BasicCommon.h"
#include "string"
#include "OpenslHelper.h"

using namespace std;

class PlayOpenSL {
private:
    string playAudioInfo;
    mutex audioMutex;
    condition_variable audioCondition;
    bool isDataReady = false;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    string sAudioPath;

    AVFormatContext *mFmtCtx = nullptr;
    AVCodecContext *mDecodeCtx = nullptr;
    SwrContext *mSwrCtx = nullptr;
    AVPacket *mPacket = nullptr;
    AVFrame *mFrame = nullptr;

    OpenslHelper helper;
    uint8_t *outBuffer = nullptr;

    int channel_count = 0;
    int out_buffer_size = 0;
    int audio_index = -1;
    bool is_finished = false;
    int current_buffer_size = 0;

    volatile bool is_stop = false;
    volatile bool is_playing = false;

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    void playAudioProcedure();

    void release();

    static void playerCallback(SLAndroidSimpleBufferQueueItf bq, void *pContext);

    void getAudioData();

    bool decodeAudioFrame();

public:
    PlayOpenSL(JNIEnv *env, jobject thiz);

    ~PlayOpenSL();

    void startPlayOpenSL(const char *srcPath);

    void stopPlayOpenSL();
};

#endif //FFMPEGPRACTICE_PLAYOPENSL_H