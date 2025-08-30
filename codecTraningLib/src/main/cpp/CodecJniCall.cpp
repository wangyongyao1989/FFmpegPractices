#include <jni.h>
#include <string>
#include "BasicCommon.h"
#include "GetMediaMsg.h"
#include "GetMediaTimeBase.h"
#include "GetMeidaTimeStamp.h"
#include "CopyMeidaFile.h"
#include "PeelAudioOfMedia.h"
#include "SplitVideoOfMedia.h"
#include "MergeAudio.h"
#include "RecodecVideo.h"

//包名+类名字符串定义：
const char *java_class_name = "com/wangyao/codectraninglib/CodecOperate";
using namespace std;

GetMediaMsg *getMediaMsg;
GetMediaTimeBase *getMediaTimeBase;
GetMeidaTimeStamp *getMediaTimeStamp;
CopyMeidaFile *copyMeidaFile;
PeelAudioOfMedia *peelAudioOfMedia;
SplitVideoOfMedia *splitVideoOfMedia;
MergeAudio *mergeAudio;
RecodecVideo *recodecVideo;

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
    const string &peelAudioInfo = peelAudioOfMedia->peelAudioOfMedia(cSrcPath, cDestPath);

    env->ReleaseStringUTFChars(srcPath, cSrcPath);
    env->ReleaseStringUTFChars(destPath, cDestPath);

    return env->NewStringUTF(peelAudioInfo.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
cpp_split_video_of_media(JNIEnv *env, jobject thiz, jstring srcPath, jstring destPath) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cDestPath = env->GetStringUTFChars(destPath, nullptr);

    if (splitVideoOfMedia == nullptr) {
        splitVideoOfMedia = new SplitVideoOfMedia();
    }
    const string &splitVideoInfo = splitVideoOfMedia->splitVideoOfMedia(cSrcPath, cDestPath);

    env->ReleaseStringUTFChars(srcPath, cSrcPath);
    env->ReleaseStringUTFChars(destPath, cDestPath);

    return env->NewStringUTF(splitVideoInfo.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
cpp_merge_audio(JNIEnv *env, jobject thiz, jstring srcVideoPath,
                jstring srcAudioPath, jstring destPath) {
    const char *cSrcVidePath = env->GetStringUTFChars(srcVideoPath, nullptr);
    const char *cSrcAudioPath = env->GetStringUTFChars(srcAudioPath, nullptr);
    const char *cDestPath = env->GetStringUTFChars(destPath, nullptr);

    if (mergeAudio == nullptr) {
        mergeAudio = new MergeAudio();
    }
    const string &mergeAudioInfo = mergeAudio->mergeAudio(cSrcVidePath, cSrcAudioPath, cDestPath);

    env->ReleaseStringUTFChars(srcVideoPath, cSrcVidePath);
    env->ReleaseStringUTFChars(srcAudioPath, cSrcAudioPath);
    env->ReleaseStringUTFChars(destPath, cDestPath);

    return env->NewStringUTF(mergeAudioInfo.c_str());
}

extern "C"
JNIEXPORT jstring JNICALL
cpp_recodec_video(JNIEnv *env, jobject thiz, jstring srcVideoPath, jstring destPath) {
    const char *cSrcVidePath = env->GetStringUTFChars(srcVideoPath, nullptr);
    const char *cDestPath = env->GetStringUTFChars(destPath, nullptr);

    if (recodecVideo == nullptr) {
        recodecVideo = new RecodecVideo(env, thiz);
    }
    const string &recodecInfo = recodecVideo->recodecVideo(cSrcVidePath, cDestPath);

    env->ReleaseStringUTFChars(srcVideoPath, cSrcVidePath);
    env->ReleaseStringUTFChars(destPath, cDestPath);

    return env->NewStringUTF(recodecInfo.c_str());
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
        {"native_split_video_of_media", "(Ljava/lang/String;"
                                        "Ljava/lang/String;)Ljava/lang/String;",  (void *) cpp_split_video_of_media},
        {"native_merge_audio",          "(Ljava/lang/String;"
                                        "Ljava/lang/String;"
                                        "Ljava/lang/String;)Ljava/lang/String;",  (void *) cpp_merge_audio},

        {"native_recodec_video",        "(Ljava/lang/String;"
                                        "Ljava/lang/String;)Ljava/lang/String;",  (void *) cpp_recodec_video},
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