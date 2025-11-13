// Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/11.
//

#include "includes/PlayOpenSL.h"

PlayOpenSL::PlayOpenSL(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

PlayOpenSL::~PlayOpenSL() {
    stopPlayOpenSL();
    // 等待播放线程结束
    if (is_playing) {
        unique_lock<mutex> lock(audioMutex);
        audioCondition.wait_for(lock, chrono::seconds(2), [this]() { return !is_playing; });
    }

    mEnv->DeleteGlobalRef(mJavaObj);
    release();
}

void PlayOpenSL::startPlayOpenSL(const char *srcPath) {
    if (is_playing) {
        LOGE("Already playing");
        return;
    }

    sAudioPath = srcPath;
    is_stop = false;
    is_playing = true;

    // 在新线程中播放
    thread playThread(&PlayOpenSL::playAudioProcedure, this);
    playThread.detach();
}

void PlayOpenSL::stopPlayOpenSL() {
    is_stop = true;
    {
        unique_lock<mutex> lock(audioMutex);
        audioCondition.notify_all();
    }
}

void PlayOpenSL::playAudioProcedure() {
    LOGE("PlayAudio: %s", sAudioPath.c_str());

    // 初始化FFmpeg
    mFmtCtx = avformat_alloc_context();
    if (!mFmtCtx) {
        LOGE("Cannot allocate format context");
        PostStatusMessage("Cannot allocate format context\n");
        is_playing = false;
        return;
    }

    // 打开音频文件
    if (avformat_open_input(&mFmtCtx, sAudioPath.c_str(), NULL, NULL) != 0) {
        LOGE("Cannot open audio file: %s", sAudioPath.c_str());
        PostStatusMessage(("Cannot open audio file: " + sAudioPath + "\n").c_str());
        is_playing = false;
        return;
    }

    PostStatusMessage(("Open audio file: " + sAudioPath + "\n").c_str());

    // 查找流信息
    if (avformat_find_stream_info(mFmtCtx, NULL) < 0) {
        LOGE("Cannot find stream information");
        PostStatusMessage("Cannot find stream information\n");
        is_playing = false;
        return;
    }

    // 找到音频流
    audio_index = av_find_best_stream(mFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index == -1) {
        LOGE("No audio stream found");
        PostStatusMessage("No audio stream found\n");
        is_playing = false;
        return;
    }

    AVCodecParameters *codec_para = mFmtCtx->streams[audio_index]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codec_para->codec_id);
    if (!codec) {
        LOGE("Codec not found");
        PostStatusMessage("Codec not found\n");
        is_playing = false;
        return;
    }

    // 分配解码器上下文
    mDecodeCtx = avcodec_alloc_context3(codec);
    if (!mDecodeCtx) {
        LOGE("CodecContext not found");
        PostStatusMessage("CodecContext not found\n");
        is_playing = false;
        return;
    }

    // 复制编解码参数
    if (avcodec_parameters_to_context(mDecodeCtx, codec_para) < 0) {
        LOGE("Fill CodecContext failed");
        PostStatusMessage("Fill CodecContext failed\n");
        is_playing = false;
        return;
    }

    // 打开解码器
    if (avcodec_open2(mDecodeCtx, codec, NULL) != 0) {
        LOGE("Open CodecContext failed");
        PostStatusMessage("Open CodecContext failed\n");
        is_playing = false;
        return;
    }

    // 配置音频重采样
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    channel_count = mDecodeCtx->ch_layout.nb_channels;

    swr_alloc_set_opts2(&mSwrCtx,
                        &out_ch_layout, AV_SAMPLE_FMT_S16, mDecodeCtx->sample_rate,
                        &mDecodeCtx->ch_layout, mDecodeCtx->sample_fmt, mDecodeCtx->sample_rate,
                        0, NULL);

    if (!mSwrCtx || swr_init(mSwrCtx) < 0) {
        LOGE("Swr init failed");
        PostStatusMessage("Swr init failed\n");
        is_playing = false;
        return;
    }

    // 分配缓冲区
    out_buffer_size = 44100 * 2 * 2; // 44100Hz * 2 channels * 2 bytes per sample
    outBuffer = (uint8_t *) av_malloc(out_buffer_size);
    if (!outBuffer) {
        LOGE("Cannot allocate output buffer");
        PostStatusMessage("Cannot allocate output buffer\n");
        is_playing = false;
        return;
    }

    mPacket = av_packet_alloc();
    mFrame = av_frame_alloc();
    if (!mPacket || !mFrame) {
        LOGE("Cannot allocate packet or frame");
        PostStatusMessage("Cannot allocate packet or frame\n");
        is_playing = false;
        return;
    }

    // 初始化OpenSL ES
    SLresult result = helper.createEngine();
    if (!helper.isSuccess(result)) {
        LOGE("create engine error: %d", result);
        PostStatusMessage("Create engine error\n");
        is_playing = false;
        return;
    }

    result = helper.createMix();
    if (!helper.isSuccess(result)) {
        LOGE("create mix error: %d", result);
        PostStatusMessage("Create mix error\n");
        is_playing = false;
        return;
    }

    result = helper.createPlayer(2, SL_SAMPLINGRATE_44_1, SL_PCMSAMPLEFORMAT_FIXED_16,
                                 SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT);
    if (!helper.isSuccess(result)) {
        LOGE("create player error: %d", result);
        PostStatusMessage("Create player error\n");
        is_playing = false;
        return;
    }

    // 注册回调并开始播放
    result = helper.registerCallback(playerCallback);
    if (!helper.isSuccess(result)) {
        LOGE("register callback error: %d", result);
        PostStatusMessage("Register callback error\n");
        is_playing = false;
        return;
    }

    // 设置上下文，便于回调访问
    result = (*helper.bufferQueueItf)->Enqueue(helper.bufferQueueItf, outBuffer, 0);
    if (!helper.isSuccess(result)) {
        LOGE("first enqueue error: %d", result);
    }

    result = helper.play();
    if (!helper.isSuccess(result)) {
        LOGE("play error: %d", result);
        PostStatusMessage("Play error\n");
        is_playing = false;
        return;
    }

    PostStatusMessage("Start playing audio\n");

    // 等待播放结束
    while (!is_stop && !is_finished) {
        this_thread::sleep_for(chrono::milliseconds(100));
    }

    // 停止播放
    helper.stop();
    is_playing = false;
    audioCondition.notify_all();

    PostStatusMessage("Audio playback finished\n");
}

