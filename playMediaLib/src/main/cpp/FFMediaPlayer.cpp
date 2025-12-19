//
// Created by wangyao on 2025/12/13.
//

#include "includes/FFMediaPlayer.h"
#include <unistd.h>

FFMediaPlayer::FFMediaPlayer(JNIEnv *env, jobject thiz)
        : mState(STATE_IDLE),
          mUrl(nullptr),
          mFormatContext(nullptr),
          mExit(false),
          mPause(false),
          mDuration(0),
          mMaxPackets(50) {

    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);

    pthread_mutex_init(&mStateMutex, nullptr);
    pthread_cond_init(&mStateCond, nullptr);
    pthread_mutex_init(&mPacketMutex, nullptr);
    pthread_cond_init(&mPacketCond, nullptr);

    pthread_mutex_init(&mBufferMutex, nullptr);
    pthread_cond_init(&mBufferReadyCond, nullptr);

    // 初始化OpenSLES缓冲区
    for (int i = 0; i < NUM_BUFFERS; i++) {
        mBuffers[i] = new uint8_t[BUFFER_SIZE];
        memset(mBuffers[i], 0, BUFFER_SIZE); // 初始化为静音
    }
}

FFMediaPlayer::~FFMediaPlayer() {
    if (androidSurface) {
        mEnv->DeleteLocalRef(androidSurface);
    }
    if (mNativeWindow) {
        ANativeWindow_release(mNativeWindow);
        mNativeWindow = nullptr;
    }
    mEnv->DeleteGlobalRef(mJavaObj);
    release();
    pthread_mutex_destroy(&mStateMutex);
    pthread_cond_destroy(&mStateCond);
    pthread_mutex_destroy(&mPacketMutex);
    pthread_cond_destroy(&mPacketCond);
    pthread_mutex_destroy(&mBufferMutex);
    pthread_cond_destroy(&mBufferReadyCond);
}

bool FFMediaPlayer::init(const char *url, jobject surface) {

    androidSurface = mEnv->NewGlobalRef(surface);
    mUrl = strdup(url);
    mState = STATE_INITIALIZED;
    return true;
}


bool FFMediaPlayer::prepare() {
    LOGI("prepare()");
    playAudioInfo =
            "prepare() \n";
    PostStatusMessage(playAudioInfo.c_str());
    if (mState != STATE_INITIALIZED) {
        LOGE("prepare called in invalid state: %d", mState);
        return false;
    }

    mState = STATE_PREPARING;


    // 打开输入文件
    if (avformat_open_input(&mFormatContext, mUrl, nullptr, nullptr) < 0) {
        LOGE("Could not open input file: %s", mUrl);
        mState = STATE_ERROR;
        return false;
    }

    // 查找流信息
    if (avformat_find_stream_info(mFormatContext, nullptr) < 0) {
        LOGE("Could not find stream information");
        mState = STATE_ERROR;
        return false;
    }

    // 打开音频解码器
    if (!openCodecContext(&mAudioInfo.streamIndex,
                          &mAudioInfo.codecContext,
                          mFormatContext, AVMEDIA_TYPE_AUDIO)) {
        LOGE("Could not open audio codec");
        // 不是错误，可能没有音频流
    } else {
        // 初始化音频重采样
        mAudioInfo.sampleRate = mAudioInfo.codecContext->sample_rate;
        mAudioInfo.channels = mAudioInfo.codecContext->channels;
        mAudioInfo.channelLayout = &mAudioInfo.codecContext->ch_layout;
        mAudioInfo.format = mAudioInfo.codecContext->sample_fmt;

        // 配置音频重采样
        AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        swr_alloc_set_opts2(&mAudioInfo.swrContext,
                            &out_ch_layout, AV_SAMPLE_FMT_S16, mAudioInfo.sampleRate,
                            mAudioInfo.channelLayout, mAudioInfo.format, mAudioInfo.sampleRate,
                            0, NULL);

        if (!mAudioInfo.swrContext || swr_init(mAudioInfo.swrContext) < 0) {
            LOGE("Could not initialize swresample");
            mState = STATE_ERROR;
            return false;
        }

        // 初始化OpenSL ES
        if (!initOpenSLES()) {
            LOGE("Could not initialize OpenSL ES");
            mState = STATE_ERROR;
            return false;
        }
    }

    // 打开视频解码器
    if (!openCodecContext(&mVideoInfo.streamIndex,
                          &mVideoInfo.codecContext,
                          mFormatContext, AVMEDIA_TYPE_VIDEO)) {
        LOGE("Could not open video codec");
        mState = STATE_ERROR;
        return false;
    } else {
        // 初始化视频渲染
        initANativeWindow();
    }

    mDuration = mFormatContext->duration / AV_TIME_BASE;
    mState = STATE_PREPARED;

    LOGI("Media player prepared successfully");
    return true;
}

