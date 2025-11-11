//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/10.
//

#include "includes/PlayAudioTrack.h"


PlayAudioTrack::PlayAudioTrack(JNIEnv *env, jobject thiz) {
    LOGE("PlayAudioTrack::PlayAudioTrack");
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);

}

PlayAudioTrack::~PlayAudioTrack() {
    mEnv->DeleteGlobalRef(mJavaObj);
    if (mEnv) {
        mEnv = nullptr;
    }

    if (mJavaVm) {
        mJavaVm = nullptr;
    }

    if (mJavaObj) {
        mJavaObj = nullptr;
    }

}

void
PlayAudioTrack::startPlayAudioTrack(const char *srcPath) {
    sAudioPath = srcPath;
    playAudioProcedure();
}

void
PlayAudioTrack::stopPlayAudioTrack() {
    is_stop = 1;
}

void
PlayAudioTrack::playAudioProcedure() {
    LOGE("PlayAudio: %s", sAudioPath.c_str());
    AVFormatContext *fmt_ctx = avformat_alloc_context(); //分配封装器实例
    LOGI("Open audio file");
    // 打开音视频文件
    if (avformat_open_input(&fmt_ctx, sAudioPath.c_str(), NULL, NULL) != 0) {
        LOGE("Cannot open audio file: %s\n", sAudioPath.c_str());
        playAudioInfo = "Cannot open audio file： " + sAudioPath + "\n";
        PostStatusMessage(playAudioInfo.c_str());
        return;
    }

    playAudioInfo = "Open audio file: " + sAudioPath + "\n";
    PostStatusMessage(playAudioInfo.c_str());

    // 查找音视频文件中的流信息
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        LOGE("Cannot find stream information.");
        playAudioInfo = "Cannot find stream information  \n";
        PostStatusMessage(playAudioInfo.c_str());
        return;
    }

    // 找到音频流的索引
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index == -1) {
        LOGE("No audio stream found.");
        playAudioInfo = "No audio stream found. \n";
        PostStatusMessage(playAudioInfo.c_str());
        return;
    }

    AVCodecParameters *codec_para = fmt_ctx->streams[audio_index]->codecpar;
    LOGI("Find the decoder for the audio stream");
    // 查找音频解码器
    AVCodec *codec = (AVCodec *) avcodec_find_decoder(codec_para->codec_id);
    if (codec == NULL) {
        LOGE("Codec not found.");
        playAudioInfo = "Codec not found.\n";
        PostStatusMessage(playAudioInfo.c_str());
        return;
    }

    // 分配解码器的实例
    AVCodecContext *decode_ctx = avcodec_alloc_context3(codec);
    if (decode_ctx == NULL) {
        LOGE("CodecContext not found.");
        playAudioInfo = "CodecContext not found. \n";
        PostStatusMessage(playAudioInfo.c_str());
        return;
    }

    // 把音频流中的编解码参数复制给解码器的实例
    if (avcodec_parameters_to_context(decode_ctx, codec_para) < 0) {
        LOGE("Fill CodecContext failed.");
        playAudioInfo = "Fill CodecContext failed. \n";
        PostStatusMessage(playAudioInfo.c_str());
        return;
    }
    LOGE("decode_ctx bit_rate=%d", decode_ctx->bit_rate);
    LOGE("decode_ctx sample_fmt=%d", decode_ctx->sample_fmt);
    LOGE("decode_ctx sample_rate=%d", decode_ctx->sample_rate);
    LOGE("decode_ctx nb_channels=%d", decode_ctx->channels);

    LOGI("open Codec");
    playAudioInfo = "open Codec \n";
    PostStatusMessage(playAudioInfo.c_str());
    // 打开解码器的实例
    if (avcodec_open2(decode_ctx, codec, NULL)) {
        LOGE("Open CodecContext failed.");
        playAudioInfo = "Open CodecContext failed \n";
        PostStatusMessage(playAudioInfo.c_str());
        return;
    }

    SwrContext *swr_ctx = NULL; // 音频采样器的实例
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // 输出的声道布局
    swr_alloc_set_opts2(&swr_ctx, &out_ch_layout, AV_SAMPLE_FMT_S16,
                        decode_ctx->sample_rate,
                        &decode_ctx->ch_layout, decode_ctx->sample_fmt,
                        decode_ctx->sample_rate, 0, NULL
    );
    LOGE("swr_init");
    playAudioInfo = " swr_init..... \n";
    PostStatusMessage(playAudioInfo.c_str());
    swr_init(swr_ctx); // 初始化音频采样器的实例

    // 原音频的通道数
    int channel_count = decode_ctx->ch_layout.nb_channels;
    // 单通道最大存放转码数据。所占字节 = 采样率*量化格式 / 8
    int out_size = 44100 * 16 / 8;
    uint8_t *out = (uint8_t *) (av_malloc(out_size));
    //回调AudioFormat参数给Java层
    JavaAudioFormatCallback(decode_ctx->sample_rate, out_ch_layout.nb_channels);

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(fmt_ctx, packet) == 0) { // 轮询数据包
        if (packet->stream_index == audio_index) { // 音频包需要解码
            // 把未解压的数据包发给解码器实例
            int ret = avcodec_send_packet(decode_ctx, packet);
            if (ret == 0) {
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(decode_ctx, frame);
                if (ret == 0) {
                    // 重采样。也就是把输入的音频数据根据指定的采样规格转换为新的音频数据输出
                    swr_convert(swr_ctx, &out, out_size,
                                (const uint8_t **) (frame->data), frame->nb_samples);
                    // 获取采样缓冲区的真实大小
                    int size = av_samples_get_buffer_size(NULL, channel_count,
                                                          frame->nb_samples,
                                                          AV_SAMPLE_FMT_S16, 1);
                    LOGE("out_size=%d, size=%d", out_size, size);
                    playAudioInfo = "回调的音频数据：size= " + to_string(size) + "\n";
                    PostStatusMessage(playAudioInfo.c_str());
                    //回调 out_audio_data 音频数据给Java层
                    JavaAudioBytesCallback(out, size);
                    if (is_stop) { // 是否停止播放
                        break;
                    }
                }
            }
        }
//        av_packet_free(&packet); // 这句会导致程序挂掉
    }
    LOGI("release memory");
    // 释放各类资源
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    swr_free(&swr_ctx); // 释放音频采样器的实例
    avcodec_close(decode_ctx); // 关闭音频解码器的实例
    avcodec_free_context(&decode_ctx); // 释放音频解码器的实例
    avformat_free_context(fmt_ctx); // 关闭音视频文件

}



