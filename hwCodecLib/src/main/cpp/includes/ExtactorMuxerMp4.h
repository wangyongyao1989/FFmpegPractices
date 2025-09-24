//
// Created by wangyao on 2025/9/24.
//

#ifndef FFMPEGPRACTICE_ExtactorMuxerMp4_H
#define FFMPEGPRACTICE_ExtactorMuxerMp4_H
//#define LOG_NDEBUG 0
#define LOG_TAG "muxer"

#include <jni.h>
#include <thread>
#include "string"
#include "LogUtils.h"
#include "HwExtractor.h"
#include "HwMuxer.h"


class ExtactorMuxerMp4 {
private:
    string callbackInfo;


    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    string sSrcPath;
    string sOutPath1;
    string sOutPath2;
    string sFmt;

    HwMuxer *mHwMuxer;
    HwExtractor *mHwExtractor;

    FILE *inputFp;

    MUXER_OUTPUT_T outputFormat;


    void processExtactorMuxerMp4();

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    MUXER_OUTPUT_T getMuxerOutFormat(string fmt);

    bool writeStatsHeader();


public:
    ExtactorMuxerMp4(JNIEnv *env, jobject thiz);

    ~ExtactorMuxerMp4();

    void startExtactorMuxerMp4(const char *srcPath, const char *outPath1
            , const char *outPath2 , const char* fmt);
};


#endif //FFMPEGPRACTICE_ExtactorMuxerMp4_H