bool FFMediaPlayer::start() {
    if (mState != STATE_PREPARED && mState != STATE_PAUSED) {
        LOGE("start called in invalid state: %d", mState);
        return false;
    }

    mExit = false;
    mPause = false;


    // 重置状态
    mQueuedBufferCount = 0;
    mCurrentBuffer = 0;

    // 清空缓冲区队列
    if (helper.bufferQueueItf) {
        (*helper.bufferQueueItf)->Clear(helper.bufferQueueItf);
    }

    // 启动解复用线程
    if (pthread_create(&mDemuxThread, nullptr, demuxThread, this) != 0) {
        LOGE("Could not create demux thread");
        return false;
    }

    // 启动音频解码线程
    if (mAudioInfo.codecContext) {
        if (pthread_create(&mAudioDecodeThread, nullptr, audioDecodeThread, this) != 0) {
            LOGE("Could not create audio decode thread");
            return false;
        }

        // 启动音频播放线程
        if (pthread_create(&mAudioPlayThread, nullptr, audioPlayThread, this) != 0) {
            LOGE("Could not create audio play thread");
            return false;
        }
    }

    // 启动视频解码线程
    if (pthread_create(&mVideoDecodeThread, nullptr, videoDecodeThread, this) != 0) {
        LOGE("Could not create video decode thread");
        return false;
    }

    // 启动视频播放线程
    if (pthread_create(&mVideoPlayThread, nullptr, videoPlayThread, this) != 0) {
        LOGE("Could not create video play thread");
        return false;
    }

    // 启动音频播放
    SLresult result = helper.play();
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to set play state: %d, error: %s", result, getSLErrorString(result));
        playAudioInfo = "Failed to set play state: " + string(getSLErrorString(result));
        PostStatusMessage(playAudioInfo.c_str());
    }
    LOGE("helper.play() successfully");


    mState = STATE_STARTED;
    return true;
}

bool FFMediaPlayer::pause() {
    if (mState != STATE_STARTED) {
        LOGE("pause called in invalid state: %d", mState);
        return false;
    }

    mPause = true;

    SLresult result = helper.pause();
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to set play state: %d, error: %s", result, getSLErrorString(result));
        playAudioInfo = "Failed to set play state: " + string(getSLErrorString(result));
        PostStatusMessage(playAudioInfo.c_str());
    }

    mState = STATE_PAUSED;
    return true;
}

bool FFMediaPlayer::stop() {
    if (mState != STATE_STARTED && mState != STATE_PAUSED) {
        LOGE("stop called in invalid state: %d", mState);
        return false;
    }

    mExit = true;
    mPause = false;

    // 通知所有线程
    pthread_cond_broadcast(&mPacketCond);
    pthread_cond_broadcast(&mAudioInfo.audioCond);
    pthread_cond_broadcast(&mVideoInfo.videoCond);
    pthread_cond_broadcast(&mBufferReadyCond);

    cleanupANativeWindow();


    // 等待线程结束
    if (mAudioInfo.codecContext) {
        pthread_join(mAudioDecodeThread, nullptr);
        pthread_join(mAudioPlayThread, nullptr);
    }
    pthread_join(mVideoDecodeThread, nullptr);
    pthread_join(mVideoPlayThread, nullptr);
    pthread_join(mDemuxThread, nullptr);

    // 清空队列
    clearAudioPackets();
    clearVideoPackets();
    clearAudioFrames();
    clearVideoFrames();

    if (helper.player) {
        helper.stop();
    }

    // 清空缓冲区队列
    if (helper.bufferQueueItf) {
        (*helper.bufferQueueItf)->Clear(helper.bufferQueueItf);
    }

    // 清理缓冲区
    for (int i = 0; i < NUM_BUFFERS; i++) {
        delete[] mBuffers[i];
    }
    mState = STATE_STOPPED;
    return true;
}

void FFMediaPlayer::release() {
    if (mState == STATE_IDLE || mState == STATE_ERROR) {
        return;
    }
    stop();
    if (mAudioInfo.swrContext) {
        swr_free(&mAudioInfo.swrContext);
    }

    // 释放FFmpeg资源
    if (mAudioInfo.codecContext) {
        avcodec_free_context(&mAudioInfo.codecContext);
    }

    if (mVideoInfo.codecContext) {
        avcodec_free_context(&mVideoInfo.codecContext);
    }

    if (mFormatContext) {
        avformat_close_input(&mFormatContext);
        mFormatContext = nullptr;
    }

    if (mUrl) {
        free(mUrl);
        mUrl = nullptr;
    }

    mState = STATE_IDLE;
}

int64_t FFMediaPlayer::getCurrentPosition() {
    return (int64_t) (getMasterClock() * 1000); // 转换为毫秒
}

// 线程函数实现
void *FFMediaPlayer::demuxThread(void *arg) {
    FFMediaPlayer *player = static_cast<FFMediaPlayer *>(arg);
    player->demux();
    return nullptr;
}

void *FFMediaPlayer::audioDecodeThread(void *arg) {
    FFMediaPlayer *player = static_cast<FFMediaPlayer *>(arg);
    player->audioDecode();
    return nullptr;
}

void *FFMediaPlayer::videoDecodeThread(void *arg) {
    FFMediaPlayer *player = static_cast<FFMediaPlayer *>(arg);
    player->videoDecode();
    return nullptr;
}

