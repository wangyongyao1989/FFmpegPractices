//
// Created by wangyao on 2025/8/31.
//
#include <jni.h>
#include <string>
#include "BasicCommon.h"
#include "AndroidThreadManager.h"
#include "ProcessVideoFilter.h"
#include "ProcessVideoToPNG.h"


//包名+类名字符串定义：
const char *java_class_name = "com/wangyao/processfilterlib/ProcessFilterOperate";
using namespace std;

JavaVM *g_jvm = nullptr;
std::unique_ptr<AndroidThreadManager> g_threadManager;

ProcessVideoFilter *mProcessVideoFilter = nullptr;
ProcessVideoToPNG *mProcessVideoToPNG = nullptr;



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
cpp_process_video_filter(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath,
                         jstring filterCmd) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);
    const char *cFilterCmd = env->GetStringUTFChars(filterCmd, nullptr);

    if (mProcessVideoFilter == nullptr) {
        mProcessVideoFilter = new ProcessVideoFilter(env, thiz);
    }
    ThreadTask task = [cSrcPath, cOutPath, cFilterCmd]() {
        mProcessVideoFilter->startProcessVideoFilter(cSrcPath, cOutPath, cFilterCmd);
    };

    g_threadManager->submitTask("videoFilterThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(srcPath, cSrcPath);
    env->ReleaseStringUTFChars(filterCmd, cFilterCmd);

}

extern "C"
JNIEXPORT void JNICALL
cpp_process_video_to_png(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath,
                         jstring filterCmd) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);
    const char *cFilterCmd = env->GetStringUTFChars(filterCmd, nullptr);

    if (mProcessVideoToPNG == nullptr) {
        mProcessVideoToPNG = new ProcessVideoToPNG(env, thiz);
    }
    ThreadTask task = [cSrcPath, cOutPath, cFilterCmd]() {
        mProcessVideoToPNG->startProcessVideoToPNG(cSrcPath, cOutPath, cFilterCmd);
    };

    g_threadManager->submitTask("videoToPNGThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(srcPath, cSrcPath);
    env->ReleaseStringUTFChars(filterCmd, cFilterCmd);

}


// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        {"native_get_filter_ffmpeg_version", "()Ljava/lang/String;", (void *) cpp_string_from_jni},
        {"native_process_video_filter",      "(Ljava/lang/String;"
                                             "Ljava/lang/String;"
                                             "Ljava/lang/String;)V", (void *) cpp_process_video_filter},
        {"native_process_video_to_png",      "(Ljava/lang/String;"
                                             "Ljava/lang/String;"
                                             "Ljava/lang/String;)V", (void *) cpp_process_video_to_png},
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
    if (mProcessVideoFilter) {
        mProcessVideoFilter = nullptr;
    }
    if (mProcessVideoToPNG) {
        mProcessVideoFilter = nullptr;
    }

}