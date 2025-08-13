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
const char *java_class_name = "com/wangyongyao/basictraninglib/FFmpegOperate";
using namespace std;

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
    int ret = avformat_open_input(&fmt_ctx, cFragPath, nullptr, nullptr);

    if (ret < 0) {
        LOGE("Can't open file %s.\n", cFragPath);
        return nullptr;
    }
    LOGI("Success open input_file %s.\n", cFragPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        return nullptr;
    }
    LOGI("Success find stream information.\n");
    const AVInputFormat *iformat = fmt_ctx->iformat;
    int64_t duration = fmt_ctx->duration;
    LOGI("format name is %s.\n", iformat->name);
    LOGI("duration is %d.\n", duration);

    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "打开视频获取信息 : ");
    strcat(strBuffer, "\n format name is : ");
    strcat(strBuffer, iformat->name);

    // 关闭音视频文件
    avformat_close_input(&fmt_ctx);

    LOGD("GetFFmpegVersion\n%s", strBuffer);
    env->ReleaseStringUTFChars(videoPath, cFragPath);
    return env->NewStringUTF(strBuffer);
}

JNIEXPORT jstring JNICALL
cpp_get_media_msg(JNIEnv *env, jobject thiz, jstring videoPath) {

    const char *cFragPath = env->GetStringUTFChars(videoPath, nullptr);
    LOGI("videoPath=== %s", cFragPath);
    AVFormatContext *fmt_ctx = nullptr;
    // 打开音视频文件
    int ret = avformat_open_input(&fmt_ctx, cFragPath, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", cFragPath);
        return nullptr;
    }
    LOGI("Success open input_file %s.\n", cFragPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        return nullptr;
    }
    LOGI("Success find stream information.\n");
    // 格式化输出文件信息
    av_dump_format(fmt_ctx, 0, cFragPath, 0);
    string infoMsg;
    infoMsg = infoMsg + "视频信息 : "
              + "\n duration = " + to_string(fmt_ctx->duration) // 持续时间，单位微秒
              + "\n bit_rate = " + to_string(fmt_ctx->bit_rate) // 比特率，单位比特每秒
              + "\n nb_streams = " + to_string(fmt_ctx->nb_streams) // 数据流的数量
              + "\n max_streams = " + to_string(fmt_ctx->max_streams);  // 数据流的最大数量
    // 找到视频流的索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    LOGI("video_index=%d\n", video_index);
    if (video_index >= 0) {
        AVStream *video_stream = fmt_ctx->streams[video_index];
        infoMsg = infoMsg + "\n video_stream index = " + to_string(video_stream->start_time)
                  + "\n video_stream start_time= " + to_string(video_stream->start_time)
                  + "\n video_stream duration = " + to_string(video_stream->duration);
    }
    // 找到音频流的索引
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    LOGI("audio_index=%d\n", audio_index);
    if (audio_index >= 0) {
        AVStream *audio_stream = fmt_ctx->streams[audio_index];
        infoMsg = infoMsg + "\n audio_stream index = " + to_string(audio_stream->index)
                  + "\n audio_stream start_time = " + to_string(audio_stream->start_time)
                  + "\n audio_stream nb_frames = " + to_string(audio_stream->duration);
    }

    avformat_close_input(&fmt_ctx); // 关闭音视频文件

    env->ReleaseStringUTFChars(videoPath, cFragPath);

    return env->NewStringUTF(infoMsg.c_str());
}

// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        {"native_string_from_jni",    "()Ljava/lang/String;",                   (void *) cpp_string_from_jni},
        {"native_get_ffmpeg_version", "()Ljava/lang/String;",                   (void *) cpp_get_ffmpeg_version},
        {"native_get_video_msg",      "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_video_msg},
        {"native_get_media_msg",      "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_media_msg},


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