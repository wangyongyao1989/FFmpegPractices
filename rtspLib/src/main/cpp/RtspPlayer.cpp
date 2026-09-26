//  Author : wangyao https://github.com/wangyongyao1989
#include "includes/RtspPlayer.h"
#include <unistd.h>
#include <chrono>
#include <algorithm>

static std::string errToStr(int err) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(err, buf, sizeof(buf));
    return std::string(buf);
}

RtspPlayer::RtspPlayer(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);

    pthread_mutex_init(&mAudioCodecMutex, nullptr);
    pthread_mutex_init(&mVideoCodecMutex, nullptr);
    memset(mSilenceBuffer, 0, sizeof(mSilenceBuffer));
}

RtspPlayer::~RtspPlayer() {
    requestStop();
    // 等待看门狗（JNI 播放任务）完成 join，避免重复 join
    for (int i = 0; i < 100 && isPlaying(); i++) {
        usleep(50 * 1000);
    }
    joinThreads();
    cleanupAll();

    pthread_mutex_destroy(&mAudioCodecMutex);
    pthread_mutex_destroy(&mVideoCodecMutex);

    if (mJavaObj) {
        bool isAttach = false;
        JNIEnv *env = GetJNIEnv(&isAttach);
        if (env) {
            env->DeleteGlobalRef(mJavaObj);
        }
        if (isAttach) {
            mJavaVm->DetachCurrentThread();
        }
        mJavaObj = nullptr;
    }
}

JNIEnv *RtspPlayer::GetJNIEnv(bool *isAttach) {
    if (!mJavaVm) {
        LOGE("RtspPlayer::GetJNIEnv mJavaVm == nullptr");
        return nullptr;
    }
    JNIEnv *env;
    int status = mJavaVm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status != JNI_OK) {
        status = mJavaVm->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGE("RtspPlayer::GetJNIEnv attach failed");
            return nullptr;
        }
        *isAttach = true;
    } else {
        *isAttach = false;
    }
    return env;
}

void RtspPlayer::PostStatusMessage(const char *msg) {
    bool isAttach = false;
    JNIEnv *pEnv = GetJNIEnv(&isAttach);
    if (pEnv == nullptr) {
        return;
    }
    jmethodID mid = pEnv->GetMethodID(pEnv->GetObjectClass(mJavaObj), "CppStatusCallback",
                                      "(Ljava/lang/String;)V");
    jstring pJstring = pEnv->NewStringUTF(msg);
    pEnv->CallVoidMethod(mJavaObj, mid, pJstring);
    pEnv->DeleteLocalRef(pJstring);
    if (isAttach) {
        mJavaVm->DetachCurrentThread();
    }
}

int RtspPlayer::interruptCallback(void *ctx) {
    RtspPlayer *player = static_cast<RtspPlayer *>(ctx);
    return player->mStopFlag.load() ? 1 : 0;
}

// ---------------- 会话建立 ----------------

