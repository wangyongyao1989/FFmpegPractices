//  Author : wangyao https://github.com/wangyongyao1989
// RTSP 拉流播放器 JNI 入口：动态注册 + 线程池任务派发
#include <jni.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include "BasicCommon.h"
#include "AndroidThreadManager.h"
#include "RtspPlayer.h"

//包名+类名字符串定义：
const char *java_class_name = "com/wangyao/rtsplib/RtspOperate";
using namespace std;

JavaVM *g_jvm = nullptr;
std::unique_ptr<AndroidThreadManager> g_threadManager;

RtspPlayer *gRtspPlayer = nullptr;

extern "C"
JNIEXPORT jstring JNICALL
cpp_get_rtsp_version(JNIEnv *env, jobject thiz) {
    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "libavcodec : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVCODEC_VERSION));
    strcat(strBuffer, "\nlibavformat : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFORMAT_VERSION));
    strcat(strBuffer, "\nlibavutil : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVUTIL_VERSION));
    strcat(strBuffer, "\nlibswresample : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWRESAMPLE_VERSION));
    strcat(strBuffer, "\nlibswscale : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWSCALE_VERSION));
    strcat(strBuffer, "\nrtsp muxer/demuxer 已集成，支持 h264/hevc + aac/mp3 拉流\n");
    LOGD("GetRTSPFFmpegVersion\n%s", strBuffer);
    return env->NewStringUTF(strBuffer);
}

extern "C"
JNIEXPORT void JNICALL
cpp_rtsp_play(JNIEnv *env, jobject thiz, jstring rtspUrl, jobject surface,
              jboolean hardDecode) {
    const char *cUrl = env->GetStringUTFChars(rtspUrl, nullptr);
    if (gRtspPlayer == nullptr) {
        gRtspPlayer = new RtspPlayer(env, thiz);
    }
    char *urlCopy = strdup(cUrl);
    jobject surfaceGlobal = env->NewGlobalRef(surface);

    // 任务内先等上一会话收尾，再 play + 看门狗收尾
    ThreadTask realTask = [urlCopy, surfaceGlobal, hardDecode]() {
        JNIEnv *envTask = nullptr;
        bool isAttach = false;
        if (g_jvm->GetEnv((void **) &envTask, JNI_VERSION_1_6) == JNI_EDETACHED) {
            g_jvm->AttachCurrentThread(&envTask, nullptr);
            isAttach = true;
        }
        for (int i = 0; gRtspPlayer->isPlaying() && i < 100; i++) {
            gRtspPlayer->requestStop();
            usleep(50 * 1000);
        }
        gRtspPlayer->play(urlCopy, surfaceGlobal, hardDecode);
        gRtspPlayer->finishSession();
        free(urlCopy);
        if (envTask && surfaceGlobal) {
            envTask->DeleteGlobalRef(surfaceGlobal);
        }
        if (isAttach) {
            g_jvm->DetachCurrentThread();
        }
    };
    g_threadManager->submitTask("rtspPlayThread", realTask, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(rtspUrl, cUrl);
}

extern "C"
JNIEXPORT void JNICALL
cpp_rtsp_stop(JNIEnv *env, jobject thiz) {
    if (gRtspPlayer != nullptr) {
        ThreadTask task = []() {
            gRtspPlayer->requestStop();
        };
        g_threadManager->submitTask("rtspStopThread", task, PRIORITY_NORMAL);
    }
}

// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        {"native_get_rtsp_version", "()Ljava/lang/String;",             (void *) cpp_get_rtsp_version},
        {"native_rtsp_play",        "(Ljava/lang/String;"
                                    "Landroid/view/Surface;"
                                    "Z)V",                              (void *) cpp_rtsp_play},
        {"native_rtsp_stop",        "()V",                              (void *) cpp_rtsp_stop},
};

/**
 * 定义注册方法
 * @param vm
 * @param reserved
 * @return
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD("RTSP 动态注册");
    JNIEnv *env;
    if ((vm)->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        LOGD("RTSP 动态注册GetEnv  fail");
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
    if (regist_result) {
        LOGE("RTSP 动态注册 fail regist_result = %d", regist_result);
    } else {
        LOGI("RTSP 动态注册 success result = %d", regist_result);
    }

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
    if (gRtspPlayer) {
        delete gRtspPlayer;
        gRtspPlayer = nullptr;
    }
    g_threadManager.reset();
}