void *FFMediaPlayer::audioPlayThread(void *arg) {
    FFMediaPlayer *player = static_cast<FFMediaPlayer *>(arg);
    player->audioPlay();
    return nullptr;
}

void *FFMediaPlayer::videoPlayThread(void *arg) {
    FFMediaPlayer *player = static_cast<FFMediaPlayer *>(arg);
    player->videoPlay();
    return nullptr;
}

void FFMediaPlayer::demux() {
    AVPacket *packet = av_packet_alloc();

    while (!mExit) {
        if (mPause) {
            usleep(10000); // 暂停时睡眠10ms
            continue;
        }

        // 控制包队列大小，避免内存占用过大
        pthread_mutex_lock(&mPacketMutex);
        if (mAudioPackets.size() + mVideoPackets.size() > mMaxPackets) {
//            LOGI("demux() Waiting for Packets slot, Packets: %d", mAudioPackets.size() + mVideoPackets.size());
            pthread_cond_wait(&mPacketCond, &mPacketMutex);
        }
        pthread_mutex_unlock(&mPacketMutex);

        if (av_read_frame(mFormatContext, packet) < 0) {
            break;
        }

        if (packet->stream_index == mAudioInfo.streamIndex) {
            putAudioPacket(packet);
            packet = av_packet_alloc(); // 分配新包，旧的已入队
        } else if (packet->stream_index == mVideoInfo.streamIndex) {
            putVideoPacket(packet);
            packet = av_packet_alloc(); // 分配新包，旧的已入队
        } else {
            av_packet_unref(packet);
        }
    }

    av_packet_free(&packet);

    // 发送结束信号
    if (mAudioInfo.codecContext) {
        AVPacket *endPacket = av_packet_alloc();
        endPacket->data = nullptr;
        endPacket->size = 0;
        putAudioPacket(endPacket);
    }

    AVPacket *endPacket = av_packet_alloc();
    endPacket->data = nullptr;
    endPacket->size = 0;
    putVideoPacket(endPacket);
}

void FFMediaPlayer::audioDecode() {
    while (!mExit) {
        if (mPause) {
            usleep(10000);
            continue;
        }

        AVPacket *packet = getAudioPacket();
        if (!packet) {
            continue;
        }

        // 检查结束包
        if (packet->data == nullptr && packet->size == 0) {
            av_packet_free(&packet);
            break;
        }

        processAudioPacket(packet);
        av_packet_free(&packet);
    }

    // 刷新解码器
    AVPacket *flushPacket = av_packet_alloc();
    flushPacket->data = nullptr;
    flushPacket->size = 0;
    processAudioPacket(flushPacket);
    av_packet_free(&flushPacket);
}

void FFMediaPlayer::videoDecode() {
    while (!mExit) {
        if (mPause) {
            usleep(10000);
            continue;
        }

        AVPacket *packet = getVideoPacket();
        if (!packet) {
            continue;
        }

        // 检查结束包
        if (packet->data == nullptr && packet->size == 0) {
            av_packet_free(&packet);
            break;
        }

        processVideoPacket(packet);
        av_packet_free(&packet);
    }

    // 刷新解码器
    AVPacket *flushPacket = av_packet_alloc();
    flushPacket->data = nullptr;
    flushPacket->size = 0;
    processVideoPacket(flushPacket);
    av_packet_free(&flushPacket);
}

