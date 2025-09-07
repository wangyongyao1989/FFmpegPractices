//
// Created by wangyao on 2025/9/6.
//

#include <sys/stat.h>
#include "includes/SaveWavOfMedia.h"

SaveWavOfMedia::SaveWavOfMedia(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

SaveWavOfMedia::~SaveWavOfMedia() {
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

    std::destroy(sSrcPath1.begin(), sSrcPath1.end());
    std::destroy(sDestPath1.begin(), sDestPath1.end());
    std::destroy(sDestPath2.begin(), sDestPath2.end());

    if (audio_decode_ctx)
        avcodec_close(audio_decode_ctx); // 关闭音频解码器的实例
    if (audio_decode_ctx)
        avcodec_free_context(&audio_decode_ctx); // 释放音频解码器的实例
    if (audio_encode_ctx)
        avcodec_close(audio_encode_ctx); // 关闭音频编码器的实例
    if (audio_encode_ctx)
        avcodec_free_context(&audio_encode_ctx); // 释放音频编码器的实例
    if (in_fmt_ctx)
        avformat_close_input(&in_fmt_ctx); // 关闭音视频文件


}

void
SaveWavOfMedia::startSaveWav(const char *srcPath1, const char *destPath1, const char *destPath2) {
    sSrcPath1 = srcPath1;
    sDestPath1 = destPath1;
    sDestPath2 = destPath2;
    processAudioProcedure();

}


void SaveWavOfMedia::processAudioProcedure() {
    AVFormatContext *in_fmt_ctx = NULL; // 输入文件的封装器实例
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, sSrcPath1.c_str(), nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", sSrcPath1.c_str());
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveWavInfo = "Can't open file " + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveWavInfo.c_str());
        return;
    }
    LOGI("Success open input_file %s.\n", sSrcPath1.c_str());
    saveWavInfo = "Success open input_file：" + sSrcPath1 + "\n";
    PostStatusMessage(saveWavInfo.c_str());
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveWavInfo = "Can't open file " + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveWavInfo.c_str());
        return;
    }
    // 找到音频流的索引
    int audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index >= 0) {
        AVStream *src_audio = in_fmt_ctx->streams[audio_index];
        enum AVCodecID audio_codec_id = src_audio->codecpar->codec_id;
        // 查找音频解码器
        AVCodec *audio_codec = (AVCodec *) avcodec_find_decoder(audio_codec_id);
        if (!audio_codec) {
            LOGE("audio_codec not found\n");
            saveWavInfo = "audio_codec not found \n";
            PostStatusMessage(saveWavInfo.c_str());
            return;
        }
        audio_decode_ctx = avcodec_alloc_context3(audio_codec); // 分配解码器的实例
        if (!audio_decode_ctx) {
            LOGE("audio_decode_ctx is null\n");
            saveWavInfo = "audio_decode_ctx is null\n";
            PostStatusMessage(saveWavInfo.c_str());
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
            saveWavInfo = "Can't open file " + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
            PostStatusMessage(saveWavInfo.c_str());
            return;
        }
    } else {
        LOGE("Can't find audio stream.\n");
        saveWavInfo = "Can't find audio stream. \n";
        PostStatusMessage(saveWavInfo.c_str());
        return;
    }
    FILE *fp_out = fopen(sDestPath1.c_str(), "wb"); // 以写方式打开文件
    if (!fp_out) {
        LOGE("open file %s fail.\n", sDestPath1.c_str());
        saveWavInfo = "open file %s fail." + sDestPath1 + "\n";
        PostStatusMessage(saveWavInfo.c_str());
        return;
    }
    LOGI("target audio file is %s\n", sDestPath1.c_str());
    saveWavInfo = "target audio file is  " + sDestPath1 + "\n";
    PostStatusMessage(saveWavInfo.c_str());

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
                    saveWavInfo = "Can't open file " + to_string(ret) + "\n error msg：" +
                                  string(errbuf) + "\n";
                    PostStatusMessage(saveWavInfo.c_str());
                    continue;
                }
                // 把音频帧保存为PCM音频
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
                saveWavInfo = "Can't open file " + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
                PostStatusMessage(saveWavInfo.c_str());
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    fclose(fp_out); // 关闭文件
    LOGI("Success save audio frame as pcm file.\n");
    saveWavInfo = "Success save audio frame as pcm file.  \n";
    PostStatusMessage(saveWavInfo.c_str());

    save_wav_file(sDestPath1.c_str()); // 把PCM文件转换为WAV文件
    LOGI("Success save audio frame as wav file.\n");
    saveWavInfo = "Success save audio frame as wav file.\n";
    PostStatusMessage(saveWavInfo.c_str());

    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(audio_decode_ctx); // 关闭音频解码器的实例
    avcodec_free_context(&audio_decode_ctx); // 释放音频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
}

