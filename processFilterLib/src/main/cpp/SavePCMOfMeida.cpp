//
// Created by wangyao on 2025/9/6.
//

#include "includes/SavePCMOfMeida.h"

SavePCMOfMeida::SavePCMOfMeida(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

SavePCMOfMeida::~SavePCMOfMeida() {
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

void SavePCMOfMeida::startSavePCM(const char *srcPath, const char *destPath) {
    sSrcPath = srcPath;
    sDestPath = destPath;
    processAudioProcedure();
}

void SavePCMOfMeida::processAudioProcedure() {
    AVFormatContext *in_fmt_ctx = NULL; // 输入文件的封装器实例
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, sSrcPath.c_str(), NULL, NULL);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", sSrcPath.c_str());
        av_strerror(ret, errbuf, sizeof(errbuf));
        savePCMInfo = "Can't find stream information. " + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(savePCMInfo.c_str());
        return;
    }
    LOGI("Success open input_file %s.\n", sSrcPath.c_str());
    savePCMInfo = "Success open input_file：" + sSrcPath + "\n";
    PostStatusMessage(savePCMInfo.c_str());
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, NULL);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        savePCMInfo = "Can't find stream information. " + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(savePCMInfo.c_str());
        return;
    }
    AVCodecContext *audio_decode_ctx = nullptr; // 音频解码器的实例
    // 找到音频流的索引
    int audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index >= 0) {
        AVStream *src_audio = in_fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = src_audio->codecpar->codec_id;
        // 查找音频解码器
        AVCodec *audio_codec = (AVCodec *) avcodec_find_decoder(audio_codec_id);
        if (!audio_codec) {
            LOGE("audio_codec not found \n");
            savePCMInfo = "audio_codec not found\n";
            PostStatusMessage(savePCMInfo.c_str());
            return;
        }
        audio_decode_ctx = avcodec_alloc_context3(audio_codec); // 分配解码器的实例
        if (!audio_decode_ctx) {
            LOGE("audio_decode_ctx is null \n");
            savePCMInfo = "audio_decode_ctx is null \n";
            PostStatusMessage(savePCMInfo.c_str());
            return;
        }
        // 把音频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(audio_decode_ctx, src_audio->codecpar);
        // 音频帧的format字段为AVSampleFormat枚举类型，为8时表示AV_SAMPLE_FMT_FLTP
        LOGI("sample_fmt=%d, nb_samples=%d, nb_channels=%d, ",
             audio_decode_ctx->sample_fmt, audio_decode_ctx->frame_size,
             audio_decode_ctx->ch_layout.nb_channels);
        LOGI("format_name=%s, is_planar=%d, data_size=%d\n",
             av_get_sample_fmt_name(audio_decode_ctx->sample_fmt),
             av_sample_fmt_is_planar(audio_decode_ctx->sample_fmt),
             av_get_bytes_per_sample(audio_decode_ctx->sample_fmt));
        ret = avcodec_open2(audio_decode_ctx, audio_codec, nullptr); // 打开解码器的实例
        if (ret < 0) {
            LOGE("Can't open audio_decode_ctx.\n");
            av_strerror(ret, errbuf, sizeof(errbuf));
            savePCMInfo = "Can't open audio_decode_ctx :" + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
            PostStatusMessage(savePCMInfo.c_str());
            return;
        }
    } else {
        LOGE("Can't find audio stream.\n");
        savePCMInfo = "Can't find audio stream. \n";
        PostStatusMessage(savePCMInfo.c_str());
        return;
    }
    FILE *fp_out = fopen(sDestPath.c_str(), "wb"); // 以写方式打开文件
    if (!fp_out) {
        LOGE("open file %s fail.\n", sDestPath.c_str());
        savePCMInfo = "open file %s fail" + sDestPath + "\n";
        PostStatusMessage(savePCMInfo.c_str());
        return;
    }
    LOGI("target audio file is %s\n", sDestPath.c_str());
    savePCMInfo = "target audio file is " + sDestPath + "\n";
    PostStatusMessage(savePCMInfo.c_str());
    int i = 0, j = 0, data_size = 0;
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == audio_index) { // 音频包需要重新编码
            // 把未解压的数据包发给解码器实例
            ret = avcodec_send_packet(audio_decode_ctx, packet);
            if (ret == 0) {
                // 从解码器实例获取还原后的数据帧
                ret = avcodec_receive_frame(audio_decode_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    continue;
                } else if (ret < 0) {
                    LOGE("decode frame occur error %d.\n", ret);
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    savePCMInfo = "decode frame occur error:" + to_string(ret) + "\n error msg：" +
                                  string(errbuf) + "\n";
                    PostStatusMessage(savePCMInfo.c_str());
                    continue;
                }
                // 把音频帧保存到PCM文件
                if (av_sample_fmt_is_planar((enum AVSampleFormat) frame->format)) {
                    // 平面模式的音频在存储时要改为交错模式
                    data_size = av_get_bytes_per_sample((enum AVSampleFormat) frame->format);
                    i = 0;
                    while (i < frame->nb_samples) {
                        j = 0;
                        while (j < frame->ch_layout.nb_channels) {
                            fwrite(frame->data[j] + data_size * i, 1, data_size, fp_out);
                            j++;
                        }
                        i++;
                    }
                } else { // 非平面模式，直接写入文件
                    fwrite(frame->extended_data[0], 1, frame->linesize[0], fp_out);
                }
                av_frame_unref(frame); // 清除数据帧
            } else {
                LOGE("send packet occur error %d.\n", ret);
                av_strerror(ret, errbuf, sizeof(errbuf));
                savePCMInfo = "send packet occur error:" + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
                PostStatusMessage(savePCMInfo.c_str());
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    fclose(fp_out); // 关闭文件
    LOGI("Success save audio frame as pcm file.\n");
    savePCMInfo = "Success save audio frame as pcm file." + sDestPath + "\n";
    PostStatusMessage(savePCMInfo.c_str());

    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(audio_decode_ctx); // 关闭音频解码器的实例
    avcodec_free_context(&audio_decode_ctx); // 释放音频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
}


JNIEnv *SavePCMOfMeida::GetJNIEnv(bool *isAttach) {
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

void SavePCMOfMeida::PostStatusMessage(const char *msg) {
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