bool RtspPlayer::openInput(const char *url) {
    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);   // 拉流强制 TCP，避免 UDP 丢包花屏
    av_dict_set(&opts, "timeout", "8000000", 0);      // socket 超时 8s（ffmpeg 5+）
    av_dict_set(&opts, "stimeout", "8000000", 0);     // 兼容旧版命名
    av_dict_set(&opts, "max_delay", "500000", 0);     // 500ms 复用缓冲，压低时延

    mFormatContext = avformat_alloc_context();
    if (!mFormatContext) {
        av_dict_free(&opts);
        return false;
    }
    mFormatContext->interrupt_callback.callback = interruptCallback;
    mFormatContext->interrupt_callback.opaque = this;

    int ret = avformat_open_input(&mFormatContext, url, nullptr, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        LOGE("openInput: cannot open %s: %s", url, errToStr(ret).c_str());
        mFormatContext = nullptr;
        return false;
    }

    ret = avformat_find_stream_info(mFormatContext, nullptr);
    if (ret < 0) {
        LOGE("openInput: find_stream_info failed: %s", errToStr(ret).c_str());
        avformat_close_input(&mFormatContext);
        return false;
    }

    mVideoStreamIndex = av_find_best_stream(mFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    mAudioStreamIndex = av_find_best_stream(mFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (mVideoStreamIndex < 0 && mAudioStreamIndex < 0) {
        LOGE("openInput: no media stream found");
        avformat_close_input(&mFormatContext);
        return false;
    }

    if (mVideoStreamIndex >= 0) {
        AVCodecParameters *par = mFormatContext->streams[mVideoStreamIndex]->codecpar;
        mVideoTimeBase = mFormatContext->streams[mVideoStreamIndex]->time_base;
        mWidth = par->width;
        mHeight = par->height;
        if (par->codec_id == AV_CODEC_ID_H264) {
            mHardMime = "video/avc";
        } else if (par->codec_id == AV_CODEC_ID_HEVC) {
            mHardMime = "video/hevc";
        } else {
            mHardMime.clear();
        }
        // 提取 SPS/PPS：先取 extradata，再扫描前几个视频包（RTSP 的 SPS/PPS 常带内重复）
        extractSpsPps(par->extradata, par->extradata_size, mSpsData, mPpsData);
        if (mSpsData.empty()) {
            AVPacket *probe = av_packet_alloc();
            for (int i = 0; i < 12 && !mStopFlag.load(); i++) {
                if (av_read_frame(mFormatContext, probe) < 0) {
                    break;
                }
                if (probe->stream_index == mVideoStreamIndex) {
                    extractSpsPpsFromAnnexB(probe->data, probe->size, mSpsData, mPpsData);
                }
                if (mSpsData.empty()) {
                    // 未拿到参数集前不能把包丢给解码线程，缓存到队列
                    AVPacket *clone = av_packet_clone(probe);
                    mVideoPackets.push(clone);
                }
                av_packet_unref(probe);
                if (!mSpsData.empty()) {
                    break;
                }
            }
            av_packet_free(&probe);
        }
    }
    if (mAudioStreamIndex >= 0) {
        mAudioTimeBase = mFormatContext->streams[mAudioStreamIndex]->time_base;
    }

    infoMsg = "RTSP 打开成功: " + string(url) +
              (mVideoStreamIndex >= 0 ? ", video codec: " +
               string(avcodec_get_name(mFormatContext->streams[mVideoStreamIndex]->codecpar->codec_id))
                                      + " " + to_string(mWidth) + "x" + to_string(mHeight) : ", 无视频流") +
              (mAudioStreamIndex >= 0 ? ", audio codec: " +
               string(avcodec_get_name(mFormatContext->streams[mAudioStreamIndex]->codecpar->codec_id)) : ", 无音频流") +
              "\n";
    PostStatusMessage(infoMsg.c_str());
    return true;
}

void RtspPlayer::closeInput() {
    if (mFormatContext) {
        avformat_close_input(&mFormatContext);
        mFormatContext = nullptr;
    }
}

bool RtspPlayer::openAudioCodec() {
    if (mAudioStreamIndex < 0 || !mFormatContext) {
        return false;
    }
    AVCodecParameters *par = mFormatContext->streams[mAudioStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec) {
        LOGE("openAudioCodec: decoder not found");
        return false;
    }
    mAudioCodecCtx = avcodec_alloc_context3(codec);
    if (!mAudioCodecCtx || avcodec_parameters_to_context(mAudioCodecCtx, par) < 0 ||
        avcodec_open2(mAudioCodecCtx, codec, nullptr) < 0) {
        LOGE("openAudioCodec: open failed");
        closeAudioCodec();
        return false;
    }

    mSampleRate = mAudioCodecCtx->sample_rate > 0 ? mAudioCodecCtx->sample_rate : 44100;
    mChannels = mAudioCodecCtx->ch_layout.nb_channels > 0 ? mAudioCodecCtx->ch_layout.nb_channels : 2;

    // 重采样：原始格式 → S16 交错，保持声道数与采样率
    AVChannelLayout outLayout{};
    av_channel_layout_default(&outLayout, mChannels);
    int ret = swr_alloc_set_opts2(&mSwrContext,
                                  &outLayout, AV_SAMPLE_FMT_S16, mSampleRate,
                                  &mAudioCodecCtx->ch_layout, mAudioCodecCtx->sample_fmt,
                                  mAudioCodecCtx->sample_rate, 0, nullptr);
    if (ret < 0 || !mSwrContext || swr_init(mSwrContext) < 0) {
        LOGE("openAudioCodec: swr init failed");
        if (mSwrContext) {
            swr_free(&mSwrContext);
        }
        av_channel_layout_uninit(&outLayout);
        closeAudioCodec();
        return false;
    }
    mOutChLayout = outLayout;
    mHasAudio.store(true);
    return true;
}

void RtspPlayer::closeAudioCodec() {
    if (mSwrContext) {
        swr_free(&mSwrContext);
        mSwrContext = nullptr;
    }
    if (mAudioCodecCtx) {
        avcodec_free_context(&mAudioCodecCtx);
        mAudioCodecCtx = nullptr;
    }
    mHasAudio.store(false);
}

bool RtspPlayer::openVideoCodecSoft() {
    if (mVideoStreamIndex < 0 || !mFormatContext) {
        return false;
    }
    AVCodecParameters *par = mFormatContext->streams[mVideoStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec) {
        return false;
    }
    mVideoCodecCtx = avcodec_alloc_context3(codec);
    if (!mVideoCodecCtx || avcodec_parameters_to_context(mVideoCodecCtx, par) < 0 ||
        avcodec_open2(mVideoCodecCtx, codec, nullptr) < 0) {
        LOGE("openVideoCodecSoft: open failed");
        closeVideoCodecSoft();
        return false;
    }
    return true;
}

void RtspPlayer::closeVideoCodecSoft() {
    if (mVideoCodecCtx) {
        avcodec_free_context(&mVideoCodecCtx);
        mVideoCodecCtx = nullptr;
    }
}

void RtspPlayer::extractSpsPps(const uint8_t *extra, int extraSize,
                               vector<uint8_t> &sps, vector<uint8_t> &pps) {
    if (!extra || extraSize <= 0) {
        return;
    }
    if (extra[0] == 0 && extra[1] == 0) {
        // AnnexB
        extractSpsPpsFromAnnexB(extra, extraSize, sps, pps);
    } else if (extra[0] == 1 && extraSize > 7) {
        // AVCC(hvcC 暂不支持，只处理 avcC)
        int numSPS = extra[5] & 0x1F;
        int offset = 6;
        for (int i = 0; i < numSPS && offset + 2 <= extraSize; i++) {
            int len = (extra[offset] << 8) | extra[offset + 1];
            offset += 2;
            if (offset + len > extraSize) {
                break;
            }
            sps.assign(extra + offset, extra + offset + len);
            offset += len;
        }
        if (offset + 1 < extraSize) {
            int numPPS = extra[offset];
            offset++;
            for (int i = 0; i < numPPS && offset + 2 <= extraSize; i++) {
                int len = (extra[offset] << 8) | extra[offset + 1];
                offset += 2;
                if (offset + len > extraSize) {
                    break;
                }
                pps.assign(extra + offset, extra + offset + len);
                offset += len;
            }
        }
        // 转成 AnnexB 供 MediaCodec csd 使用
        if (!sps.empty()) {
            vector<uint8_t> tmp = {0, 0, 0, 1};
            tmp.insert(tmp.end(), sps.begin(), sps.end());
            sps = tmp;
        }
        if (!pps.empty()) {
            vector<uint8_t> tmp = {0, 0, 0, 1};
            tmp.insert(tmp.end(), pps.begin(), pps.end());
            pps = tmp;
        }
    }
}

void RtspPlayer::extractSpsPpsFromAnnexB(const uint8_t *buf, int size,
                                         vector<uint8_t> &sps, vector<uint8_t> &pps) {
    int i = 0;
    while (i + 3 < size) {
        int scLen = 0;
        if (buf[i] == 0 && buf[i + 1] == 0 && buf[i + 2] == 1) {
            scLen = 3;
        } else if (i + 4 < size && buf[i] == 0 && buf[i + 1] == 0 && buf[i + 2] == 0 &&
                   buf[i + 3] == 1) {
            scLen = 4;
        }
        if (scLen == 0) {
            i++;
            continue;
        }
        int nalStart = i + scLen;
        int nalType = nalStart < size ? (buf[nalStart] & 0x1F) : 0;
        int next = nalStart;
        // 找下一个起始码
        while (next + 3 < size) {
            if (buf[next] == 0 && buf[next + 1] == 0 &&
                (buf[next + 2] == 1 || (next + 4 < size && buf[next + 2] == 0 && buf[next + 3] == 1))) {
                break;
            }
            next++;
        }
        if (next + 3 >= size) {
            next = size;
        }
        if (nalType == 7 && sps.empty()) {
            sps.assign(buf + i, buf + next);   // 含起始码
        } else if (nalType == 8 && pps.empty()) {
            pps.assign(buf + i, buf + next);
        }
        i = next;
    }
}

// ---------------- 播放/停止 ----------------

bool RtspPlayer::play(const char *url, jobject surface, bool useHardDecode) {
    if (isPlaying()) {
        PostStatusMessage("RtspPlayer 已在播放，先停止\n");
        return false;
    }

    mStopFlag.store(false);
    mReopening.store(false);
    mStreamLost.store(false);
    mSysBaseSet.store(false);
    mAudioClock.store(0);
    mHardInited.store(false);
    mHardFailed.store(false);
    mSpsData.clear();
    mPpsData.clear();
    // 会话起点先丢到下一个关键帧：直播不从队列深处追赶，直接从实时边缘开播，
    // 避免音视频队列被灌满后主时钟长期落后于画面 pts
    mVideoDropping = true;
    // restart() 只是把队列的停止标志清掉（见 ThreadSafeQueue::restart），
    // 队列内容一个都不动。只靠退出路径上的 drainPacketQueues() 还不够：
    // 上一会话的包/帧如果还留在队列里（例如软/硬解切换时读线程刚 push 完就退出），
    // 新会话的解码线程会先消费这些旧数据——表现就是切解码方式后先闪一下
    // 上一段画面、音频也是旧内容续播。所以起播前显式排空并释放一次。
    drainPacketQueues();
    mVideoPackets.restart();
    mAudioPackets.restart();
    mVideoFrames.restart();
    mAudioChunks.restart();
    // 同一 RtspPlayer 实例第二次 play()（软/硬解切换、停止后再起播）必须复位音频状态，
    // 否则上一会话残留的 mQueuedBufferCount 会让播放线程误判队列已满而永不入队
    resetAudioPipelineState();

    mUrl = strdup(url);
    mUseHardDecode.store(useHardDecode);

    // Surface → ANativeWindow（play 由 JNI 线程池调用，需要自行附加 JNIEnv）
    bool isAttach = false;
    JNIEnv *env = GetJNIEnv(&isAttach);
    if (!env) {
        free(mUrl);
        mUrl = nullptr;
        return false;
    }
    jobject surfaceGlobal = env->NewGlobalRef(surface);
    mNativeWindow = ANativeWindow_fromSurface(env, surfaceGlobal);
    env->DeleteGlobalRef(surfaceGlobal);
    if (isAttach) {
        mJavaVm->DetachCurrentThread();
    }
    if (!mNativeWindow) {
        PostStatusMessage("ANativeWindow 获取失败\n");
        free(mUrl);
        mUrl = nullptr;
        return false;
    }

    if (!openInput(url)) {
        PostStatusMessage("RTSP 打开失败，检查地址与网络\n");
        cleanupAll();
        mState.store(STATE_ERROR);
        return false;
    }

    pthread_mutex_lock(&mAudioCodecMutex);
    bool audioOk = openAudioCodec();
    pthread_mutex_unlock(&mAudioCodecMutex);

    bool videoSoftOk = true;
    pthread_mutex_lock(&mVideoCodecMutex);
    if (mUseHardDecode.load()) {
        if (mHardMime.empty()) {
            mHardFailed.store(true);
            PostStatusMessage("视频编码不支持 MediaCodec，回退软解\n");
            videoSoftOk = openVideoCodecSoft();
        }
    } else {
        videoSoftOk = openVideoCodecSoft();
    }
    pthread_mutex_unlock(&mVideoCodecMutex);

    if (!audioOk && mVideoStreamIndex < 0) {
        PostStatusMessage("无可播放流\n");
        cleanupAll();
        mState.store(STATE_ERROR);
        return false;
    }
    if (mVideoStreamIndex >= 0 && !videoSoftOk && mHardFailed.load()) {
        PostStatusMessage("视频解码器打开失败\n");
        cleanupAll();
        mState.store(STATE_ERROR);
        return false;
    }

    mState.store(STATE_RUNNING);

    pthread_create(&mDemuxThread, nullptr, demuxThreadWrapper, this);
    if (mAudioStreamIndex >= 0) {
        pthread_create(&mAudioDecodeThread, nullptr, audioDecodeThreadWrapper, this);
        pthread_create(&mAudioPlayThread, nullptr, audioPlayThreadWrapper, this);
    }
    if (mVideoStreamIndex >= 0) {
        if (mUseHardDecode.load() && !mHardFailed.load()) {
            pthread_create(&mVideoHardThread, nullptr, videoHardThreadWrapper, this);
        } else {
            pthread_create(&mVideoDecodeThread, nullptr, videoDecodeThreadWrapper, this);
            pthread_create(&mVideoRenderThread, nullptr, videoRenderThreadWrapper, this);
        }
    }

    infoMsg = std::string("RtspPlayer 启动（") +
              (mUseHardDecode.load() && !mHardFailed.load() ? "硬解 MediaCodec" : "软解 FFmpeg") +
              (mHasAudio.load() ? " + OpenSL 音频" : "，无音频") + "）\n";
    PostStatusMessage(infoMsg.c_str());
    return true;
}

void RtspPlayer::requestStop() {
    if (mState.load() != STATE_RUNNING) {
        return;
    }
    mStopFlag.store(true);
    // 唤醒所有阻塞在 pop 上的消费者
    mVideoPackets.stop();
    mAudioPackets.stop();
    mVideoFrames.stop();
    mAudioChunks.stop();
}

void RtspPlayer::finishSession() {
    while (mState.load() == STATE_RUNNING && !mStopFlag.load()) {
        usleep(50 * 1000);
    }
    int expected = STATE_RUNNING;
    if (!mState.compare_exchange_strong(expected, STATE_STOPPING)) {
        return;   // 其他调用者（或 play 失败路径）负责收尾
    }
    joinThreads();
    cleanupAll();
    mState.store(mStreamLost.load() ? STATE_ERROR : STATE_STOPPED);
    PostStatusMessage(mStreamLost.load() ? "RTSP 流中断（重连失败），会话结束\n"
                                         : "RtspPlayer 已停止\n");
}

void RtspPlayer::joinThreads() {
    if (mDemuxThread) {
        pthread_join(mDemuxThread, nullptr);
        mDemuxThread = 0;
    }
    if (mAudioDecodeThread) {
        pthread_join(mAudioDecodeThread, nullptr);
        mAudioDecodeThread = 0;
    }
    if (mAudioPlayThread) {
        pthread_join(mAudioPlayThread, nullptr);
        mAudioPlayThread = 0;
    }
    if (mVideoHardThread) {
        pthread_join(mVideoHardThread, nullptr);
        mVideoHardThread = 0;
    }
    // 硬解线程内部可能动态启动了软解线程
    if (mVideoDecodeThread) {
        pthread_join(mVideoDecodeThread, nullptr);
        mVideoDecodeThread = 0;
    }
    if (mVideoRenderThread) {
        pthread_join(mVideoRenderThread, nullptr);
        mVideoRenderThread = 0;
    }
}

void RtspPlayer::cleanupAll() {
    pthread_mutex_lock(&mAudioCodecMutex);
    closeAudioCodec();
    pthread_mutex_unlock(&mAudioCodecMutex);

    pthread_mutex_lock(&mVideoCodecMutex);
    closeVideoCodecSoft();
    mHardDecoder.release();
    mHardInited.store(false);
    pthread_mutex_unlock(&mVideoCodecMutex);

    closeInput();
    releaseSwRenderer();
    releaseOpenSL();

    if (mNativeWindow) {
        ANativeWindow_release(mNativeWindow);
        mNativeWindow = nullptr;
    }
    if (mUrl) {
        free(mUrl);
        mUrl = nullptr;
    }
    mVideoStreamIndex = -1;
    mAudioStreamIndex = -1;
    mHasAudio.store(false);
}

// ---------------- 时钟 ----------------

double RtspPlayer::getMasterClock() {
    double audio = mAudioClock.load();
    if (mHasAudio.load() && audio > 0.001) {
        return audio;
    }
    if (!mSysBaseSet.load()) {
        return 0;
    }
    return mSysBasePts + (double) (av_gettime() - mSysBaseTimeUs) / 1000000.0;
}

void RtspPlayer::waitUntilClock(double targetPts, int64_t maxWaitUs) {
    if (!mHasAudio.load() && !mSysBaseSet.load()) {
        mSysBasePts = targetPts;
        mSysBaseTimeUs = av_gettime();
        mSysBaseSet.store(true);
        return;
    }
    int64_t deadline = av_gettime() + maxWaitUs;  // 时钟停滞时最多等 maxWaitUs
    while (!mStopFlag.load()) {
        double now = getMasterClock();
        if (now < 0.001) {
            return;   // 音频时钟尚未启动，先显示
        }
        double diff = targetPts - now;
        if (diff <= SYNC_THRESHOLD) {
            return;
        }
        if (diff > MAX_FRAME_DELAY) {
            diff = MAX_FRAME_DELAY;
        }
        int64_t sleepUs = (int64_t) (diff * 1000000);
        if (sleepUs > 20000) {
            sleepUs = 20000;   // 分片睡眠保证可中断
        }
        usleep((useconds_t) sleepUs);
        if (av_gettime() > deadline) {
            return;
        }
    }
}

// ---------------- demux ----------------

void *RtspPlayer::demuxThreadWrapper(void *ctx) {
    static_cast<RtspPlayer *>(ctx)->demuxThread();
    return nullptr;
}

void RtspPlayer::demuxThread() {
    LOGI("RTSP demux thread started");
    AVPacket *pkt = av_packet_alloc();
    int errorCount = 0;

    while (!mStopFlag.load()) {
        if (mReopening.load()) {
            usleep(10 * 1000);
            continue;
        }
        int ret = av_read_frame(mFormatContext, pkt);
        if (ret >= 0) {
            errorCount = 0;
            AVPacket *clone = nullptr;
            if (pkt->stream_index == mVideoStreamIndex) {
                // 直播低延迟策略：队列满说明解码跟不上实时边缘，
                // 丢到下一个关键帧再恢复入队（H.264 必须从 I 帧恢复，否则花屏）
                bool isKey = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
                if (mVideoPackets.size() >= MAX_VIDEO_PACKETS && !mReopening.load()) {
                    mVideoDropping = true;
                }
                if (mVideoDropping && isKey) {
                    mVideoDropping = false;
                }
                if (!mVideoDropping) {
                    clone = av_packet_clone(pkt);
                    if (clone && !mStopFlag.load() && !mReopening.load()) {
                        mVideoPackets.push(clone);
                    } else if (clone) {
                        av_packet_free(&clone);
                    }
                }
            } else if (pkt->stream_index == mAudioStreamIndex) {
                // 视频正在丢到下一个关键帧时音频一起丢，避免声音跑到画面前面
                if (!mVideoDropping) {
                    clone = av_packet_clone(pkt);
                    // 有界队列 + 丢最老：demux 线程绝不允许阻塞，
                    // 一旦 TCP 读窗口堵住，MediaMTX 会判定 "reader is too slow" 并断连
                    while (mAudioPackets.size() >= MAX_AUDIO_PACKETS) {
                        AVPacket *old = nullptr;
                        if (!mAudioPackets.pop(old, std::chrono::milliseconds(0)) || !old) {
                            break;
                        }
                        av_packet_free(&old);
                    }
                    if (clone && !mStopFlag.load() && !mReopening.load()) {
                        mAudioPackets.push(clone);
                    } else if (clone) {
                        av_packet_free(&clone);
                    }
                }
            }
            av_packet_unref(pkt);
        } else {
            av_packet_unref(pkt);
            if (mStopFlag.load()) {
                break;
            }
            errorCount++;
            infoMsg = "RTSP 读取错误(" + errToStr(ret) + ")，准备重连 " +
                      to_string(errorCount) + "/" + to_string(RECONNECT_MAX) + "\n";
            LOGE("%s", infoMsg.c_str());
            PostStatusMessage(infoMsg.c_str());

            bool reconnected = false;
            for (int i = errorCount; i <= RECONNECT_MAX && !mStopFlag.load(); i++) {
                // 每次尝试间隔 3s：给推流端重启/链路恢复留出时间（MediaMTX 侧路径恢复需要几秒）
                for (int s = 0; s < 30 && !mStopFlag.load(); s++) {
                    usleep(100 * 1000);
                }
                if (mStopFlag.load()) {
                    break;
                }
                infoMsg = "RTSP 重连尝试 " + to_string(i) + "/" + to_string(RECONNECT_MAX) + "\n";
                LOGE("%s", infoMsg.c_str());
                PostStatusMessage(infoMsg.c_str());
                if (reconnectSession()) {
                    reconnected = true;
                    errorCount = 0;
                    break;
                }
            }
            if (!reconnected) {
                if (!mStopFlag.load()) {
                    mStreamLost.store(true);   // 非用户主动停止，标记断流
                }
                break;
            }
        }
    }

    av_packet_free(&pkt);
    mStopFlag.store(true);
    mVideoPackets.stop();
    mAudioPackets.stop();
    mVideoFrames.stop();
    mAudioChunks.stop();
    LOGI("RTSP demux thread finished");
}

void RtspPlayer::drainPacketQueues() {
    AVPacket *pkt = nullptr;
    while (mVideoPackets.pop(pkt, std::chrono::milliseconds(0)) && pkt) {
        av_packet_free(&pkt);
        pkt = nullptr;
    }
    while (mAudioPackets.pop(pkt, std::chrono::milliseconds(0)) && pkt) {
        av_packet_free(&pkt);
        pkt = nullptr;
    }
    VideoFrameData *vfd = nullptr;
    while (mVideoFrames.pop(vfd, std::chrono::milliseconds(0)) && vfd) {
        delete vfd;
        vfd = nullptr;
    }
    AudioChunk *chunk = nullptr;
    while (mAudioChunks.pop(chunk, std::chrono::milliseconds(0)) && chunk) {
        delete chunk;
        chunk = nullptr;
    }
}

// 会话起点复位音频播放状态。
// 上一会话 releaseOpenSL() 销毁 player 后 OpenSL 不再回调 processBufferQueue，
// mQueuedBufferCount 会停在 NUM_BUFFERS；若不复位，同一实例二次起播时播放线程
// 会一直判定"缓冲队列已满"而不入队：音频轨 standby、主时钟恒 0、视频退化成定速追赶。
void RtspPlayer::resetAudioPipelineState() {
    {
        lock_guard<mutex> lock(mPtsRingMutex);
        mPtsRing.clear();
    }
    mAudioClock.store(0);
    mLastEnqueuedEndPts = 0;
    mQueuedBufferCount.store(0);
    mFillIndex = 0;
    mAudioDisabled.store(false);
}

bool RtspPlayer::reconnectSession() {
    mReopening.store(true);
    drainPacketQueues();
    mVideoDropping = true;   // 重连同样从下一个关键帧起播（实时边缘）

    pthread_mutex_lock(&mAudioCodecMutex);
    closeAudioCodec();
    pthread_mutex_unlock(&mAudioCodecMutex);

    pthread_mutex_lock(&mVideoCodecMutex);
    closeVideoCodecSoft();
    pthread_mutex_unlock(&mVideoCodecMutex);
    closeInput();

    bool opened = false;
    if (mUrl) {
        opened = openInput(mUrl);
    }
    if (!opened) {
        mReopening.store(false);
        return false;
    }

    pthread_mutex_lock(&mAudioCodecMutex);
    openAudioCodec();
    pthread_mutex_unlock(&mAudioCodecMutex);

    pthread_mutex_lock(&mVideoCodecMutex);
    if (mUseHardDecode.load() && !mHardMime.empty()) {
        // 硬解线程检测到 mSessionGen 变化后重建/冲刷解码器
    } else if (mVideoStreamIndex >= 0) {
        openVideoCodecSoft();
    }
    pthread_mutex_unlock(&mVideoCodecMutex);

    resetAudioPipelineState();
    mSysBaseSet.store(false);
    mSessionGen.fetch_add(1);
    mReopening.store(false);

    PostStatusMessage("RTSP 重连成功，会话已重置\n");
    return true;
}

// ---------------- 音频解码 ----------------

void *RtspPlayer::audioDecodeThreadWrapper(void *ctx) {
    static_cast<RtspPlayer *>(ctx)->audioDecodeThread();
    return nullptr;
}

void RtspPlayer::audioDecodeThread() {
    LOGI("RTSP audio decode thread started");
    AVFrame *frame = av_frame_alloc();
    while (!mStopFlag.load()) {
        AVPacket *pkt = nullptr;
        if (!mAudioPackets.pop(pkt, std::chrono::milliseconds(20)) || !pkt) {
            continue;
        }
        if (mAudioDisabled.load()) {
            av_packet_free(&pkt);
            continue;
        }

        pthread_mutex_lock(&mAudioCodecMutex);
        if (mAudioCodecCtx && mSwrContext && !mReopening.load()) {
            int ret = avcodec_send_packet(mAudioCodecCtx, pkt);
            if (ret == 0) {
                while (avcodec_receive_frame(mAudioCodecCtx, frame) == 0) {
                    int64_t bestPts = frame->best_effort_timestamp != AV_NOPTS_VALUE
                                      ? frame->best_effort_timestamp : frame->pts;
                    double ptsSec = bestPts != AV_NOPTS_VALUE
                                    ? bestPts * av_q2d(mAudioTimeBase)
                                    : mAudioClock.load();

                    int outSamples = av_rescale_rnd(
                            swr_get_delay(mSwrContext, mSampleRate) + frame->nb_samples,
                            mSampleRate, mAudioCodecCtx->sample_rate, AV_ROUND_UP);
                    auto *chunk = new AudioChunk();
                    chunk->data = (uint8_t *) av_malloc((size_t) outSamples * mChannels * 2);
                    chunk->pts = ptsSec;
                    int converted = swr_convert(mSwrContext, &chunk->data, outSamples,
                                                (const uint8_t **) frame->data, frame->nb_samples);
                    if (converted > 0 && chunk->data) {
                        chunk->size = converted * mChannels * 2;
                        // 容量控制：丢最老的 PCM 块而不是阻塞解码线程，
                        // 否则会顺着 mAudioPackets 反压到 demux 造成读取停滞
                        while (mAudioChunks.size() >= MAX_AUDIO_CHUNKS) {
                            AudioChunk *old = nullptr;
                            if (!mAudioChunks.pop(old, std::chrono::milliseconds(0)) || !old) {
                                break;
                            }
                            delete old;
                        }
                        if (!mStopFlag.load()) {
                            mAudioChunks.push(chunk);
                        } else {
                            delete chunk;
                        }
                    } else {
                        delete chunk;
                    }
                    av_frame_unref(frame);
                }
            }
        }
        pthread_mutex_unlock(&mAudioCodecMutex);
        av_packet_free(&pkt);
    }
    av_frame_free(&frame);
    LOGI("RTSP audio decode thread finished");
}

// ---------------- 音频播放（OpenSL ES） ----------------

bool RtspPlayer::initOpenSL(int sampleRate, int channels) {
    SLresult result = mOpensl.createEngine();
    if (!mOpensl.isSuccess(result)) {
        PostStatusMessage("OpenSL createEngine fail \n");
        return false;
    }
    result = mOpensl.createMix();
    if (!mOpensl.isSuccess(result)) {
        PostStatusMessage("OpenSL createMix fail \n");
        return false;
    }
    int channelMask = channels == 1 ? SL_SPEAKER_FRONT_CENTER
                                    : (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT);
    // OpenSL ES 规范中采样率单位为毫赫兹（与 playMediaLib 可用写法一致：sampleRate * 1000）
    result = mOpensl.createPlayer(channels, sampleRate * 1000, SL_PCMSAMPLEFORMAT_FIXED_16,
                                  channelMask);
    if (!mOpensl.isSuccess(result)) {
        PostStatusMessage("OpenSL createPlayer fail \n");
        return false;
    }
    result = mOpensl.registerCallback(bufferQueueCallback, this);
    if (!mOpensl.isSuccess(result)) {
        PostStatusMessage("OpenSL registerCallback fail \n");
        return false;
    }
    result = mOpensl.play();
    if (!mOpensl.isSuccess(result)) {
        PostStatusMessage("OpenSL play fail \n");
        return false;
    }
    mOpenSLReady = true;
    LOGI("OpenSL ready: %dHz %dch", sampleRate, channels);
    return true;
}

void RtspPlayer::releaseOpenSL() {
    if (mOpenSLReady) {
        mOpensl.release();
        mOpenSLReady = false;
    }
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (mPlayBuffers[i]) {
            av_free(mPlayBuffers[i]);
            mPlayBuffers[i] = nullptr;
        }
    }
}