void PlayOpenSL::release() {
    LOGI("release memory");

    if (mFrame) {
        av_frame_free(&mFrame);
        mFrame = nullptr;
    }
    if (mPacket) {
        av_packet_free(&mPacket);
        mPacket = nullptr;
    }
    if (outBuffer) {
        av_free(outBuffer);
        outBuffer = nullptr;
    }
    if (mSwrCtx) {
        swr_free(&mSwrCtx);
        mSwrCtx = nullptr;
    }
    if (mDecodeCtx) {
        avcodec_close(mDecodeCtx);
        avcodec_free_context(&mDecodeCtx);
        mDecodeCtx = nullptr;
    }
    if (mFmtCtx) {
        avformat_close_input(&mFmtCtx);
        avformat_free_context(mFmtCtx);
        mFmtCtx = nullptr;
    }

    if (mJavaObj) {
        bool isAttach = false;
        JNIEnv *env = GetJNIEnv(&isAttach);
        if (env) {
            env->DeleteGlobalRef(mJavaObj);
            mJavaObj = nullptr;
        }
        if (isAttach && mJavaVm) {
            mJavaVm->DetachCurrentThread();
        }
    }
}

bool PlayOpenSL::decodeAudioFrame() {
    SLresult result;
    SLuint32 buff_size;
    while (!is_stop) {
        int ret = av_read_frame(mFmtCtx, mPacket);
        if (ret < 0) {
            is_finished = true;
            return false;
        }

        if (mPacket->stream_index == audio_index) {
            ret = avcodec_send_packet(mDecodeCtx, mPacket);
            av_packet_unref(mPacket); // 使用unref而不是free

            if (ret == 0) {
                ret = avcodec_receive_frame(mDecodeCtx, mFrame);
                if (ret == 0) {
                    // 重采样
                    int samples = swr_convert(mSwrCtx, &outBuffer, out_buffer_size / 4,
                                              (const uint8_t **) mFrame->data, mFrame->nb_samples);
                    if (samples > 0) {
                        current_buffer_size =
                                samples * 2 * 2; // samples * channels * bytes per sample
                        return true;
                    }
                    // 获取采样缓冲区的真实大小
                    buff_size = av_samples_get_buffer_size(NULL, channel_count,
                                                            mFrame->nb_samples,
                                                            AV_SAMPLE_FMT_S16, 1);
                } else if (ret == AVERROR(EAGAIN)) {
                    continue;
                } else {
                    is_finished = true;
                    return false;
                }
            }
        } else {
            av_packet_unref(mPacket);
        }
    }

    result = (*helper.bufferQueueItf)->Enqueue(helper.bufferQueueItf, outBuffer, buff_size);
    if (!helper.isSuccess(result)) {
        LOGE("first enqueue error: %d", result);
    }
    return false;
}

void PlayOpenSL::playerCallback(SLAndroidSimpleBufferQueueItf bq, void *pContext) {
    LOGE("playerCallback==============");
    PlayOpenSL *player = static_cast<PlayOpenSL *>(pContext);
    if (player != nullptr) {
        player->decodeAudioFrame();
    }

}

JNIEnv *PlayOpenSL::GetJNIEnv(bool *isAttach) {
    JNIEnv *env;
    int status;
    if (nullptr == mJavaVm) {
        LOGE("GetJNIEnv mJavaVm == nullptr");
        return nullptr;
    }
    *isAttach = false;
    status = mJavaVm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status != JNI_OK) {
        status = mJavaVm->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGE("GetJNIEnv failed to attach current thread");
            return nullptr;
        }
        *isAttach = true;
    }
    return env;
}

void PlayOpenSL::PostStatusMessage(const char *msg) {
    bool isAttach = false;
    JNIEnv *pEnv = GetJNIEnv(&isAttach);
    if (pEnv == nullptr) {
        return;
    }

    jmethodID mid = pEnv->GetMethodID(pEnv->GetObjectClass(mJavaObj), "CppStatusCallback",
                                      "(Ljava/lang/String;)V");
    if (mid) {
        jstring jMsg = pEnv->NewStringUTF(msg);
        pEnv->CallVoidMethod(mJavaObj, mid, jMsg);
        pEnv->DeleteLocalRef(jMsg);
    }

    if (isAttach) {
        mJavaVm->DetachCurrentThread();
    }
}