#include <jni.h>
#include <string>
#include "BasicCommon.h"
#include "FFGetVersion.h"
#include "FFGetVideoMsg.h"
#include "FFGetMediaMsg.h"
#include "FFGetMediaCodecMsg.h"
#include "FFGetMediaCodecCopyNew.h"
#include "FFWriteMediaToMp4.h"
#include "FFWriteMediaFilter.h"

//包名+类名字符串定义：
const char *java_class_name = "com/wangyongyao/basictraninglib/FFmpegOperate";
using namespace std;

FFGetVideoMsg *ffGetVideoMsg;
FFGetVersion *ffGetVersion;
FFGetMediaMsg *ffGetMediaMsg;
FFGetMediaCodecMsg *ffGetMediaCodecMsg;
FFGetMediaCodecCopyNew *ffGetMediaCodecCopyNew;
FFWriteMediaToMp4 *ffWriteMediaToMp4;
FFWriteMediaFilter *ffWriteMediaFilter;

extern "C"
JNIEXPORT jstring JNICALL
cpp_string_from_jni(JNIEnv *env, jobject thiz) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}


JNIEXPORT jstring JNICALL
cpp_get_ffmpeg_version(JNIEnv *env, jobject thiz) {
    if (ffGetVersion == nullptr)
        ffGetVersion = new FFGetVersion();
    const string &versionMsg = ffGetVersion->getVersion();
    return env->NewStringUTF(versionMsg.c_str());
}

/**
 * 练习一：打开关闭一个音视频流，并获取其内部的信息
 */
JNIEXPORT jstring JNICALL
cpp_get_video_msg(JNIEnv *env, jobject thiz, jstring videoPath) {

    const char *cFragPath = env->GetStringUTFChars(videoPath, nullptr);
    LOGI("cpp_get_video_msg=== %s", cFragPath);

    if (ffGetVideoMsg == nullptr) {
        ffGetVideoMsg = new FFGetVideoMsg();
    }
    const string &videoString = ffGetVideoMsg->getVideoMsg(cFragPath);
    env->ReleaseStringUTFChars(videoPath, cFragPath);
    return env->NewStringUTF(videoString.c_str());
}

/**
 *  练习二：打开一个音视频文件后，打印出视频及音频相关信息
 */
JNIEXPORT jstring JNICALL
cpp_get_media_msg(JNIEnv *env, jobject thiz, jstring videoPath) {

    const char *cFragPath = env->GetStringUTFChars(videoPath, nullptr);
    LOGI("cpp_get_media_msg=== %s", cFragPath);
    if (ffGetMediaMsg == nullptr) {
        ffGetMediaMsg = new FFGetMediaMsg();
    }
    const string &mediaMsgString = ffGetMediaMsg->getMediaMsg(cFragPath);
    env->ReleaseStringUTFChars(videoPath, cFragPath);
    return env->NewStringUTF(mediaMsgString.c_str());
}

/**
 * 练习三：打开一个音视频文件后，获取视频及音频的解码ID及解码器名字。
 */
JNIEXPORT jstring JNICALL
cpp_get_media_codec_msg(JNIEnv *env, jobject thiz, jstring videoPath) {

    const char *cFragPath = env->GetStringUTFChars(videoPath, nullptr);
    LOGI("cpp_get_media_codec_msg videoPath=== %s", cFragPath);

    if (ffGetMediaCodecMsg == nullptr) {
        ffGetMediaCodecMsg = new FFGetMediaCodecMsg();
    }
    const string &mediaCodecMsg = ffGetMediaCodecMsg->getMediaCodecMsg(cFragPath);

    env->ReleaseStringUTFChars(videoPath, cFragPath);
    return env->NewStringUTF(mediaCodecMsg.c_str());
}

/**
 * 练习四：把音视频流中的编码参数复制给解码器实例参数。
 */
JNIEXPORT jstring JNICALL
cpp_media_codec_copy_new_to_codec(JNIEnv *env, jobject thiz, jstring videoPath) {

    const char *cFragPath = env->GetStringUTFChars(videoPath, nullptr);
    LOGI("cpp_media_codec_copy_new_to_codec=== %s", cFragPath);
    if (ffGetMediaCodecCopyNew == nullptr) {
        ffGetMediaCodecCopyNew = new FFGetMediaCodecCopyNew();
    }

    const string &infoMsg = ffGetMediaCodecCopyNew->getMediaCodecCopy(cFragPath);

    env->ReleaseStringUTFChars(videoPath, cFragPath);
    return env->NewStringUTF(infoMsg.c_str());
}

/**
 * 练习五：写入一个音视频文件的封装实例
 */
JNIEXPORT jint JNICALL
cpp_write_media_to_mp4(JNIEnv *env, jobject thiz, jstring outPath) {

    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);
    LOGI("cpp_write_media_to_mp4=== %s", cOutPath);
    if (ffWriteMediaToMp4 == nullptr) {
        ffWriteMediaToMp4 = new FFWriteMediaToMp4();
    }

    const int &toMp4 = ffWriteMediaToMp4->writeMediaToMp4(cOutPath);

    if (toMp4 < 0) {
        env->ReleaseStringUTFChars(outPath, cOutPath);
        return -1;
    }
    env->ReleaseStringUTFChars(outPath, cOutPath);

    return 0;
}


JNIEXPORT jstring JNICALL
cpp_write_media_filter(JNIEnv *env, jobject thiz, jstring outPath) {

    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);
    string retString = "cpp_write_media_filter:";
    LOGI("cpp_write_media_filter=== %s", cOutPath);
    if (ffWriteMediaFilter == nullptr) {
        ffWriteMediaFilter = new FFWriteMediaFilter();
    }

    const string &filter = ffWriteMediaFilter->writeMediaFilter(cOutPath);
    retString = retString + filter;
    env->ReleaseStringUTFChars(outPath, cOutPath);

    return env->NewStringUTF(retString.c_str());
}

// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        {"native_string_from_jni",       "()Ljava/lang/String;",                   (void *) cpp_string_from_jni},
        {"native_get_ffmpeg_version",    "()Ljava/lang/String;",                   (void *) cpp_get_ffmpeg_version},
        {"native_get_video_msg",         "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_video_msg},
        {"native_get_media_msg",         "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_media_msg},
        {"native_get_media_codec_msg",   "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_media_codec_msg},
        {"native_media_copy_to_decodec", "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_media_codec_copy_new_to_codec},
        {"native_write_media_to_mp4",    "(Ljava/lang/String;)I",                  (void *) cpp_write_media_to_mp4},
        {"native_write_media_filter",    "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_write_media_filter},

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