void RtspPlayer::bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    static_cast<RtspPlayer *>(context)->processBufferQueue();
}

void RtspPlayer::processBufferQueue() {
    // 一个 buffer 播放完成：FIFO 最老的先完成，其结束时间即当前音频时钟
    {
        lock_guard<mutex> lock(mPtsRingMutex);
        if (!mPtsRing.empty()) {
            mAudioClock.store(mPtsRing.front());
            mPtsRing.pop_front();
        }
    }
    if (mQueuedBufferCount > 0) {
        mQueuedBufferCount--;
    }
}

void *RtspPlayer::audioPlayThreadWrapper(void *ctx) {
    static_cast<RtspPlayer *>(ctx)->audioPlayThread();
    return nullptr;
}

void RtspPlayer::audioPlayThread() {
    LOGI("RTSP audio play thread started");
    int bytesPerSecond = mSampleRate * mChannels * 2;
    int lastGen = mSessionGen.load();

    if (!initOpenSL(mSampleRate, mChannels)) {
        // OpenSL 不可用：关掉音频时钟标志，视频/硬解节奏回退到系统时钟，避免死等；
        // 并让解码线程直接丢弃音频包，防止队列堆积反压 demux。
        mHasAudio.store(false);
        mAudioDisabled.store(true);
        PostStatusMessage("音频播放不可用，视频改用系统时钟\n");
        return;
    }
    for (int i = 0; i < NUM_BUFFERS; i++) {
        mPlayBuffers[i] = (uint8_t *) av_malloc(PLAY_BUFFER_SIZE);
    }

    while (!mStopFlag.load()) {
        if (mReopening.load()) {
            usleep(10 * 1000);
            continue;
        }
        if (lastGen != mSessionGen.load()) {
            lastGen = mSessionGen.load();
            mQueuedBufferCount.store(0);
            mFillIndex = 0;
            (*mOpensl.bufferQueueItf)->Clear(mOpensl.bufferQueueItf);
            lock_guard<mutex> lock(mPtsRingMutex);
            mPtsRing.clear();
        }

        if (mQueuedBufferCount.load() >= NUM_BUFFERS) {
            usleep(5 * 1000);
            continue;
        }

        AudioChunk *chunk = nullptr;
        if (mAudioChunks.pop(chunk, std::chrono::milliseconds(20)) && chunk) {
            int offset = 0;
            double baseSec = (double) chunk->size / bytesPerSecond;
            while (offset < chunk->size && !mStopFlag.load()) {
                int seg = std::min(chunk->size - offset, PLAY_BUFFER_SIZE);
                memcpy(mPlayBuffers[mFillIndex], chunk->data + offset, seg);
                double segSec = (double) seg / bytesPerSecond;
                mLastEnqueuedEndPts = chunk->pts + baseSec * ((double) (offset + seg) / chunk->size);
                {
                    lock_guard<mutex> lock(mPtsRingMutex);
                    mPtsRing.push_back(mLastEnqueuedEndPts);
                }
                SLresult result = (*mOpensl.bufferQueueItf)->Enqueue(
                        mOpensl.bufferQueueItf, mPlayBuffers[mFillIndex], seg);
                if (result != SL_RESULT_SUCCESS) {
                    break;
                }
                mQueuedBufferCount++;
                mFillIndex = (mFillIndex + 1) % NUM_BUFFERS;
                offset += seg;
            }
            delete chunk;
        } else {
            // 数据饥饿：补 20ms 静音维持播放管线（不推动时钟前进过多）
            if (mQueuedBufferCount.load() < NUM_BUFFERS) {
                int seg = (int) (0.02 * bytesPerSecond);
                if (seg > PLAY_BUFFER_SIZE) {
                    seg = PLAY_BUFFER_SIZE;
                }
                mLastEnqueuedEndPts += 0.02;
                {
                    lock_guard<mutex> lock(mPtsRingMutex);
                    mPtsRing.push_back(mLastEnqueuedEndPts);
                }
                (*mOpensl.bufferQueueItf)->Enqueue(mOpensl.bufferQueueItf, mSilenceBuffer, seg);
                mQueuedBufferCount++;
                mFillIndex = (mFillIndex + 1) % NUM_BUFFERS;
            }
        }
    }
    LOGI("RTSP audio play thread finished");
}

