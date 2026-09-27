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

    // 必须显式初始化：入口的判空回收和析构函数都会用到这些成员，
    // 无初值时首次点击按钮就会 delete/fclose 野指针。
    HwMuxer *mHwMuxer = nullptr;
    HwExtractor *mHwExtractor = nullptr;

    FILE *inputFp = nullptr;

    MUXER_OUTPUT_T outputFormat = MUXER_OUTPUT_FORMAT_INVALID;


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
