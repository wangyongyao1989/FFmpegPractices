//
// Created by wangyao on 2025/11/10.
//
#include <jni.h>
#include <string>
#include "BasicCommon.h"
#include "PlayAudioTrack.h"


//包名+类名字符串定义：
const char *java_class_name = "com/wangyao/playaudiolib/PlayAudioOperate";
using namespace std;

JavaVM *g_jvm = nullptr;

PlayAudioTrack *playAudioTrack = nullptr;


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
    LOGE("cpp_play_audio_by_track");
    if (playAudioTrack == nullptr) {
        LOGE("cpp_play_audio_by_track");

        playAudioTrack = new PlayAudioTrack(env, thiz);
    }
    LOGE("cpp_play_audio_by_track11111");

    playAudioTrack->startPlayAudioTrack(cAudioPath);

    env->ReleaseStringUTFChars(audioPath, cAudioPath);
}

extern "C"
JNIEXPORT void JNICALL
cpp_stop_audio_by_track(JNIEnv *env, jobject thiz) {
    if (playAudioTrack) {
        playAudioTrack->stopPlayAudioTrack();
    }
}


// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        {"native_string_from_jni",     "()Ljava/lang/String;",  (void *) cpp_string_from_jni},
        {"native_play_audio_by_track", "(Ljava/lang/String;)V", (void *) cpp_play_audio_by_track},
        {"native_stop_audio_by_track", "()V",                   (void *) cpp_stop_audio_by_track},

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

}