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

/**
 * 练习一：打开关闭一个音视频流，并获取其内部的信息
 */
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

/**
 *  练习二：打开一个音视频文件后，打印出视频及音频相关信息
 */
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

/**
 * 练习三：打开一个音视频文件后，获取视频及音频的解码ID及解码器名字。
 */
JNIEXPORT jstring JNICALL
cpp_get_media_codec_msg(JNIEnv *env, jobject thiz, jstring videoPath) {

    const char *cFragPath = env->GetStringUTFChars(videoPath, nullptr);
    LOGI("videoPath=== %s", cFragPath);
    AVFormatContext *fmt_ctx = nullptr;
    // 打开音视频文件
    int ret = avformat_open_input(&fmt_ctx, cFragPath, nullptr, nullptr);

    env->ReleaseStringUTFChars(videoPath, cFragPath);
    string infoMsg;

    if (ret < 0) {
        LOGE("Can't open file %s.\n", cFragPath);
        infoMsg = "Can't open file";
        return env->NewStringUTF(infoMsg.c_str());
    }
    LOGI("Success open input_file %s.\n", cFragPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        infoMsg = "Can't find stream information.";
        return env->NewStringUTF(infoMsg.c_str());
    }
    LOGI("Success find stream information.\n");
    // 格式化输出文件信息
    av_dump_format(fmt_ctx, 0, cFragPath, 0);
    // 找到视频流的索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    LOGI("video_index=%d\n", video_index);
    if (video_index >= 0) {
        AVStream *video_stream = fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        AVCodec *video_codec = (AVCodec *) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            LOGE("vide_codec not find.\n");
            infoMsg = "vide_codec not find";
            return env->NewStringUTF(infoMsg.c_str());
        }
        infoMsg = infoMsg + "视频解码信息 : " + "\n video codec id= " + to_string(video_codec->id)
                  + "\n video codec name= " + video_codec->name
                  + "\n video codec type = " + to_string(video_codec->type);
    }

    // 找到音频流的索引
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    LOGI("audio_index=%d\n", audio_index);

    if (audio_index >= 0) {
        AVStream *audio_stream = fmt_ctx->streams[audio_index];
        //查找音频解码器
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;

        const AVCodec *audio_codec = (AVCodec *) avcodec_find_decoder(audio_codec_id);
        if (!audio_codec) {
            LOGE("audio_codec not find.\n");
            infoMsg = "audio_codec not find";
            return env->NewStringUTF(infoMsg.c_str());
        }
//        LOGI("audio_codec long_name =%c\n", to_string(audio_codec->type));

        infoMsg = infoMsg + "\n 音频解码器" + "\n audio_codec id = " + to_string(audio_codec->id)
                  + "\n audio_codec name = " + audio_codec->name
                  + "\n audio_codec type = " + to_string(audio_codec->type);
    }

    avformat_close_input(&fmt_ctx); // 关闭音视频文件


    return env->NewStringUTF(infoMsg.c_str());
}