// ---------------- 视频软解 ----------------

void *RtspPlayer::videoDecodeThreadWrapper(void *ctx) {
    static_cast<RtspPlayer *>(ctx)->videoDecodeThread();
    return nullptr;
}

void RtspPlayer::videoDecodeThread() {
    LOGI("RTSP video decode(soft) thread started");
    AVFrame *frame = av_frame_alloc();
    int64_t lastStatUs = av_gettime();
    int decOutCount = 0;    // 本窗口内解出的帧数
    int frameDropCount = 0; // 本窗口内因渲染队列满而丢掉的帧数
    double decLastPts = -1;
    while (!mStopFlag.load()) {
        {   // 每 5s 汇报一次软解管线状态
            int64_t nowUs = av_gettime();
            if (nowUs - lastStatUs > 5 * 1000000) {
                lastStatUs = nowUs;
                LOGI("SOFT-DEC clock=%.2f lastPts=%.2f vpQ=%zu vfQ=%zu drop=%d out=%d lost=%d",
                     getMasterClock(), decLastPts, mVideoPackets.size(), mVideoFrames.size(),
                     mVideoDropping ? 1 : 0, decOutCount, frameDropCount);
                decOutCount = 0;
                frameDropCount = 0;
            }
        }
        AVPacket *pkt = nullptr;
        if (!mVideoPackets.pop(pkt, std::chrono::milliseconds(20)) || !pkt) {
            continue;
        }
        pthread_mutex_lock(&mVideoCodecMutex);
        if (mVideoCodecCtx && !mReopening.load()) {
            if (avcodec_send_packet(mVideoCodecCtx, pkt) == 0) {
                while (avcodec_receive_frame(mVideoCodecCtx, frame) == 0) {
                    int64_t bestPts = frame->best_effort_timestamp != AV_NOPTS_VALUE
                                      ? frame->best_effort_timestamp : frame->pts;
                    auto *vfd = new VideoFrameData();
                    vfd->frame = av_frame_clone(frame);
                    vfd->pts = bestPts != AV_NOPTS_VALUE
                               ? bestPts * av_q2d(mVideoTimeBase) : 0;
                    av_frame_unref(frame);
                    decLastPts = vfd->pts;
                    decOutCount++;
                    // 渲染队列满：丢最老的一帧（直播只看最新画面），
                    // 绝不能在这里 sleep，否则会反压 demux 触发服务端 "reader is too slow"
                    while (mVideoFrames.size() >= MAX_VIDEO_FRAMES) {
                        VideoFrameData *old = nullptr;
                        if (!mVideoFrames.pop(old, std::chrono::milliseconds(0)) || !old) {
                            break;
                        }
                        delete old;
                        frameDropCount++;
                    }
                    if (!mStopFlag.load()) {
                        mVideoFrames.push(vfd);
                    } else {
                        delete vfd;
                    }
                }
            }
        }
        pthread_mutex_unlock(&mVideoCodecMutex);
        av_packet_free(&pkt);
    }
    av_frame_free(&frame);
    mVideoFrames.stop();
    LOGI("RTSP video decode(soft) thread finished");
}