void FFMediaPlayer::audioPlay() {
    // 音频播放主要由OpenSL ES回调驱动
    // 这里主要处理音频队列管理和时钟更新
    LOGW("audioPlay===============");
    while (!mExit) {
        if (mPause) {
            usleep(10000);
            continue;
        }
        // 等待直到有可用的缓冲区槽位
        pthread_mutex_lock(&mBufferMutex);

        while (mQueuedBufferCount >= NUM_BUFFERS) {
            pthread_cond_wait(&mBufferReadyCond, &mBufferMutex);
        }

        // 从音频队列获取一帧数据
        AudioFrame *aframe = getAudioFrame();
        if (aframe) {
            // 更新音频时钟
            setAudioClock(aframe->pts);

            // 重采样音频数据
            uint8_t *buffer = mBuffers[mCurrentBuffer];
            uint8_t *outBuffer = buffer;

            // 计算最大输出样本数
            int maxSamples = BUFFER_SIZE / (mAudioInfo.channels * 2);
            int outSamples = swr_convert(mAudioInfo.swrContext, &outBuffer, maxSamples,
                                         (const uint8_t **) aframe->frame->data,
                                         aframe->frame->nb_samples);

            if (outSamples > 0) {
                int bytesDecoded = outSamples * mAudioInfo.channels * 2;
                // 确保不超过缓冲区大小
                if (bytesDecoded > BUFFER_SIZE) {
                    LOGW("Decoded data exceeds buffer size: %d > %d", bytesDecoded,
                         BUFFER_SIZE);
                    bytesDecoded = BUFFER_SIZE;
                }

                // 检查是否还有可用的缓冲区槽位
                if (mQueuedBufferCount >= NUM_BUFFERS) {
                    LOGW("No buffer slots available, skipping frame");
                    pthread_mutex_unlock(&mBufferMutex);
                    break;
                }


                // 将缓冲区加入播放队列
                SLresult result = (*helper.bufferQueueItf)->Enqueue(helper.bufferQueueItf,
                                                                    buffer, bytesDecoded);

                if (result != SL_RESULT_SUCCESS) {
                    LOGE("Failed to enqueue buffer: %d, error: %s",
                         result, getSLErrorString(result));

                    if (result == SL_RESULT_BUFFER_INSUFFICIENT) {
                        // 等待一段时间后重试
                        pthread_mutex_unlock(&mBufferMutex);
                        usleep(10000); // 10ms
                        pthread_mutex_lock(&mBufferMutex);
                    }
                    break;
                } else {
                    // 成功入队，更新状态
                    mQueuedBufferCount++;
                    mCurrentBuffer = (mCurrentBuffer + 1) % NUM_BUFFERS;
                }
            } else if (outSamples < 0) {
                LOGE("swr_convert failed: %d", outSamples);
            }
            delete aframe;
        } else {
            // 队列为空，送入静音数据
            static uint8_t silence[4096] = {0};
            // 将缓冲区加入播放队列
            SLresult result = (*helper.bufferQueueItf)->Enqueue(helper.bufferQueueItf,
                                                                silence, sizeof(silence));

            if (result != SL_RESULT_SUCCESS) {
                LOGE("Failed to enqueue buffer: %d, error: %s",
                     result, getSLErrorString(result));

                if (result == SL_RESULT_BUFFER_INSUFFICIENT) {
                    // 等待一段时间后重试
                    pthread_mutex_unlock(&mBufferMutex);
                    usleep(10000); // 10ms
                    pthread_mutex_lock(&mBufferMutex);
                }
                break;
            } else {
                // 成功入队，更新状态
                mQueuedBufferCount++;
                mCurrentBuffer = (mCurrentBuffer + 1) % NUM_BUFFERS;
            }

        }
        pthread_mutex_unlock(&mBufferMutex);
        usleep(1000); // 减少CPU占用
    }
}

void FFMediaPlayer::videoPlay() {
    while (!mExit) {
        if (mPause) {
            usleep(10000);
            continue;
        }

        VideoFrame *vframe = getVideoFrame();
        if (!vframe) {
            usleep(10000);
            continue;
        }

        // 音视频同步
        syncVideo(vframe->pts);

        // 渲染视频
        renderVideoFrame(vframe);
        delete vframe;
    }
}

// 数据处理方法
void FFMediaPlayer::processAudioPacket(AVPacket *packet) {
    AVFrame *frame = av_frame_alloc();
    int ret = avcodec_send_packet(mAudioInfo.codecContext, packet);
    if (ret < 0) {
        av_frame_free(&frame);
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(mAudioInfo.codecContext, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            break;
        }

        AudioFrame *aframe = decodeAudioFrame(frame);
        if (aframe) {
            putAudioFrame(aframe);
        }

        av_frame_unref(frame);
    }

    av_frame_free(&frame);
}

void FFMediaPlayer::processVideoPacket(AVPacket *packet) {
    AVFrame *frame = av_frame_alloc();
    int ret = avcodec_send_packet(mVideoInfo.codecContext, packet);
    if (ret < 0) {
        av_frame_free(&frame);
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(mVideoInfo.codecContext, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            break;
        }

        VideoFrame *vframe = decodeVideoFrame(frame);
        if (vframe) {
            putVideoFrame(vframe);
        }

        av_frame_unref(frame);
    }

    av_frame_free(&frame);
}

AudioFrame *FFMediaPlayer::decodeAudioFrame(AVFrame *frame) {
    if (!frame || !mAudioInfo.swrContext) {
        return nullptr;
    }

    // 计算PTS
    double pts = frame->best_effort_timestamp;
    if (pts != AV_NOPTS_VALUE) {
        pts *= av_q2d(mAudioInfo.timeBase);
        setAudioClock(pts);
    }

    // 创建视频帧副本
    AVFrame *frameCopy = av_frame_alloc();
    if (av_frame_ref(frameCopy, frame) < 0) {
        av_frame_free(&frameCopy);
        return nullptr;
    }

    AudioFrame *aframe = new AudioFrame();
    aframe->frame = frameCopy;
    aframe->pts = pts;

    return aframe;
}

VideoFrame *FFMediaPlayer::decodeVideoFrame(AVFrame *frame) {
    if (!frame) {
        return nullptr;
    }

    // 计算PTS
    double pts = frame->best_effort_timestamp;
    if (pts != AV_NOPTS_VALUE) {
        pts *= av_q2d(mVideoInfo.timeBase);
        setVideoClock(pts);
    }

    // 创建视频帧副本
    AVFrame *frameCopy = av_frame_alloc();
    if (av_frame_ref(frameCopy, frame) < 0) {
        av_frame_free(&frameCopy);
        return nullptr;
    }

    VideoFrame *vframe = new VideoFrame();
    vframe->frame = frameCopy;
    vframe->pts = pts;
    return vframe;
}

