//
// Created by wangyao on 2025/9/24.
//

#ifndef FFMPEGPRACTICE_PROCESSDECODEC_H
#define FFMPEGPRACTICE_PROCESSDECODEC_H
#include <jni.h>
#include <thread>
#include "string"
#include "LogUtils.h"
#include "HwExtractor.h"
#include "HwDeCodec.h"

class ProcessDeCodec {
private:
    string callbackInfo;


    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    string sSrcPath;
    string sOutPath1;
    string sOutPath2;
    string sCodecName;

    // 必须显式初始化为 nullptr：析构函数和 startProcessDecodec() 入口都会
    // 无条件判空后 fclose/delete 这几个成员，没有初值时判的是栈/堆上的随机数，
    // 首次点击按钮就会 free/delete 野指针（SIGSEGV 或堆破坏 SIGABRT）。
    FILE *inputFp = nullptr;
    FILE *outputFp = nullptr;

    HwDeCodec *pHwDeCodec = nullptr;
    HwExtractor *mHwExtractor = nullptr;



    void processProcessDecodec();

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    bool writeStatsHeader();




public:
    ProcessDeCodec(JNIEnv *env, jobject thiz);

    ~ProcessDeCodec();

    void startProcessDecodec(const char *srcPath, const char *outPath1
            , const char *outPath2,const char *codecName);


};


#endif //FFMPEGPRACTICE_PROCESSDECODEC_H