bool RtspPlayer::prepareSwRenderer(int width, int height) {
    if (mSwsContext && mSrcWidth == width && mSrcHeight == height) {
        return true;
    }
    releaseSwRenderer();
    mSrcWidth = width;
    mSrcHeight = height;
    // 输出按 MAX_RENDER_WIDTH 等比缩小，降低 CPU 缩放/拷贝开销
    mRgbWidth = width;
    mRgbHeight = height;
    if (mRgbWidth > MAX_RENDER_WIDTH) {
        mRgbWidth = MAX_RENDER_WIDTH;
        mRgbHeight = (int) ((long long) height * mRgbWidth / width) & ~1;
        if (mRgbHeight <= 0) {
            mRgbHeight = 2;
        }
    }
    LOGI("prepareSwRenderer: %dx%d -> %dx%d", mSrcWidth, mSrcHeight, mRgbWidth, mRgbHeight);

    mRgbaFrame = av_frame_alloc();
    if (!mRgbaFrame) {
        return false;
    }
    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, mRgbWidth, mRgbHeight, 1);
    mOutBuffer = (uint8_t *) av_malloc(bufferSize);
    if (!mOutBuffer) {
        return false;
    }
    if (av_image_fill_arrays(mRgbaFrame->data, mRgbaFrame->linesize, mOutBuffer,
                             AV_PIX_FMT_RGBA, mRgbWidth, mRgbHeight, 1) < 0) {
        return false;
    }
    mSwsContext = sws_getContext(mSrcWidth, mSrcHeight, AV_PIX_FMT_YUV420P,
                                 mRgbWidth, mRgbHeight, AV_PIX_FMT_RGBA,
                                 SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!mSwsContext) {
        return false;
    }
    if (ANativeWindow_setBuffersGeometry(mNativeWindow, mRgbWidth, mRgbHeight,
                                         WINDOW_FORMAT_RGBA_8888) < 0) {
        LOGE("prepareSwRenderer: setBuffersGeometry failed");
        return false;
    }
    return true;
}