void FFMediaPlayer::renderVideoFrame(VideoFrame *vframe) {
    if (!mNativeWindow || !vframe) {
        return;
    }

    ANativeWindow_Buffer windowBuffer;

    // 颜色空间转换
    sws_scale(mSwsContext, vframe->frame->data, vframe->frame->linesize, 0,
              mVideoInfo.height, mRgbFrame->data, mRgbFrame->linesize);

    // 锁定窗口缓冲区
    if (ANativeWindow_lock(mNativeWindow, &windowBuffer, nullptr) < 0) {
        LOGE("Cannot lock window");
        return;
    }

    // 优化拷贝：检查 stride 是否匹配
    uint8_t *dst = static_cast<uint8_t *>(windowBuffer.bits);
    int dstStride = windowBuffer.stride * 4;  // 目标步长（字节）
    int srcStride = mRgbFrame->linesize[0];   // 源步长（字节）

    if (dstStride == srcStride) {
        // 步长匹配，可以直接整体拷贝
        memcpy(dst, mOutbuffer, srcStride * mVideoInfo.height);
    } else {
        // 步长不匹配，需要逐行拷贝
        for (int h = 0; h < mVideoInfo.height; h++) {
            memcpy(dst + h * dstStride,
                   mOutbuffer + h * srcStride,
                   srcStride);
        }
    }

    ANativeWindow_unlockAndPost(mNativeWindow);
}

// 队列操作方法实现
void FFMediaPlayer::putAudioPacket(AVPacket *packet) {
    pthread_mutex_lock(&mPacketMutex);
    mAudioPackets.push(packet);
    pthread_cond_signal(&mPacketCond);
    pthread_mutex_unlock(&mPacketMutex);
}

void FFMediaPlayer::putVideoPacket(AVPacket *packet) {
    pthread_mutex_lock(&mPacketMutex);
    mVideoPackets.push(packet);
    pthread_cond_signal(&mPacketCond);
    pthread_mutex_unlock(&mPacketMutex);
}

AVPacket *FFMediaPlayer::getAudioPacket() {
    pthread_mutex_lock(&mPacketMutex);
    while (mAudioPackets.empty() && !mExit) {
        pthread_cond_wait(&mPacketCond, &mPacketMutex);
    }

    if (mExit) {
        pthread_mutex_unlock(&mPacketMutex);
        return nullptr;
    }

    AVPacket *packet = mAudioPackets.front();
    mAudioPackets.pop();
    pthread_cond_signal(&mPacketCond);
    pthread_mutex_unlock(&mPacketMutex);

    return packet;
}

AVPacket *FFMediaPlayer::getVideoPacket() {
    pthread_mutex_lock(&mPacketMutex);
    while (mVideoPackets.empty() && !mExit) {
        pthread_cond_wait(&mPacketCond, &mPacketMutex);
    }

    if (mExit) {
        pthread_mutex_unlock(&mPacketMutex);
        return nullptr;
    }

    AVPacket *packet = mVideoPackets.front();
    mVideoPackets.pop();
    pthread_cond_signal(&mPacketCond);
    pthread_mutex_unlock(&mPacketMutex);

    return packet;
}

void FFMediaPlayer::clearAudioPackets() {
    pthread_mutex_lock(&mPacketMutex);
    while (!mAudioPackets.empty()) {
        AVPacket *packet = mAudioPackets.front();
        mAudioPackets.pop();
        av_packet_free(&packet);
    }
    pthread_mutex_unlock(&mPacketMutex);
}

void FFMediaPlayer::clearVideoPackets() {
    pthread_mutex_lock(&mPacketMutex);
    while (!mVideoPackets.empty()) {
        AVPacket *packet = mVideoPackets.front();
        mVideoPackets.pop();
        av_packet_free(&packet);
    }
    pthread_mutex_unlock(&mPacketMutex);
}

void FFMediaPlayer::putAudioFrame(AudioFrame *frame) {
    pthread_mutex_lock(&mAudioInfo.audioMutex);
    while (mAudioInfo.audioQueue.size() >= mAudioInfo.maxAudioFrames && !mExit) {
//        LOGI("putAudioFrame Waiting for buffer slot, audioQueue: %d", mAudioInfo.audioQueue.size());
        pthread_cond_wait(&mAudioInfo.audioCond, &mAudioInfo.audioMutex);
    }

    if (!mExit) {
        mAudioInfo.audioQueue.push(frame);
    }
    pthread_cond_signal(&mAudioInfo.audioCond);
    pthread_mutex_unlock(&mAudioInfo.audioMutex);
}

void FFMediaPlayer::putVideoFrame(VideoFrame *frame) {
    pthread_mutex_lock(&mVideoInfo.videoMutex);
    while (mVideoInfo.videoQueue.size() >= mVideoInfo.maxVideoFrames && !mExit) {
//        LOGI("putVideoFrame Waiting for buffer slot, videoQueue: %d", mVideoInfo.videoQueue.size());
        pthread_cond_wait(&mVideoInfo.videoCond, &mVideoInfo.videoMutex);
    }

    if (!mExit) {
        mVideoInfo.videoQueue.push(frame);
    }
    pthread_cond_signal(&mVideoInfo.videoCond);
    pthread_mutex_unlock(&mVideoInfo.videoMutex);
}

