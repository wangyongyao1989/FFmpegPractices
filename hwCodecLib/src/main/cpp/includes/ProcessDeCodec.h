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

    FILE *inputFp;
    FILE *outputFp;

    HwDeCodec *pHwDeCodec;
    HwExtractor *mHwExtractor;



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
