//
// Created by wangyao on 2025/8/31.
//
#include <jni.h>
#include <string>
#include "BasicCommon.h"
#include "AndroidThreadManager.h"
#include "SavePCMOfMeida.h"
#include "SaveAACOfMedia.h"


//包名+类名字符串定义：
const char *java_class_name = "com/wangyao/processaudiolib/ProcessAuidoOperate";
using namespace std;

JavaVM *g_jvm = nullptr;
std::unique_ptr<AndroidThreadManager> g_threadManager;

SavePCMOfMeida *mSavePCMOfMeida;
SaveAACOfMedia *mSaveAACOfMedia;

extern "C"
JNIEXPORT jstring JNICALL
cpp_string_from_jni(JNIEnv *env, jobject thiz) {
    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "libavcodec : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVCODEC_VERSION));
    strcat(strBuffer, "\nlibavformat : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFORMAT_VERSION));
    strcat(strBuffer, "\nlibavutil : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVUTIL_VERSION));
    strcat(strBuffer, "\nlibavfilter : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFILTER_VERSION));
    strcat(strBuffer, "\nlibswresample : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWRESAMPLE_VERSION));
    strcat(strBuffer, "\nlibswscale : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWSCALE_VERSION));
    strcat(strBuffer, "\navcodec_configure : \n");
    strcat(strBuffer, avcodec_configuration());
    strcat(strBuffer, "\navcodec_license : ");
    strcat(strBuffer, avcodec_license());
    LOGD("GetFFmpegVersion\n%s", strBuffer);
    return env->NewStringUTF(strBuffer);
}

extern "C"
JNIEXPORT void JNICALL
cpp_save_pcm_of_media(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);

    if (mSavePCMOfMeida == nullptr) {
        mSavePCMOfMeida = new SavePCMOfMeida(env, thiz);
    }

    ThreadTask task = [cSrcPath, cOutPath]() {
        mSavePCMOfMeida->startSavePCM(cSrcPath, cOutPath);
    };

    g_threadManager->submitTask("savePCMThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(srcPath, cSrcPath);
}

extern "C"
JNIEXPORT void JNICALL
cpp_save_aac_of_media(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);

    if (mSaveAACOfMedia == nullptr) {
        mSaveAACOfMedia = new SaveAACOfMedia(env, thiz);
    }

    ThreadTask task = [cSrcPath, cOutPath]() {
        mSaveAACOfMedia->startSaveAAC(cSrcPath, cOutPath);
    };

    g_threadManager->submitTask("saveAACThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(srcPath, cSrcPath);
}

// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        {"native_get_audio_ffmpeg_version", "()Ljava/lang/String;", (void *) cpp_string_from_jni},
        {"native_save_pcm_of_media",        "(Ljava/lang/String;"
                                            "Ljava/lang/String;)V", (void *) cpp_save_pcm_of_media},
        {"native_save_aac_of_media",        "(Ljava/lang/String;"
                                            "Ljava/lang/String;)V", (void *) cpp_save_aac_of_media},
};




/**
 * 定义注册方法
 * @param vm
 * @param reserved
 * @return
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD("动态注册");
    JNIEnv *env;
    if ((vm)->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        LOGD("动态注册GetEnv  fail");
        return JNI_ERR;
    }
    g_jvm = vm;
    g_threadManager = std::make_unique<AndroidThreadManager>(vm);

    // 初始化线程池
    ThreadPoolConfig config;
    config.minThreads = 2;
    config.maxThreads = 4;
    config.idleTimeoutMs = 30000;
    config.queueSize = 50;
    g_threadManager->initThreadPool(config);

    // 获取类引用
    jclass clazz = env->FindClass(java_class_name);
    // 注册native方法
    jint regist_result = env->RegisterNatives(clazz, methods,
                                              sizeof(methods) / sizeof(methods[0]));
    if (regist_result) { // 非零true 进if
        LOGE("动态注册 fail regist_result = %d", regist_result);
    } else {
        LOGI("动态注册 success result = %d", regist_result);
    }

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
    g_threadManager.reset();
    if (mSavePCMOfMeida) {
        mSavePCMOfMeida = nullptr;
    }
}