AudioFrame *FFMediaPlayer::getAudioFrame() {
    pthread_mutex_lock(&mAudioInfo.audioMutex);
    if (mAudioInfo.audioQueue.empty()) {
        pthread_mutex_unlock(&mAudioInfo.audioMutex);
        return nullptr;
    }

    AudioFrame *frame = mAudioInfo.audioQueue.front();
    mAudioInfo.audioQueue.pop();
    pthread_cond_signal(&mAudioInfo.audioCond);
    pthread_mutex_unlock(&mAudioInfo.audioMutex);

    return frame;
}

VideoFrame *FFMediaPlayer::getVideoFrame() {
    pthread_mutex_lock(&mVideoInfo.videoMutex);
    if (mVideoInfo.videoQueue.empty()) {
        pthread_mutex_unlock(&mVideoInfo.videoMutex);
        return nullptr;
    }

    VideoFrame *frame = mVideoInfo.videoQueue.front();
    mVideoInfo.videoQueue.pop();
    pthread_cond_signal(&mVideoInfo.videoCond);
    pthread_mutex_unlock(&mVideoInfo.videoMutex);

    return frame;
}

void FFMediaPlayer::clearAudioFrames() {
    pthread_mutex_lock(&mAudioInfo.audioMutex);
    while (!mAudioInfo.audioQueue.empty()) {
        AudioFrame *frame = mAudioInfo.audioQueue.front();
        mAudioInfo.audioQueue.pop();
        delete frame;
    }
    pthread_mutex_unlock(&mAudioInfo.audioMutex);
}

void FFMediaPlayer::clearVideoFrames() {
    pthread_mutex_lock(&mVideoInfo.videoMutex);
    while (!mVideoInfo.videoQueue.empty()) {
        VideoFrame *frame = mVideoInfo.videoQueue.front();
        mVideoInfo.videoQueue.pop();
        delete frame;
    }
    pthread_mutex_unlock(&mVideoInfo.videoMutex);
}

// 同步方法实现
double FFMediaPlayer::getMasterClock() {
    return getAudioClock(); // 以音频时钟为主时钟
}

double FFMediaPlayer::getAudioClock() {
    pthread_mutex_lock(&mAudioInfo.clockMutex);
    double clock = mAudioInfo.clock;
    pthread_mutex_unlock(&mAudioInfo.clockMutex);
    return clock;
}

double FFMediaPlayer::getVideoClock() {
    pthread_mutex_lock(&mVideoInfo.clockMutex);
    double clock = mVideoInfo.clock;
    pthread_mutex_unlock(&mVideoInfo.clockMutex);
    return clock;
}

void FFMediaPlayer::setAudioClock(double pts) {
    pthread_mutex_lock(&mAudioInfo.clockMutex);
    mAudioInfo.clock = pts;
    pthread_mutex_unlock(&mAudioInfo.clockMutex);
}

void FFMediaPlayer::setVideoClock(double pts) {
    pthread_mutex_lock(&mVideoInfo.clockMutex);
    mVideoInfo.clock = pts;
    pthread_mutex_unlock(&mVideoInfo.clockMutex);
}

void FFMediaPlayer::syncVideo(double pts) {
    double audioTime = getAudioClock();
    double videoTime = pts;
    double diff = videoTime - audioTime;

    // 同步阈值
    const double syncThreshold = 0.01;  // 10ms同步阈值
    const double maxFrameDelay = 0.1;   // 最大100ms延迟
    LOGE("视频减音频的差值：%ld", diff);

    if (fabs(diff) < maxFrameDelay) {
        if (diff <= -syncThreshold) {
            // 视频落后，立即显示
            LOGW("视频落后，立即显示===============");
            playAudioInfo =
                    "视频落后，立即显示=============== \n";
            PostStatusMessage(playAudioInfo.c_str());
            return;
        } else if (diff >= syncThreshold) {
            // 视频超前，延迟显示
            int delay = (int) (diff * 1000000 ); // 转换为微秒
            LOGW("视频超前，延迟显示===============%d", delay);
            playAudioInfo =
                    "视频超前，延迟显示===============" + to_string(delay) + " \n";
            PostStatusMessage(playAudioInfo.c_str());
            usleep(delay);
        }
    }
    // 差异太大，直接显示
}

// OpenSL ES初始化和其他辅助方法
bool FFMediaPlayer::openCodecContext(int *streamIndex, AVCodecContext **codecContext,
                                     AVFormatContext *formatContext, enum AVMediaType type) {
    int streamIdx = av_find_best_stream(formatContext, type, -1, -1, nullptr, 0);
    if (streamIdx < 0) {
        LOGE("Could not find %s stream", av_get_media_type_string(type));
        return false;
    }

    AVStream *stream = formatContext->streams[streamIdx];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        LOGE("Could not find %s codec", av_get_media_type_string(type));
        return false;
    }

    *codecContext = avcodec_alloc_context3(codec);
    if (!*codecContext) {
        LOGE("Could not allocate %s codec context", av_get_media_type_string(type));
        return false;
    }

    if (avcodec_parameters_to_context(*codecContext, stream->codecpar) < 0) {
        LOGE("Could not copy %s codec parameters", av_get_media_type_string(type));
        return false;
    }

    if (avcodec_open2(*codecContext, codec, nullptr) < 0) {
        LOGE("Could not open %s codec", av_get_media_type_string(type));
        return false;
    }

    *streamIndex = streamIdx;

    // 设置时间基
    if (type == AVMEDIA_TYPE_AUDIO) {
        mAudioInfo.timeBase = stream->time_base;
    } else if (type == AVMEDIA_TYPE_VIDEO) {
        mVideoInfo.timeBase = stream->time_base;
    }

    return true;
}

