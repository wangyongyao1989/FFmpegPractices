//
// Created by wangyao on 2025/9/6.
//

#ifndef FFMPEGPRACTICE_SAVEPCMOFMEIDA_H
#define FFMPEGPRACTICE_SAVEPCMOFMEIDA_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;

class SavePCMOfMeida {

private:
    string savePCMInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    string sSrcPath;
    string sDestPath;

    char errbuf[1024];


    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    void processAudioProcedure();


public:

    SavePCMOfMeida(JNIEnv *env, jobject thiz);

    ~SavePCMOfMeida();

    void startSavePCM(const char *srcPath, const char *destPath);

};


#endif //FFMPEGPRACTICE_SAVEPCMOFMEIDA_H