// 把PCM文件转换为WAV文件
int SaveWavOfMedia::save_wav_file(const char *pcm_name) {
    struct stat size; // 保存文件信息的结构
    if (stat(pcm_name, &size) != 0) { // 获取文件信息
        LOGE("file %s is not exists\n", pcm_name);
        saveWavInfo = "file is not exists:  " + string(pcm_name) + "\n";
        PostStatusMessage(saveWavInfo.c_str());
        return -1;
    }
    FILE *fp_pcm = fopen(pcm_name, "rb"); // 以读方式打开pcm文件
    if (!fp_pcm) {
        LOGE("open file %s fail.\n", pcm_name);
        saveWavInfo = "open file fail :" + string(pcm_name) + "\n";
        PostStatusMessage(saveWavInfo.c_str());
        return -1;
    }
    FILE *fp_wav = fopen(sDestPath2.c_str(), "wb"); // 以写方式打开wav文件
    if (!fp_wav) {
        LOGE("open file %s fail.\n", sDestPath2.c_str());
        saveWavInfo = "open file %s fail." + string(pcm_name) + "\n";
        PostStatusMessage(saveWavInfo.c_str());
        return -1;
    }
    LOGI("target audio file is %s\n", sDestPath2.c_str());
    saveWavInfo = "target audio file is " + string(sDestPath2.c_str()) + "\n";
    PostStatusMessage(saveWavInfo.c_str());
    int pcmDataSize = size.st_size; // pcm文件大小
    LOGI("pcmDataSize=%d\n", pcmDataSize);
    saveWavInfo = "pcmDataSize=" + to_string(pcmDataSize) + "\n";
    PostStatusMessage(saveWavInfo.c_str());

    WAVHeader wavHeader; // wav文件头结构
    // 设置 RIFF chunk size，RIFF chunk size 不包含 RIFF Chunk ID 和 RIFF Chunk Size的大小，所以用 PCM 数据大小加 RIFF 头信息大小减去 RIFF Chunk ID 和 RIFF Chunk Size的大小
    wavHeader.riffCkSize = (pcmDataSize + sizeof(WAVHeader) - 4 - 4);
    wavHeader.fmtCkSize = 16;
    // 设置音频格式。1为整数，3为浮点数（含双精度数）
    if (audio_decode_ctx->sample_fmt == AV_SAMPLE_FMT_FLTP
        || audio_decode_ctx->sample_fmt == AV_SAMPLE_FMT_FLT
        || audio_decode_ctx->sample_fmt == AV_SAMPLE_FMT_DBLP
        || audio_decode_ctx->sample_fmt == AV_SAMPLE_FMT_DBL) {
        wavHeader.audioFormat = 3;
    } else {
        wavHeader.audioFormat = 1;
    }
    wavHeader.channels = audio_decode_ctx->ch_layout.nb_channels; // 声道数量
    wavHeader.sampleRate = audio_decode_ctx->sample_rate; // 采样频率
    wavHeader.bitsPerSample = 8 * av_get_bytes_per_sample(audio_decode_ctx->sample_fmt);
    wavHeader.blockAlign = (wavHeader.channels * wavHeader.bitsPerSample) >> 3;
//    wavHeader.blockAlign = (wavHeader.channels * wavHeader.bitsPerSample) / 8;
    wavHeader.byteRate = wavHeader.sampleRate * wavHeader.blockAlign;
    // 设置数据块大小，即实际PCM数据的长度，单位字节
    wavHeader.dataCkSize = pcmDataSize;
    // 向wav文件写入wav文件头信息
    fwrite((const char *) &wavHeader, 1, sizeof(WAVHeader), fp_wav);
    const int per_size = 1024; // 每次读取的大小
    uint8_t *per_buff = (uint8_t *) av_malloc(per_size); // 读取缓冲区
    int len = 0;
    // 循环读取PCM文件中的音频数据
    while ((len = fread(per_buff, 1, per_size, fp_pcm)) > 0) {
        fwrite(per_buff, 1, per_size, fp_wav); // 依次写入每个PCM数据
    }
    fclose(fp_pcm); // 关闭pcm文件
    fclose(fp_wav); // 关闭wav文件
    return 0;
}


JNIEnv *SaveWavOfMedia::GetJNIEnv(bool *isAttach) {
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

void SaveWavOfMedia::PostStatusMessage(const char *msg) {
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