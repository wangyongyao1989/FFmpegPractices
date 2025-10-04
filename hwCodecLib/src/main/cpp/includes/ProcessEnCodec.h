//
// Created by wangyao on 2025/9/24.
//

#ifndef FFMPEGPRACTICE_ProcessEnCodec_H
#define FFMPEGPRACTICE_ProcessEnCodec_H

#include <jni.h>
#include <thread>
#include "string"
#include "LogUtils.h"
#include "HwExtractor.h"
#include "HwDeCodec.h"
#include "HwEnCodec.h"

class ProcessEnCodec {
private:
    string callbackInfo;
    int32_t kEncodeDefaultVideoBitRate = 8000000 /* 8 Mbps */;
    int32_t kEncodeMinVideoBitRate = 600000 /* 600 Kbps */;
    int32_t kEncodeDefaultAudioBitRate = 128000 /* 128 Kbps */;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    string sSrcPath;
    string sOutPath1;
    string sOutPath2;
    string sCodecName;

    FILE *inputFp;
    FILE *outputFp;

    HwDeCodec *pHwDeCodec;
    HwEnCodec *pHwEnCodec;


    HwExtractor *mHwExtractor;


    void processProcessEnCodec();

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    bool writeStatsHeader();


public:
    ProcessEnCodec(JNIEnv *env, jobject thiz);

    ~ProcessEnCodec();

    void startProcessEnCodec(const char *srcPath, const char *outPath1, const char *outPath2,
                             const char *codecName);


};


#endif //FFMPEGPRACTICE_ProcessEnCodec_H