void RtspPlayer::releaseSwRenderer() {
    if (mSwsContext) {
        sws_freeContext(mSwsContext);
        mSwsContext = nullptr;
    }
    if (mRgbaFrame) {
        av_frame_free(&mRgbaFrame);
        mRgbaFrame = nullptr;
    }
    if (mOutBuffer) {
        av_free(mOutBuffer);
        mOutBuffer = nullptr;
    }
    mRgbWidth = 0;
    mRgbHeight = 0;
    mSrcWidth = 0;
    mSrcHeight = 0;
}

void RtspPlayer::renderFrame(AVFrame *frame, RenderStageStat *stat) {
    if (!mNativeWindow || !frame) {
        return;
    }
    int64_t t0 = av_gettime();
    if (!prepareSwRenderer(frame->width, frame->height)) {
        return;
    }
    // srcSliceH 是「源」切片高度，缩放后与目标高度不同，不能用 mRgbHeight
    sws_scale(mSwsContext, frame->data, frame->linesize, 0, mSrcHeight,
              mRgbaFrame->data, mRgbaFrame->linesize);
    int64_t t1 = av_gettime();

    ANativeWindow_Buffer windowBuffer;
    if (ANativeWindow_lock(mNativeWindow, &windowBuffer, nullptr) < 0) {
        LOGE("renderFrame: cannot lock window");
        return;
    }
    int64_t t2 = av_gettime();
    uint8_t *dst = (uint8_t *) windowBuffer.bits;
    int dstStride = windowBuffer.stride * 4;
    int srcStride = mRgbaFrame->linesize[0];
    // 窗口缓冲尺寸可能与我们设置的 geometry 不同（含 padding），按两者较小值拷贝避免越界
    int rows = std::min(mRgbHeight, windowBuffer.height);
    int rowBytes = std::min(srcStride, dstStride);
    if (dstStride == srcStride) {
        memcpy(dst, mOutBuffer, (size_t) srcStride * rows);
    } else {
        for (int h = 0; h < rows; h++) {
            memcpy(dst + h * dstStride, mOutBuffer + h * srcStride, rowBytes);
        }
    }
    int64_t t3 = av_gettime();
    ANativeWindow_unlockAndPost(mNativeWindow);
    int64_t t4 = av_gettime();
    if (stat) {
        stat->swsUs  += (t1 - t0);
        stat->lockUs += (t2 - t1);
        stat->copyUs += (t3 - t2);
        stat->postUs += (t4 - t3);
        stat->frames++;
    }
}

