#include <jni.h>
#include <string>
#include "BasicCommon.h"
#include "GetMediaMsg.h"
#include "GetMediaTimeBase.h"
#include "GetMeidaTimeStamp.h"
#include "CopyMeidaFile.h"
#include "PeelAudioOfMedia.h"

//包名+类名字符串定义：
const char *java_class_name = "com/wangyao/codectraninglib/CodecOperate";
using namespace std;

GetMediaMsg *getMediaMsg;
GetMediaTimeBase *getMediaTimeBase;
GetMeidaTimeStamp *getMediaTimeStamp;
CopyMeidaFile *copyMeidaFile;
PeelAudioOfMedia *peelAudioOfMedia;

extern "C"
JNIEXPORT jstring JNICALL
cpp_string_from_jni(JNIEnv *env, jobject thiz) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
cpp_get_media_msg(JNIEnv *env, jobject thiz, jstring mediaPath) {
    const char *cMediaPath = env->GetStringUTFChars(mediaPath, nullptr);

    if (getMediaMsg == nullptr) {
        getMediaMsg = new GetMediaMsg();
    }

    const string &mediaInfo = getMediaMsg->getMediaMsg(cMediaPath);

    env->ReleaseStringUTFChars(mediaPath, cMediaPath);

    return env->NewStringUTF(mediaInfo.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
cpp_get_media_time_base(JNIEnv *env, jobject thiz, jstring mediaPath) {
    const char *cMediaPath = env->GetStringUTFChars(mediaPath, nullptr);

    if (getMediaTimeBase == nullptr) {
        getMediaTimeBase = new GetMediaTimeBase();
    }

    const string &timeBaseInfo = getMediaTimeBase->getMediaTimeBase(cMediaPath);

    env->ReleaseStringUTFChars(mediaPath, cMediaPath);

    return env->NewStringUTF(timeBaseInfo.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
cpp_get_media_time_stamp(JNIEnv *env, jobject thiz, jstring mediaPath) {
    const char *cMediaPath = env->GetStringUTFChars(mediaPath, nullptr);

    if (getMediaTimeStamp == nullptr) {
        getMediaTimeStamp = new GetMeidaTimeStamp();
    }

    const string &timeStampInfo = getMediaTimeStamp->getMediaTimeStamp(cMediaPath);

    env->ReleaseStringUTFChars(mediaPath, cMediaPath);

    return env->NewStringUTF(timeStampInfo.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
cpp_copy_media_file(JNIEnv *env, jobject thiz, jstring srcPath, jstring destPath) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cDestPath = env->GetStringUTFChars(destPath, nullptr);

    if (copyMeidaFile == nullptr) {
        copyMeidaFile = new CopyMeidaFile();
    }
    const string &mediaFile = copyMeidaFile->copyMediaFile(cSrcPath, cDestPath);

    env->ReleaseStringUTFChars(srcPath, cSrcPath);
    env->ReleaseStringUTFChars(destPath, cDestPath);

    return env->NewStringUTF(mediaFile.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
cpp_peel_audio_of_media(JNIEnv *env, jobject thiz, jstring srcPath, jstring destPath) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cDestPath = env->GetStringUTFChars(destPath, nullptr);

    if (peelAudioOfMedia == nullptr) {
        peelAudioOfMedia = new PeelAudioOfMedia();
    }
    const string &peelAudioFile = peelAudioOfMedia->peelAudioOfMedia(cSrcPath, cDestPath);

    env->ReleaseStringUTFChars(srcPath, cSrcPath);
    env->ReleaseStringUTFChars(destPath, cDestPath);

    return env->NewStringUTF(peelAudioFile.c_str());
}

// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        {"native_string_from_jni",      "()Ljava/lang/String;",                   (void *) cpp_string_from_jni},
        {"native_get_media_msg",        "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_media_msg},
        {"native_get_media_time_base",  "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_media_time_base},
        {"native_get_media_time_stamp", "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_media_time_stamp},
        {"native_copy_media_file",      "(Ljava/lang/String;"
                                        "Ljava/lang/String;)Ljava/lang/String;",  (void *) cpp_copy_media_file},
        {"native_peel_audio_of_media",  "(Ljava/lang/String;"
                                        "Ljava/lang/String;)Ljava/lang/String;",  (void *) cpp_peel_audio_of_media},
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