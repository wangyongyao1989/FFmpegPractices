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

    // 必须显式初始化：入口的判空回收和析构函数都会用到这些成员，
    // 无初值时首次点击按钮就会 delete/fclose 野指针。
    HwExtractor *mHwExtractor = nullptr;

    FILE *inputFp = nullptr;

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