bool FFMediaPlayer::initOpenSLES() {
    LOGI("initOpenSLES()");
    playAudioInfo =
            "initOpenSLES() \n";
    PostStatusMessage(playAudioInfo.c_str());
    // 创建 OpenSL 引擎与引擎接口
    SLresult result = helper.createEngine();
    if (!helper.isSuccess(result)) {
        LOGE("create engine error: %d", result);
        PostStatusMessage("Create engine error\n");
        return false;
    }
    PostStatusMessage("OpenSL createEngine Success \n");

    // 创建混音器与混音接口
    result = helper.createMix();
    if (!helper.isSuccess(result)) {
        LOGE("create mix error: %d", result);
        PostStatusMessage("Create mix error \n");
        return false;
    }
    PostStatusMessage("OpenSL createMix Success \n");

    result = helper.createPlayer(mAudioInfo.channels, mAudioInfo.sampleRate * 1000,
                                 SL_PCMSAMPLEFORMAT_FIXED_16,
                                 mAudioInfo.channels == 2 ? (SL_SPEAKER_FRONT_LEFT |
                                                             SL_SPEAKER_FRONT_RIGHT)
                                                          : SL_SPEAKER_FRONT_CENTER);
    if (!helper.isSuccess(result)) {
        LOGE("create player error: %d", result);
        PostStatusMessage("Create player error\n");
        return false;
    }
    PostStatusMessage("OpenSL createPlayer Success \n");

    // 注册回调并开始播放
    result = helper.registerCallback(bufferQueueCallback, this);
    if (!helper.isSuccess(result)) {
        LOGE("register callback error: %d", result);
        PostStatusMessage("Register callback error \n");
        return false;
    }
    PostStatusMessage("OpenSL registerCallback Success \n");

    // 清空缓冲区队列
    result = (*helper.bufferQueueItf)->Clear(helper.bufferQueueItf);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to clear buffer queue: %d, error: %s", result, getSLErrorString(result));
        playAudioInfo = "Failed to clear buffer queue:" + string(getSLErrorString(result));
        PostStatusMessage(playAudioInfo.c_str());
        return false;
    }

    LOGI("OpenSL ES initialized successfully: %d Hz, %d channels", mAudioInfo.sampleRate,
         mAudioInfo.channels);
    playAudioInfo =
            "OpenSL ES  initialized ,Hz:" + to_string(mAudioInfo.sampleRate) + ",channels:" +
            to_string(mAudioInfo.channels) + " ,duration:" + to_string(mDuration) + "\n";
    PostStatusMessage(playAudioInfo.c_str());
    return true;
}

void FFMediaPlayer::bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    FFMediaPlayer *player = static_cast<FFMediaPlayer *>(context);
    player->processBufferQueue();

}

void FFMediaPlayer::processBufferQueue() {
    pthread_mutex_lock(&mBufferMutex);
    // 缓冲区已播放完成，减少计数
    if (mQueuedBufferCount > 0) {
        mQueuedBufferCount--;
    }
    // 通知解码线程有可用的缓冲区槽位
    pthread_cond_signal(&mBufferReadyCond);
    pthread_mutex_unlock(&mBufferMutex);
}


bool FFMediaPlayer::initANativeWindow() {
    // 获取视频尺寸
    mVideoInfo.width = mVideoInfo.codecContext->width;
    mVideoInfo.height = mVideoInfo.codecContext->height;

    mNativeWindow = ANativeWindow_fromSurface(mEnv, androidSurface);
    if (!mNativeWindow) {
        LOGE("Couldn't get native window from surface");
        return false;
    }

    mRgbFrame = av_frame_alloc();
    if (!mRgbFrame) {
        LOGE("Could not allocate RGB frame");
        ANativeWindow_release(mNativeWindow);
        mNativeWindow = nullptr;
        return false;
    }

    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, mVideoInfo.width, mVideoInfo.height,
                                              1);
    mOutbuffer = (uint8_t *) av_malloc(bufferSize * sizeof(uint8_t));
    if (!mOutbuffer) {
        LOGE("Could not allocate output buffer");
        av_frame_free(&mRgbFrame);
        ANativeWindow_release(mNativeWindow);
        mNativeWindow = nullptr;
        return false;
    }

    mSwsContext = sws_getContext(mVideoInfo.width, mVideoInfo.height,
                                 mVideoInfo.codecContext->pix_fmt,
                                 mVideoInfo.width, mVideoInfo.height, AV_PIX_FMT_RGBA,
                                 SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!mSwsContext) {
        LOGE("Could not create sws context");
        av_free(mOutbuffer);
        mOutbuffer = nullptr;
        av_frame_free(&mRgbFrame);
        ANativeWindow_release(mNativeWindow);
        mNativeWindow = nullptr;
        return false;
    }

    if (ANativeWindow_setBuffersGeometry(mNativeWindow, mVideoInfo.width, mVideoInfo.height,
                                         WINDOW_FORMAT_RGBA_8888) < 0) {
        LOGE("Couldn't set buffers geometry");
        cleanupANativeWindow();
        return false;
    }

    if (av_image_fill_arrays(mRgbFrame->data, mRgbFrame->linesize,
                             mOutbuffer, AV_PIX_FMT_RGBA,
                             mVideoInfo.width, mVideoInfo.height, 1) < 0) {
        LOGE("Could not fill image arrays");
        cleanupANativeWindow();
        return false;
    }

    LOGI("ANativeWindow initialization successful");
    playAudioInfo =
            "ANativeWindow initialization successful \n";
    PostStatusMessage(playAudioInfo.c_str());
    return true;
}