void *RtspPlayer::videoRenderThreadWrapper(void *ctx) {
    static_cast<RtspPlayer *>(ctx)->videoRenderThread();
    return nullptr;
}

void RtspPlayer::videoRenderThread() {
    LOGI("RTSP video render thread started");
    int lastGen = mSessionGen.load();
    int64_t lastStatUs = av_gettime();
    int renCount = 0;       // 本窗口实际上屏帧数
    int renStaleDrop = 0;   // 因落后时钟被丢掉的帧数
    double renLastPts = -1;
    RenderStageStat stage{};
    long long waitUsTotal = 0;
    while (!mStopFlag.load()) {
        {   // 每 5s 汇报一次渲染状态：up=本窗口上屏帧数，后面是每帧平均耗时
            int64_t nowUs = av_gettime();
            if (nowUs - lastStatUs > 5 * 1000000) {
                lastStatUs = nowUs;
                double n = stage.frames > 0 ? stage.frames : 1;
                LOGI("SOFT-REN clock=%.2f lastPts=%.2f vfQ=%zu audioQ=%zu stale=%d up=%d "
                     "wait=%.1fms sws=%.1fms lock=%.1fms copy=%.1fms post=%.1fms",
                     getMasterClock(), renLastPts, mVideoFrames.size(),
                     mAudioChunks.size(), renStaleDrop, renCount,
                     waitUsTotal / 1000.0 / n, stage.swsUs / 1000.0 / n,
                     stage.lockUs / 1000.0 / n, stage.copyUs / 1000.0 / n,
                     stage.postUs / 1000.0 / n);
                renCount = 0;
                renStaleDrop = 0;
                waitUsTotal = 0;
                stage = RenderStageStat{};
            }
        }
        if (lastGen != mSessionGen.load()) {
            lastGen = mSessionGen.load();
            mSysBaseSet.store(false);
        }
        VideoFrameData *vfd = nullptr;
        if (!mVideoFrames.pop(vfd, std::chrono::milliseconds(20)) || !vfd) {
            continue;
        }
        double clock = getMasterClock();
        if (clock > 0.001 && vfd->pts < clock - STALE_FRAME_DROP) {
            renStaleDrop++;
            delete vfd;   // 落后过多直接丢帧
            continue;
        }
        int64_t w0 = av_gettime();
        waitUntilClock(vfd->pts, LIVE_MAX_WAIT_US);
        waitUsTotal += av_gettime() - w0;
        renderFrame(vfd->frame, &stage);
        renLastPts = vfd->pts;
        renCount++;
        delete vfd;
    }
    LOGI("RTSP video render thread finished");
}

