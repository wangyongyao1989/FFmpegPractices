//
// Created by wangyao on 2025/9/22.
//

#ifndef FFMPEGPRACTICE_PROCESSMUXER_H
#define FFMPEGPRACTICE_PROCESSMUXER_H

#include <jni.h>
#include <thread>
#include "string"
#include "LogUtils.h"
#include "HwExtractor.h"
#include "HwMuxer.h"

class ProcessMuxer {

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


    void processProcessMuxer();

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    MUXER_OUTPUT_T getMuxerOutFormat(string fmt);

    bool writeStatsHeader();


public:
    ProcessMuxer(JNIEnv *env, jobject thiz);

    ~ProcessMuxer();

    void startProcessMuxer(const char *srcPath, const char *outPath1
                           , const char *outPath2 , const char* fmt);

};


#endif //FFMPEGPRACTICE_PROCESSMUXER_H