void FFMediaPlayer::cleanupANativeWindow() {
    if (mSwsContext) {
        sws_freeContext(mSwsContext);
        mSwsContext = nullptr;
    }
    if (mRgbFrame) {
        av_frame_free(&mRgbFrame);
        mRgbFrame = nullptr;
    }
    if (mOutbuffer) {
        av_free(mOutbuffer);
        mOutbuffer = nullptr;
    }
    if (mNativeWindow) {
        ANativeWindow_release(mNativeWindow);
        mNativeWindow = nullptr;
    }
}


JNIEnv *FFMediaPlayer::GetJNIEnv(bool *isAttach) {
    if (!mJavaVm) {
        LOGE("GetJNIEnv mJavaVm == nullptr");
        return nullptr;
    }

    JNIEnv *env;
    int status = mJavaVm->GetEnv((void **) &env, JNI_VERSION_1_6);

    if (status == JNI_EDETACHED) {
        status = mJavaVm->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGE("Failed to attach current thread");
            return nullptr;
        }
        *isAttach = true;
    } else if (status != JNI_OK) {
        LOGE("Failed to get JNIEnv");
        return nullptr;
    } else {
        *isAttach = false;
    }

    return env;
}

void FFMediaPlayer::PostStatusMessage(const char *msg) {
    bool isAttach = false;
    JNIEnv *env = GetJNIEnv(&isAttach);
    if (!env) {
        return;
    }

    jmethodID mid = env->GetMethodID(env->GetObjectClass(mJavaObj),
                                     "CppStatusCallback", "(Ljava/lang/String;)V");
    if (mid) {
        jstring jMsg = env->NewStringUTF(msg);
        env->CallVoidMethod(mJavaObj, mid, jMsg);
        env->DeleteLocalRef(jMsg);
    }

    if (isAttach) {
        mJavaVm->DetachCurrentThread();
    }
}

// 添加这个辅助函数来获取错误描述
const char *FFMediaPlayer::getSLErrorString(SLresult result) {
    switch (result) {
        case SL_RESULT_SUCCESS:
            return "SL_RESULT_SUCCESS";
        case SL_RESULT_PRECONDITIONS_VIOLATED:
            return "SL_RESULT_PRECONDITIONS_VIOLATED";
        case SL_RESULT_PARAMETER_INVALID:
            return "SL_RESULT_PARAMETER_INVALID";
        case SL_RESULT_MEMORY_FAILURE:
            return "SL_RESULT_MEMORY_FAILURE";
        case SL_RESULT_RESOURCE_ERROR:
            return "SL_RESULT_RESOURCE_ERROR";
        case SL_RESULT_RESOURCE_LOST:
            return "SL_RESULT_RESOURCE_LOST";
        case SL_RESULT_IO_ERROR:
            return "SL_RESULT_IO_ERROR";
        case SL_RESULT_BUFFER_INSUFFICIENT:
            return "SL_RESULT_BUFFER_INSUFFICIENT";
        case SL_RESULT_CONTENT_CORRUPTED:
            return "SL_RESULT_CONTENT_CORRUPTED";
        case SL_RESULT_CONTENT_UNSUPPORTED:
            return "SL_RESULT_CONTENT_UNSUPPORTED";
        case SL_RESULT_CONTENT_NOT_FOUND:
            return "SL_RESULT_CONTENT_NOT_FOUND";
        case SL_RESULT_PERMISSION_DENIED:
            return "SL_RESULT_PERMISSION_DENIED";
        case SL_RESULT_FEATURE_UNSUPPORTED:
            return "SL_RESULT_FEATURE_UNSUPPORTED";
        case SL_RESULT_INTERNAL_ERROR:
            return "SL_RESULT_INTERNAL_ERROR";
        case SL_RESULT_UNKNOWN_ERROR:
            return "SL_RESULT_UNKNOWN_ERROR";
        case SL_RESULT_OPERATION_ABORTED:
            return "SL_RESULT_OPERATION_ABORTED";
        case SL_RESULT_CONTROL_LOST:
            return "SL_RESULT_CONTROL_LOST";
        default:
            return "Unknown error";
    }
}