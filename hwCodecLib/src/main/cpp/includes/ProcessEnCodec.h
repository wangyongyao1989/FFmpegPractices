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

    // 必须显式初始化为 nullptr：析构函数和 startProcessEnCodec() 入口都会
    // 无条件判空后 fclose/delete 这几个成员，没有初值时判的是随机数，
    // 首次点击按钮就会 free/delete 野指针（SIGSEGV 或堆破坏 SIGABRT）。
    FILE *inputFp = nullptr;
    FILE *outputFp = nullptr;

    HwDeCodec *pHwDeCodec = nullptr;
    HwEnCodec *pHwEnCodec = nullptr;


    HwExtractor *mHwExtractor = nullptr;


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
