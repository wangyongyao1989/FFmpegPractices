//
// Created by wangyao on 2025/11/10.
//
#include <jni.h>
#include <string>
#include "BasicCommon.h"
#include "AndroidThreadManager.h"
#include "PlayAudioTrack.h"
#include "FFmpegOpenSLPlayer.h"
#include "FFSurfacePlayer.h"
#include "FFGLPlayer.h"

//包名+类名字符串定义：
const char *java_class_name = "com/wangyao/playaudiolib/PlayMediaOperate";
using namespace std;

JavaVM *g_jvm = nullptr;
std::unique_ptr<AndroidThreadManager> g_threadManager;

PlayAudioTrack *playAudioTrack = nullptr;
FFmpegOpenSLPlayer *fFmpegOpenSLPlayer = nullptr;
FFSurfacePlayer *fFSurfacePlayer = nullptr;
FFGLPlayer *fFGLPlayer = nullptr;

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
cpp_play_audio_by_track(JNIEnv *env, jobject thiz, jstring audioPath) {
    const char *cAudioPath = env->GetStringUTFChars(audioPath, nullptr);
    if (playAudioTrack == nullptr) {
        playAudioTrack = new PlayAudioTrack(env, thiz);
    }

    ThreadTask task = [cAudioPath]() {
        playAudioTrack->startPlayAudioTrack(cAudioPath);
    };
    g_threadManager->submitTask("play_audio_by_track", task, PRIORITY_NORMAL);
    env->ReleaseStringUTFChars(audioPath, cAudioPath);
}

extern "C"
JNIEXPORT void JNICALL
cpp_stop_audio_by_track(JNIEnv *env, jobject thiz) {
    if (playAudioTrack) {
        playAudioTrack->stopPlayAudioTrack();
    }
}

extern "C"
JNIEXPORT void JNICALL
cpp_play_audio_by_opensl(JNIEnv *env, jobject thiz, jstring audioPath) {
    const char *cAudioPath = env->GetStringUTFChars(audioPath, nullptr);

    if (fFmpegOpenSLPlayer == nullptr) {
        fFmpegOpenSLPlayer = new FFmpegOpenSLPlayer(env, thiz);
    }

    bool result = fFmpegOpenSLPlayer->init(std::string(cAudioPath));
    if (result) {
        fFmpegOpenSLPlayer->start();
    }

    env->ReleaseStringUTFChars(audioPath, cAudioPath);
}

extern "C"
JNIEXPORT void JNICALL
cpp_stop_audio_by_opensl(JNIEnv *env, jobject thiz) {
    if (fFmpegOpenSLPlayer != nullptr) {
        fFmpegOpenSLPlayer->stop();
    }
}

extern "C"
JNIEXPORT void JNICALL
cpp_init_video_by_surface(JNIEnv *env, jobject thiz, jstring intputUrl, jobject surface) {
    const char *url = env->GetStringUTFChars(intputUrl, 0);
    if (fFSurfacePlayer == nullptr) {
        fFSurfacePlayer = new FFSurfacePlayer(env, thiz);
    }
    fFSurfacePlayer->init(url, surface);
//    fFSurfacePlayer->start();
    env->ReleaseStringUTFChars(intputUrl, url);
}

extern "C"
JNIEXPORT void JNICALL
cpp_uninit_video_by_surface(JNIEnv *env, jobject thiz) {
    if (fFSurfacePlayer != nullptr) {
        fFSurfacePlayer->stop();
    }
}

extern "C"
JNIEXPORT void JNICALL
cpp_play_video_by_surface(JNIEnv *env, jobject thiz) {
    if (fFSurfacePlayer != nullptr) {
        fFSurfacePlayer->start();
    }
}

extern "C"
JNIEXPORT void JNICALL
cpp_stop_video_by_surface(JNIEnv *env, jobject thiz) {
    if (fFSurfacePlayer != nullptr) {
        fFSurfacePlayer->stop();
    }
}

//GL视频播放
extern "C"
JNIEXPORT void JNICALL
cpp_init_video_by_gl(JNIEnv *env, jobject thiz, jstring intputUrl, jstring fragPath,
                     jstring vertexPath, jobject surface) {
    const char *url = env->GetStringUTFChars(intputUrl, 0);
    const char *cFragPath = env->GetStringUTFChars(fragPath, 0);
    const char *cVertexPath = env->GetStringUTFChars(vertexPath, 0);

    if (fFGLPlayer == nullptr) {
        fFGLPlayer = new FFGLPlayer(env, thiz);
    }
    fFGLPlayer->init(url, cFragPath, cVertexPath, surface);
    env->ReleaseStringUTFChars(intputUrl, url);
    env->ReleaseStringUTFChars(fragPath, cFragPath);
    env->ReleaseStringUTFChars(vertexPath, cVertexPath);

}

extern "C"
JNIEXPORT void JNICALL
cpp_uninit_video_by_gl(JNIEnv *env, jobject thiz) {
    if (fFGLPlayer != nullptr) {
        fFGLPlayer->stop();
    }
}

extern "C"
JNIEXPORT void JNICALL
cpp_play_video_by_gl(JNIEnv *env, jobject thiz) {
    if (fFGLPlayer != nullptr) {
        fFGLPlayer->start();
    }
}

extern "C"
JNIEXPORT void JNICALL
cpp_stop_video_by_gl(JNIEnv *env, jobject thiz) {
    if (fFGLPlayer != nullptr) {
        fFGLPlayer->stop();
    }
}


// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        //音频播放
        {"native_string_from_jni",         "()Ljava/lang/String;",      (void *) cpp_string_from_jni},
        {"native_play_audio_by_track",     "(Ljava/lang/String;)V",     (void *) cpp_play_audio_by_track},
        {"native_stop_audio_by_track",     "()V",                       (void *) cpp_stop_audio_by_track},
        {"native_play_audio_by_opensl",    "(Ljava/lang/String;)V",     (void *) cpp_play_audio_by_opensl},
        {"native_stop_audio_by_opensl",    "()V",                       (void *) cpp_stop_audio_by_opensl},

        //视频播放
        {"native_init_video_by_surface",   "(Ljava/lang/String"
                                           ";Landroid/view/Surface;)V", (void *) cpp_init_video_by_surface},
        {"native_uninit_video_by_surface", "()V",                       (void *) cpp_uninit_video_by_surface},
        {"native_play_video_by_surface",   "()V",                       (void *) cpp_play_video_by_surface},
        {"native_stop_video_by_surface",   "()V",                       (void *) cpp_stop_video_by_surface},

        //GL视频播放
        {"native_init_video_by_gl",        "(Ljava/lang/String;"
                                           "Ljava/lang/String;"
                                           "Ljava/lang/String;"
                                           "Landroid/view/Surface;"
                                           ")V",                        (void *) cpp_init_video_by_gl},
        {"native_play_video_by_gl",        "()V",                       (void *) cpp_play_video_by_gl},
        {"native_stop_video_by_gl",        "()V",                       (void *) cpp_stop_video_by_gl},
        {"native_uninit_video_by_gl",      "()V",                       (void *) cpp_uninit_video_by_gl},

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
    if (playAudioTrack) {
        playAudioTrack = nullptr;
    }

    if (fFmpegOpenSLPlayer) {
        fFmpegOpenSLPlayer = nullptr;
    }

    if (fFGLPlayer) {
        fFGLPlayer = nullptr;
    }

}