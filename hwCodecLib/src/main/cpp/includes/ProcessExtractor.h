//
// Created by wangyao on 2025/9/21.
//

#ifndef FFMPEGPRACTICE_PROCESSEXTRACTOR_H
#define FFMPEGPRACTICE_PROCESSEXTRACTOR_H

#include <jni.h>
#include <thread>
#include "string"
#include "LogUtils.h"
#include "HwExtractor.h"

using namespace std;

class ProcessExtractor {

private:
    string callbackInfo;


    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    string sSrcPath;
    string sOutPath;

    HwExtractor *mHwExtractor;

    FILE *inputFp;

    void processProcessExtractor();

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    bool writeStatsHeader();


public:
    ProcessExtractor(JNIEnv *env, jobject thiz);

    ~ProcessExtractor();

    void startProcessExtractor(const char *srcPath, const char *outPath);

};


#endif //FFMPEGPRACTICE_PROCESSEXTRACTOR_H
