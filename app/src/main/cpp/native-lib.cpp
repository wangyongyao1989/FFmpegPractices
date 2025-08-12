#include <jni.h>
#include <string>
#include "includes/LogUtils.h"

// 因为FFmpeg源码使用C语言编写，所以在C++代码中调用FFmpeg的话，
// 要通过标记“extern "C"{……}”把FFmpeg的头文件包含进来
extern "C"
{
#include "libavutil/common.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include <libavutil/avutil.h>

}

//包名+类名字符串定义：
const char *java_class_name = "com/wangyao/ffmpegpractice/ffmpegpractice/FFmpegOperate";


extern "C"
JNIEXPORT jstring JNICALL
cpp_string_from_jni(JNIEnv *env, jobject thiz) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}


JNIEXPORT jstring JNICALL
cpp_get_ffmpeg_version(JNIEnv *env, jobject thiz) {
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

JNIEXPORT jstring JNICALL
cpp_get_video_msg(JNIEnv *env, jobject thiz, jstring videoPath) {

    const char *cFragPath = env->GetStringUTFChars(videoPath, nullptr);
    LOGI("videoPath=== %s", cFragPath);
    AVFormatContext *fmt_ctx = avformat_alloc_context();

    // 打开音视频文件
    int ret = avformat_open_input(&fmt_ctx, cFragPath, NULL, NULL);

    if (ret < 0) {
        LOGE("Can't open file %s.\n", cFragPath);
        return NULL;
    }
    LOGI("Success open input_file %s.\n", cFragPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        return NULL;
    }
    LOGI("Success find stream information.\n");
    const AVInputFormat *iformat = fmt_ctx->iformat;
    int64_t duration = fmt_ctx->duration;
    LOGI("format name is %s.\n", iformat->name);
    LOGI("duration is %d.\n", duration);

    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "视频信息 : ");
    strcat(strBuffer, "\n format name is : ");
    strcat(strBuffer, iformat->name);

    // 关闭音视频文件
    avformat_close_input(&fmt_ctx);

    LOGD("GetFFmpegVersion\n%s", strBuffer);
    env->ReleaseStringUTFChars(videoPath, cFragPath);
    return env->NewStringUTF(strBuffer);
}

// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        {"native_string_from_jni",    "()Ljava/lang/String;",                   (void *) cpp_string_from_jni},
        {"native_get_ffmpeg_version", "()Ljava/lang/String;",                   (void *) cpp_get_ffmpeg_version},
        {"native_get_video_msg",      "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_video_msg},

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