JNIEXPORT jstring JNICALL
cpp_media_codec_copy_new_to_codec(JNIEnv *env, jobject thiz, jstring videoPath) {

    const char *cFragPath = env->GetStringUTFChars(videoPath, nullptr);
    LOGI("videoPath=== %s", cFragPath);
    AVFormatContext *fmt_ctx = nullptr;
    // 打开音视频文件
    int ret = avformat_open_input(&fmt_ctx, cFragPath, nullptr, nullptr);

    env->ReleaseStringUTFChars(videoPath, cFragPath);
    string infoMsg;

    if (ret < 0) {
        LOGE("Can't open file %s.\n", cFragPath);
        infoMsg = "Can't open file";
        return env->NewStringUTF(infoMsg.c_str());
    }
    LOGI("Success open input_file %s.\n", cFragPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        infoMsg = "Can't find stream information.";
        return env->NewStringUTF(infoMsg.c_str());
    }
    LOGI("Success find stream information.\n");
    // 格式化输出文件信息
    av_dump_format(fmt_ctx, 0, cFragPath, 0);
    // 找到视频流的索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    LOGI("video_index=%d\n", video_index);
    if (video_index >= 0) {
        AVStream *video_stream = fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        AVCodec *video_codec = (AVCodec *) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            LOGE("vide_codec not find.\n");
            infoMsg = "vide_codec not find";
            return env->NewStringUTF(infoMsg.c_str());
        }
        AVCodecContext *video_decode_ctx = nullptr; // 视频解码器的实例
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            LOGE("video_decode_ctx is null.\n");
            infoMsg = "video_decode_ctx is null";
            return env->NewStringUTF(infoMsg.c_str());
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, video_stream->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr); // 打开解码器的实例
        LOGI("Success open video codec.\n");
        if (ret < 0) {
            LOGE("Can't open video_decode_ctx.\n");
            infoMsg = "Can't open video_decode_ctx.";
            return env->NewStringUTF(infoMsg.c_str());
        }

        infoMsg = infoMsg + "视频解码器实例参数 : " + "\n Success copy video parameters_to_context "
                  + "\n video_decode_ctx width= " + to_string(video_decode_ctx->width)
                  + "\n video_decode_ctx height = " + to_string(video_decode_ctx->height)
                  + "\n 双向预测帧（B帧）的最大数量 = " + to_string(video_decode_ctx->max_b_frames)
                  + "\n 视频的像素格式 = " + to_string(video_decode_ctx->pix_fmt)
                  + "\n 每两个关键帧（I帧）间隔多少帧 = " + to_string(video_decode_ctx->gop_size)
                  + "\n video_decode_ctx profile = " + to_string(video_decode_ctx->profile);

        avcodec_close(video_decode_ctx); // 关闭解码器的实例
        avcodec_free_context(&video_decode_ctx); // 释放解码器的实例
    }

    // 找到音频流的索引
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    LOGI("audio_index=%d\n", audio_index);

    if (audio_index >= 0) {
        AVStream *audio_stream = fmt_ctx->streams[audio_index];
        //查找音频解码器
        enum AVCodecID audio_codec_id = audio_stream->codecpar->codec_id;

        const AVCodec *audio_codec = (AVCodec *) avcodec_find_decoder(audio_codec_id);
        if (!audio_codec) {
            LOGE("audio_codec not find.\n");
            infoMsg = "audio_codec not find";
            return env->NewStringUTF(infoMsg.c_str());
        }
        AVCodecContext *audio_decode_ctx = nullptr; // 音频解码器的实例
        audio_decode_ctx = avcodec_alloc_context3(audio_codec); // 分配解码器的实例
        if (!audio_decode_ctx) {
            LOGE("audio_decode_ctx is null\n");
            infoMsg = "audio_decode_ctx is null.";
            return env->NewStringUTF(infoMsg.c_str());
        }
        // 把音频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(audio_decode_ctx, audio_stream->codecpar);
        ret = avcodec_open2(audio_decode_ctx, audio_codec, nullptr); // 打开解码器的实例
        LOGI("Success open audio codec.\n");
        if (ret < 0) {
            LOGE("audio_decode_ctx is null.\n");
            infoMsg = "audio_decode_ctx is null.";
            return env->NewStringUTF(infoMsg.c_str());
        }

        infoMsg =
                infoMsg + "\n 音频解码器实例参数:" + "\n Success copy audio parameters_to_context. "
                + "\n audio_decode_ctx profile = " + to_string(audio_decode_ctx->profile)
                + "\n 音频的采样格式 = " + to_string(audio_decode_ctx->sample_fmt)
                + "\n 音频的采样频率 = " + to_string(audio_decode_ctx->sample_rate)
                + "\n 音频的帧大小 = " + to_string(audio_decode_ctx->frame_size)
                + "\n 音频的码率 = " + to_string(audio_decode_ctx->bit_rate)
                + "\n 音视频的时间基num = " + to_string(audio_decode_ctx->time_base.num)
                + "\n 音视频的时间基den = " + to_string(audio_decode_ctx->time_base.den)
                + "\n 音频的声道布局 = " + to_string(audio_decode_ctx->ch_layout.nb_channels);

        avcodec_close(audio_decode_ctx); // 关闭解码器的实例
        avcodec_free_context(&audio_decode_ctx); // 释放解码器的实例
    }

    avformat_close_input(&fmt_ctx); // 关闭音视频文件


    return env->NewStringUTF(infoMsg.c_str());
}

// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        {"native_string_from_jni",       "()Ljava/lang/String;",                   (void *) cpp_string_from_jni},
        {"native_get_ffmpeg_version",    "()Ljava/lang/String;",                   (void *) cpp_get_ffmpeg_version},
        {"native_get_video_msg",         "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_video_msg},
        {"native_get_media_msg",         "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_media_msg},
        {"native_get_media_codec_msg",   "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_media_codec_msg},
        {"native_media_copy_to_decodec", "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_media_codec_copy_new_to_codec},


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