// ---------------- 视频硬解 ----------------

bool RtspPlayer::initHardDecoder() {
    pthread_mutex_lock(&mVideoCodecMutex);
    bool ok = mHardDecoder.init(mHardMime.c_str(), mWidth, mHeight,
                                mSpsData.empty() ? nullptr : mSpsData.data(),
                                mSpsData.size(),
                                mPpsData.empty() ? nullptr : mPpsData.data(),
                                mPpsData.size(),
                                mNativeWindow);
    pthread_mutex_unlock(&mVideoCodecMutex);
    return ok;
}

void *RtspPlayer::videoHardThreadWrapper(void *ctx) {
    static_cast<RtspPlayer *>(ctx)->videoHardThread();
    return nullptr;
}

void RtspPlayer::videoHardThread() {
    LOGI("RTSP video decode(hard) thread started");
    if (!initHardDecoder()) {
        mHardFailed.store(true);
        PostStatusMessage("MediaCodec 硬解启动失败，回退 FFmpeg 软解\n");
        // 动态切换到软解管线
        pthread_mutex_lock(&mVideoCodecMutex);
        bool opened = openVideoCodecSoft();
        pthread_mutex_unlock(&mVideoCodecMutex);
        if (!opened) {
            LOGE("soft fallback failed");
            return;
        }
        pthread_create(&mVideoDecodeThread, nullptr, videoDecodeThreadWrapper, this);
        pthread_create(&mVideoRenderThread, nullptr, videoRenderThreadWrapper, this);
        return;
    }
    mHardInited.store(true);
    PostStatusMessage("AMediaCodec 硬解已启动\n");

    int lastGen = mSessionGen.load();
    AVPacket *pkt = nullptr;
    int64_t lastStatUs = av_gettime();
    int hardOutCount = 0;
    double hardLastPts = -1;
    while (!mStopFlag.load()) {
        {   // 每 5s 输出一次管线状态，定位卡点
            int64_t nowUs = av_gettime();
            if (nowUs - lastStatUs > 5 * 1000000) {
                lastStatUs = nowUs;
                LOGI("HARD-STAT clock=%.2f lastPts=%.2f vpQ=%zu apQ=%zu drop=%d outFrames=%d",
                     getMasterClock(), hardLastPts, mVideoPackets.size(), mAudioPackets.size(),
                     mVideoDropping ? 1 : 0, hardOutCount);
                hardOutCount = 0;
            }
        }
        if (lastGen != mSessionGen.load()) {
            lastGen = mSessionGen.load();
            mHardDecoder.flush();
            if (pkt) {
                av_packet_free(&pkt);
                pkt = nullptr;
            }
        }
        if (mReopening.load()) {
            usleep(10 * 1000);
            continue;
        }

        if (!pkt) {
            if (!mVideoPackets.pop(pkt, std::chrono::milliseconds(10)) || !pkt) {
                pkt = nullptr;
            }
        }
        if (pkt) {
            int64_t ptsUs = pkt->pts != AV_NOPTS_VALUE
                            ? av_rescale_q(pkt->pts, mVideoTimeBase, AV_TIME_BASE_Q) : 0;
            if (mHardDecoder.queueData(pkt->data, pkt->size, ptsUs, 0)) {
                av_packet_free(&pkt);
                pkt = nullptr;
            } else {
                usleep(3 * 1000);   // 输入缓冲暂不可用，保留包稍后再喂
            }
        }

        AMediaCodecBufferInfo info;
        ssize_t outIndex;
        while (!mStopFlag.load()) {
            outIndex = mHardDecoder.dequeueOutput(&info, 0);
            if (outIndex >= 0) {
                double ptsSec = (double) info.presentationTimeUs / 1000000.0;
                hardLastPts = ptsSec;
                hardOutCount++;
                double clock = getMasterClock();
                if (clock > 0.001 && ptsSec < clock - STALE_FRAME_DROP) {
                    mHardDecoder.releaseOutput(outIndex, false);
                } else {
                    waitUntilClock(ptsSec, LIVE_MAX_WAIT_US);
                    mHardDecoder.releaseOutput(outIndex, true);
                }
            } else if (outIndex == RtspHardDecoder::kFormatChanged) {
                int w = 0, h = 0;
                mHardDecoder.onFormatChanged(outIndex, w, h);
                infoMsg = "硬解输出格式: " + to_string(w) + "x" + to_string(h) + "\n";
                PostStatusMessage(infoMsg.c_str());
            } else {
                break;   // TRY_AGAIN_LATER / 其他
            }
        }
    }

    if (pkt) {
        av_packet_free(&pkt);
    }
    mHardDecoder.stop();
    LOGI("RTSP video decode(hard) thread finished");
}
