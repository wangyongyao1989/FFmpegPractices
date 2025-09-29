//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/29.
//

#ifndef FFMPEGPRACTICE_MediaExtratorDecodec_H
#define FFMPEGPRACTICE_MediaExtratorDecodec_H

#include <jni.h>
#include <string>
#include <android/log.h>
#include "string"
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaMuxer.h>
#include <media/NdkMediaFormat.h>
#include "LogUtils.h"

using namespace std;

class MediaExtratorDecodec {
private:

    string callbackInfo;


    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    AMediaExtractor *extractor = nullptr;
    AMediaMuxer *muxer = nullptr;

    int videoTrackIndex = -1;
    int audioTrackIndex = -1;
    int muxerVideoTrackIndex = -1;
    int muxerAudioTrackIndex = -1;

    bool hasVideo = false;
    bool hasAudio = false;

    string sSrcPath;
    string sOutPath;

    bool initExtractor();

    bool selectTracks();

    bool initMuxer();

    bool transmux();

    void release();

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

public:
    MediaExtratorDecodec(JNIEnv *env, jobject thiz);

    ~MediaExtratorDecodec();

    void startMediaExtratorDecodec(const char *inputPath, const char *outputPath);
};


#endif //FFMPEGPRACTICE_MediaExtratorDecodec_H