JNIEnv *PlayAudioTrack::GetJNIEnv(bool *isAttach) {
    JNIEnv *env;
    int status;
    if (nullptr == mJavaVm) {
        LOGD("SaveYUVFromVideo::GetJNIEnv mJavaVm == nullptr");
        return nullptr;
    }
    *isAttach = false;
    status = mJavaVm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status != JNI_OK) {
        status = mJavaVm->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGD("SaveYUVFromVideo::GetJNIEnv failed to attach current thread");
            return nullptr;
        }
        *isAttach = true;
    }
    return env;
}

void PlayAudioTrack::PostStatusMessage(const char *msg) {
    bool isAttach = false;
    JNIEnv *pEnv = GetJNIEnv(&isAttach);
    if (pEnv == nullptr) {
        return;
    }
    jobject javaObj = mJavaObj;
    jmethodID mid = pEnv->GetMethodID(pEnv->GetObjectClass(javaObj), "CppStatusCallback",
                                      "(Ljava/lang/String;)V");
    jstring pJstring = pEnv->NewStringUTF(msg);
    pEnv->CallVoidMethod(javaObj, mid, pJstring);
    if (isAttach) {
        JavaVM *pJavaVm = mJavaVm;
        pJavaVm->DetachCurrentThread();
    }
}

void PlayAudioTrack::JavaAudioFormatCallback(int sample_rate, int nb_channels) {
    bool isAttach = false;
    JNIEnv *pEnv = GetJNIEnv(&isAttach);
    if (pEnv == nullptr) {
        return;
    }
    jobject javaObj = mJavaObj;
    //回调到PlayAudioOperate.java中的cppAudioFormatCallback方法
    jmethodID mid = pEnv->GetMethodID(pEnv->GetObjectClass(javaObj), "cppAudioFormatCallback",
                                      "(II)V");
    pEnv->CallVoidMethod(javaObj, mid, sample_rate, nb_channels);
    if (isAttach) {
        JavaVM *pJavaVm = mJavaVm;
        pJavaVm->DetachCurrentThread();
    }
}

void PlayAudioTrack::JavaAudioBytesCallback(uint8_t *out_audio_data, int size) {
    bool isAttach = false;
    JNIEnv *pEnv = GetJNIEnv(&isAttach);
    if (pEnv == nullptr) {
        return;
    }
    jobject javaObj = mJavaObj;
    //回调到PlayAudioOperate.java中的 cppAudioBytesCallback 方法
    jmethodID mid = pEnv->GetMethodID(pEnv->GetObjectClass(javaObj), "cppAudioBytesCallback",
                                      "([BI)V");
    // 分配指定大小的Java字节数组
    jbyteArray array = pEnv->NewByteArray(size);
    // 把音频缓冲区的数据复制到ava字节数组
    pEnv->SetByteArrayRegion(array, 0, size, (const jbyte *) (out_audio_data));
    pEnv->CallVoidMethod(javaObj, mid, array, size);
    // 回收指定的Java字节数组
    pEnv->DeleteLocalRef(array);
    if (isAttach) {
        JavaVM *pJavaVm = mJavaVm;
        pJavaVm->DetachCurrentThread();
    }
}