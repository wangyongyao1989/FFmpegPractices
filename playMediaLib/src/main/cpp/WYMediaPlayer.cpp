//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/18.
//

#include "includes/WYMediaPlayer.h"
#include <unistd.h>




WYMediaPlayer::WYMediaPlayer()
        : mState(STATE_IDLE),
          mUrl(nullptr),
          mFormatContext(nullptr),
          mExit(false),
          mPause(false),
          mDuration(0),
          mMaxPackets(50) {

    pthread_mutex_init(&mStateMutex, nullptr);
    pthread_cond_init(&mStateCond, nullptr);
    pthread_mutex_init(&mPacketMutex, nullptr);
    pthread_cond_init(&mPacketCond, nullptr);
}

WYMediaPlayer::~WYMediaPlayer() {
    release();
    pthread_mutex_destroy(&mStateMutex);
    pthread_cond_destroy(&mStateCond);
    pthread_mutex_destroy(&mPacketMutex);
    pthread_cond_destroy(&mPacketCond);
}

void WYMediaPlayer::setSurface(ANativeWindow* window) {
    if (mVideoInfo.window) {
        ANativeWindow_release(mVideoInfo.window);
    }
    mVideoInfo.window = window;
    if (window) {
        ANativeWindow_acquire(window);
    }
}

bool WYMediaPlayer::setDataSource(const char* url) {
    if (mState != STATE_IDLE) {
        LOGE("setDataSource called in invalid state: %d", mState);
        return false;
    }

    mUrl = strdup(url);
    mState = STATE_INITIALIZED;
    return true;
}

bool WYMediaPlayer::prepare() {
    if (mState != STATE_INITIALIZED) {
        LOGE("prepare called in invalid state: %d", mState);
        return false;
    }

    mState = STATE_PREPARING;

//    // 初始化FFmpeg
//    av_register_all();

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
        mAudioInfo.channelLayout = mAudioInfo.codecContext->channel_layout;
        mAudioInfo.format = mAudioInfo.codecContext->sample_fmt;

        if (mAudioInfo.channelLayout == 0) {
            mAudioInfo.channelLayout = av_get_default_channel_layout(mAudioInfo.channels);
        }

        // 初始化重采样上下文
        mAudioInfo.swrContext = swr_alloc_set_opts(nullptr,
                                                   mAudioInfo.channelLayout,
                                                   AV_SAMPLE_FMT_S16,
                                                   mAudioInfo.sampleRate,
                                                   mAudioInfo.channelLayout,
                                                   mAudioInfo.format,
                                                   mAudioInfo.sampleRate,
                                                   0, nullptr);
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
        if (mVideoInfo.window) {
            initVideoRenderer();
        }
    }

    mDuration = mFormatContext->duration / AV_TIME_BASE;
    mState = STATE_PREPARED;

    LOGI("Media player prepared successfully");
    return true;
}

bool WYMediaPlayer::start() {
    if (mState != STATE_PREPARED && mState != STATE_PAUSED) {
        LOGE("start called in invalid state: %d", mState);
        return false;
    }

    mExit = false;
    mPause = false;

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
    if (mAudioInfo.playerPlay) {
        (*mAudioInfo.playerPlay)->SetPlayState(mAudioInfo.playerPlay, SL_PLAYSTATE_PLAYING);
    }

    mState = STATE_STARTED;
    return true;
}

bool WYMediaPlayer::pause() {
    if (mState != STATE_STARTED) {
        LOGE("pause called in invalid state: %d", mState);
        return false;
    }

    mPause = true;

    if (mAudioInfo.playerPlay) {
        (*mAudioInfo.playerPlay)->SetPlayState(mAudioInfo.playerPlay, SL_PLAYSTATE_PAUSED);
    }

    mState = STATE_PAUSED;
    return true;
}

bool WYMediaPlayer::stop() {
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

    if (mAudioInfo.playerPlay) {
        (*mAudioInfo.playerPlay)->SetPlayState(mAudioInfo.playerPlay, SL_PLAYSTATE_STOPPED);
    }

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

    mState = STATE_STOPPED;
    return true;
}

void WYMediaPlayer::release() {
    if (mState == STATE_IDLE || mState == STATE_ERROR) {
        return;
    }

    stop();

    // 释放音频资源
    if (mAudioInfo.playerObject) {
        (*mAudioInfo.playerObject)->Destroy(mAudioInfo.playerObject);
        mAudioInfo.playerObject = nullptr;
        mAudioInfo.playerPlay = nullptr;
        mAudioInfo.playerBufferQueue = nullptr;
    }

    if (mAudioInfo.outputMixObject) {
        (*mAudioInfo.outputMixObject)->Destroy(mAudioInfo.outputMixObject);
        mAudioInfo.outputMixObject = nullptr;
    }

    if (mAudioInfo.engineObject) {
        (*mAudioInfo.engineObject)->Destroy(mAudioInfo.engineObject);
        mAudioInfo.engineObject = nullptr;
        mAudioInfo.engineEngine = nullptr;
    }

    if (mAudioInfo.swrContext) {
        swr_free(&mAudioInfo.swrContext);
    }

    // 释放视频资源
    if (mVideoInfo.window) {
        ANativeWindow_release(mVideoInfo.window);
        mVideoInfo.window = nullptr;
    }

    if (mVideoInfo.swsContext) {
        sws_freeContext(mVideoInfo.swsContext);
        mVideoInfo.swsContext = nullptr;
    }

    if (mVideoInfo.rgbFrame) {
        av_frame_free(&mVideoInfo.rgbFrame);
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

int64_t WYMediaPlayer::getCurrentPosition() {
    return (int64_t)(getMasterClock() * 1000); // 转换为毫秒
}

// 线程函数实现
void* WYMediaPlayer::demuxThread(void* arg) {
    WYMediaPlayer* player = static_cast<WYMediaPlayer*>(arg);
    player->demux();
    return nullptr;
}

void* WYMediaPlayer::audioDecodeThread(void* arg) {
    WYMediaPlayer* player = static_cast<WYMediaPlayer*>(arg);
    player->audioDecode();
    return nullptr;
}

void* WYMediaPlayer::videoDecodeThread(void* arg) {
    WYMediaPlayer* player = static_cast<WYMediaPlayer*>(arg);
    player->videoDecode();
    return nullptr;
}

void* WYMediaPlayer::audioPlayThread(void* arg) {
    WYMediaPlayer* player = static_cast<WYMediaPlayer*>(arg);
    player->audioPlay();
    return nullptr;
}

void* WYMediaPlayer::videoPlayThread(void* arg) {
    WYMediaPlayer* player = static_cast<WYMediaPlayer*>(arg);
    player->videoPlay();
    return nullptr;
}

void WYMediaPlayer::demux() {
    AVPacket* packet = av_packet_alloc();

    while (!mExit) {
        if (mPause) {
            usleep(10000); // 暂停时睡眠10ms
            continue;
        }

        // 控制包队列大小，避免内存占用过大
        pthread_mutex_lock(&mPacketMutex);
        if (mAudioPackets.size() + mVideoPackets.size() > mMaxPackets) {
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
        AVPacket* endPacket = av_packet_alloc();
        endPacket->data = nullptr;
        endPacket->size = 0;
        putAudioPacket(endPacket);
    }

    AVPacket* endPacket = av_packet_alloc();
    endPacket->data = nullptr;
    endPacket->size = 0;
    putVideoPacket(endPacket);
}

void WYMediaPlayer::audioDecode() {
    while (!mExit) {
        if (mPause) {
            usleep(10000);
            continue;
        }

        AVPacket* packet = getAudioPacket();
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
    AVPacket* flushPacket = av_packet_alloc();
    flushPacket->data = nullptr;
    flushPacket->size = 0;
    processAudioPacket(flushPacket);
    av_packet_free(&flushPacket);
}

void WYMediaPlayer::videoDecode() {
    while (!mExit) {
        if (mPause) {
            usleep(10000);
            continue;
        }

        AVPacket* packet = getVideoPacket();
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
    AVPacket* flushPacket = av_packet_alloc();
    flushPacket->data = nullptr;
    flushPacket->size = 0;
    processVideoPacket(flushPacket);
    av_packet_free(&flushPacket);
}

void WYMediaPlayer::audioPlay() {
    // 音频播放主要由OpenSL ES回调驱动
    // 这里主要处理音频队列管理和时钟更新
    while (!mExit) {
        if (mPause) {
            usleep(10000);
            continue;
        }

        // 控制音频队列大小
        pthread_mutex_lock(&mAudioInfo.audioMutex);
        if (mAudioInfo.audioQueue.size() > mAudioInfo.maxAudioFrames) {
            pthread_cond_wait(&mAudioInfo.audioCond, &mAudioInfo.audioMutex);
        }
        pthread_mutex_unlock(&mAudioInfo.audioMutex);

        usleep(5000); // 减少CPU占用
    }
}

void WYMediaPlayer::videoPlay() {
    while (!mExit) {
        if (mPause) {
            usleep(10000);
            continue;
        }

        VideoFrame* vframe = getVideoFrame();
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
void WYMediaPlayer::processAudioPacket(AVPacket* packet) {
    AVFrame* frame = av_frame_alloc();
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

        AudioFrame* aframe = decodeAudioFrame(frame);
        if (aframe) {
            putAudioFrame(aframe);
        }

        av_frame_unref(frame);
    }

    av_frame_free(&frame);
}

void WYMediaPlayer::processVideoPacket(AVPacket* packet) {
    AVFrame* frame = av_frame_alloc();
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

        VideoFrame* vframe = decodeVideoFrame(frame);
        if (vframe) {
            putVideoFrame(vframe);
        }

        av_frame_unref(frame);
    }

    av_frame_free(&frame);
}

AudioFrame* WYMediaPlayer::decodeAudioFrame(AVFrame* frame) {
    if (!frame || !mAudioInfo.swrContext) {
        return nullptr;
    }

    // 计算PTS
    double pts = frame->best_effort_timestamp;
    if (pts != AV_NOPTS_VALUE) {
        pts *= av_q2d(mAudioInfo.timeBase);
        setAudioClock(pts);
    }

    // 重采样参数
    int dst_nb_samples = av_rescale_rnd(
            swr_get_delay(mAudioInfo.swrContext, frame->sample_rate) + frame->nb_samples,
            mAudioInfo.sampleRate, frame->sample_rate, AV_ROUND_UP);

    // 计算输出缓冲区大小
    int dst_buffer_size = av_samples_get_buffer_size(
            nullptr, mAudioInfo.channels, dst_nb_samples, AV_SAMPLE_FMT_S16, 1);

    if (dst_buffer_size < 0) {
        return nullptr;
    }

    // 分配输出缓冲区
    uint8_t* dst_data = (uint8_t*)av_malloc(dst_buffer_size);
    if (!dst_data) {
        return nullptr;
    }

    // 重采样
    int ret = swr_convert(mAudioInfo.swrContext, &dst_data, dst_nb_samples,
                          (const uint8_t**)frame->data, frame->nb_samples);
    if (ret < 0) {
        av_free(dst_data);
        return nullptr;
    }

    // 创建音频帧
    AudioFrame* aframe = new AudioFrame();
    aframe->data = dst_data;
    aframe->size = dst_buffer_size;
    aframe->pts = pts;
//    aframe->pos = av_frame_get_pkt_pos(frame);

    return aframe;
}

VideoFrame* WYMediaPlayer::decodeVideoFrame(AVFrame* frame) {
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
    AVFrame* frameCopy = av_frame_alloc();
    if (av_frame_ref(frameCopy, frame) < 0) {
        av_frame_free(&frameCopy);
        return nullptr;
    }

    VideoFrame* vframe = new VideoFrame();
    vframe->frame = frameCopy;
    vframe->pts = pts;
//    vframe->pos = av_frame_get_pkt_pos(frame);

    return vframe;
}

void WYMediaPlayer::renderVideoFrame(VideoFrame* vframe) {
    if (!mVideoInfo.window || !vframe->frame) {
        return;
    }

    // 设置窗口尺寸
    ANativeWindow_setBuffersGeometry(mVideoInfo.window,
                                     mVideoInfo.width,
                                     mVideoInfo.height,
                                     WINDOW_FORMAT_RGBA_8888);

    // 锁定窗口
    if (ANativeWindow_lock(mVideoInfo.window, &mVideoInfo.windowBuffer, nullptr) < 0) {
        LOGE("Could not lock native window");
        return;
    }

    // 初始化转换上下文
    if (!mVideoInfo.swsContext) {
        mVideoInfo.swsContext = sws_getContext(
                vframe->frame->width, vframe->frame->height,
                (AVPixelFormat)vframe->frame->format,
                mVideoInfo.width, mVideoInfo.height, AV_PIX_FMT_RGBA,
                SWS_BILINEAR, nullptr, nullptr, nullptr);
    }

    // 初始化RGB帧
    if (!mVideoInfo.rgbFrame) {
        mVideoInfo.rgbFrame = av_frame_alloc();
        mVideoInfo.rgbFrame->format = AV_PIX_FMT_RGBA;
        mVideoInfo.rgbFrame->width = mVideoInfo.width;
        mVideoInfo.rgbFrame->height = mVideoInfo.height;
        av_frame_get_buffer(mVideoInfo.rgbFrame, 32);
    }

    // 转换YUV到RGBA
    sws_scale(mVideoInfo.swsContext,
              vframe->frame->data, vframe->frame->linesize,
              0, vframe->frame->height,
              mVideoInfo.rgbFrame->data, mVideoInfo.rgbFrame->linesize);

    // 复制到窗口缓冲区
    uint8_t* dst = (uint8_t*)mVideoInfo.windowBuffer.bits;
    uint8_t* src = mVideoInfo.rgbFrame->data[0];
    int dstStride = mVideoInfo.windowBuffer.stride * 4;
    int srcStride = mVideoInfo.rgbFrame->linesize[0];

    for (int i = 0; i < mVideoInfo.height; i++) {
        memcpy(dst + i * dstStride, src + i * srcStride, srcStride);
    }

    // 解锁并显示
    ANativeWindow_unlockAndPost(mVideoInfo.window);
}

// 队列操作方法实现
void WYMediaPlayer::putAudioPacket(AVPacket* packet) {
    pthread_mutex_lock(&mPacketMutex);
    mAudioPackets.push(packet);
    pthread_cond_signal(&mPacketCond);
    pthread_mutex_unlock(&mPacketMutex);
}

void WYMediaPlayer::putVideoPacket(AVPacket* packet) {
    pthread_mutex_lock(&mPacketMutex);
    mVideoPackets.push(packet);
    pthread_cond_signal(&mPacketCond);
    pthread_mutex_unlock(&mPacketMutex);
}

AVPacket* WYMediaPlayer::getAudioPacket() {
    pthread_mutex_lock(&mPacketMutex);
    while (mAudioPackets.empty() && !mExit) {
        pthread_cond_wait(&mPacketCond, &mPacketMutex);
    }

    if (mExit) {
        pthread_mutex_unlock(&mPacketMutex);
        return nullptr;
    }

    AVPacket* packet = mAudioPackets.front();
    mAudioPackets.pop();
    pthread_cond_signal(&mPacketCond);
    pthread_mutex_unlock(&mPacketMutex);

    return packet;
}

AVPacket* WYMediaPlayer::getVideoPacket() {
    pthread_mutex_lock(&mPacketMutex);
    while (mVideoPackets.empty() && !mExit) {
        pthread_cond_wait(&mPacketCond, &mPacketMutex);
    }

    if (mExit) {
        pthread_mutex_unlock(&mPacketMutex);
        return nullptr;
    }

    AVPacket* packet = mVideoPackets.front();
    mVideoPackets.pop();
    pthread_cond_signal(&mPacketCond);
    pthread_mutex_unlock(&mPacketMutex);

    return packet;
}

void WYMediaPlayer::clearAudioPackets() {
    pthread_mutex_lock(&mPacketMutex);
    while (!mAudioPackets.empty()) {
        AVPacket* packet = mAudioPackets.front();
        mAudioPackets.pop();
        av_packet_free(&packet);
    }
    pthread_mutex_unlock(&mPacketMutex);
}

void WYMediaPlayer::clearVideoPackets() {
    pthread_mutex_lock(&mPacketMutex);
    while (!mVideoPackets.empty()) {
        AVPacket* packet = mVideoPackets.front();
        mVideoPackets.pop();
        av_packet_free(&packet);
    }
    pthread_mutex_unlock(&mPacketMutex);
}

void WYMediaPlayer::putAudioFrame(AudioFrame* frame) {
    pthread_mutex_lock(&mAudioInfo.audioMutex);
    while (mAudioInfo.audioQueue.size() >= mAudioInfo.maxAudioFrames && !mExit) {
        pthread_cond_wait(&mAudioInfo.audioCond, &mAudioInfo.audioMutex);
    }

    if (!mExit) {
        mAudioInfo.audioQueue.push(frame);
    }
    pthread_cond_signal(&mAudioInfo.audioCond);
    pthread_mutex_unlock(&mAudioInfo.audioMutex);
}

void WYMediaPlayer::putVideoFrame(VideoFrame* frame) {
    pthread_mutex_lock(&mVideoInfo.videoMutex);
    while (mVideoInfo.videoQueue.size() >= mVideoInfo.maxVideoFrames && !mExit) {
        pthread_cond_wait(&mVideoInfo.videoCond, &mVideoInfo.videoMutex);
    }

    if (!mExit) {
        mVideoInfo.videoQueue.push(frame);
    }
    pthread_cond_signal(&mVideoInfo.videoCond);
    pthread_mutex_unlock(&mVideoInfo.videoMutex);
}

AudioFrame* WYMediaPlayer::getAudioFrame() {
    pthread_mutex_lock(&mAudioInfo.audioMutex);
    if (mAudioInfo.audioQueue.empty()) {
        pthread_mutex_unlock(&mAudioInfo.audioMutex);
        return nullptr;
    }

    AudioFrame* frame = mAudioInfo.audioQueue.front();
    mAudioInfo.audioQueue.pop();
    pthread_cond_signal(&mAudioInfo.audioCond);
    pthread_mutex_unlock(&mAudioInfo.audioMutex);

    return frame;
}

VideoFrame* WYMediaPlayer::getVideoFrame() {
    pthread_mutex_lock(&mVideoInfo.videoMutex);
    if (mVideoInfo.videoQueue.empty()) {
        pthread_mutex_unlock(&mVideoInfo.videoMutex);
        return nullptr;
    }

    VideoFrame* frame = mVideoInfo.videoQueue.front();
    mVideoInfo.videoQueue.pop();
    pthread_cond_signal(&mVideoInfo.videoCond);
    pthread_mutex_unlock(&mVideoInfo.videoMutex);

    return frame;
}

void WYMediaPlayer::clearAudioFrames() {
    pthread_mutex_lock(&mAudioInfo.audioMutex);
    while (!mAudioInfo.audioQueue.empty()) {
        AudioFrame* frame = mAudioInfo.audioQueue.front();
        mAudioInfo.audioQueue.pop();
        delete frame;
    }
    pthread_mutex_unlock(&mAudioInfo.audioMutex);
}

void WYMediaPlayer::clearVideoFrames() {
    pthread_mutex_lock(&mVideoInfo.videoMutex);
    while (!mVideoInfo.videoQueue.empty()) {
        VideoFrame* frame = mVideoInfo.videoQueue.front();
        mVideoInfo.videoQueue.pop();
        delete frame;
    }
    pthread_mutex_unlock(&mVideoInfo.videoMutex);
}

// 同步方法实现
double WYMediaPlayer::getMasterClock() {
    return getAudioClock(); // 以音频时钟为主时钟
}

double WYMediaPlayer::getAudioClock() {
    pthread_mutex_lock(&mAudioInfo.clockMutex);
    double clock = mAudioInfo.clock;
    pthread_mutex_unlock(&mAudioInfo.clockMutex);
    return clock;
}

double WYMediaPlayer::getVideoClock() {
    pthread_mutex_lock(&mVideoInfo.clockMutex);
    double clock = mVideoInfo.clock;
    pthread_mutex_unlock(&mVideoInfo.clockMutex);
    return clock;
}

void WYMediaPlayer::setAudioClock(double pts) {
    pthread_mutex_lock(&mAudioInfo.clockMutex);
    mAudioInfo.clock = pts;
    pthread_mutex_unlock(&mAudioInfo.clockMutex);
}

void WYMediaPlayer::setVideoClock(double pts) {
    pthread_mutex_lock(&mVideoInfo.clockMutex);
    mVideoInfo.clock = pts;
    pthread_mutex_unlock(&mVideoInfo.clockMutex);
}

void WYMediaPlayer::syncVideo(double pts) {
    double audioTime = getAudioClock();
    double videoTime = pts;
    double diff = videoTime - audioTime;

    // 同步阈值
    const double syncThreshold = 0.01;
    const double maxFrameDelay = 0.1;

    if (fabs(diff) < maxFrameDelay) {
        if (diff <= -syncThreshold) {
            // 视频落后，立即显示
            return;
        } else if (diff >= syncThreshold) {
            // 视频超前，延迟显示
            int delay = (int)(diff * 1000000); // 转换为微秒
            usleep(delay);
        }
    }
    // 差异太大，直接显示
}

// OpenSL ES初始化和其他辅助方法
bool WYMediaPlayer::openCodecContext(int* streamIndex, AVCodecContext** codecContext,
                                   AVFormatContext* formatContext, enum AVMediaType type) {
    int streamIdx = av_find_best_stream(formatContext, type, -1, -1, nullptr, 0);
    if (streamIdx < 0) {
        LOGE("Could not find %s stream", av_get_media_type_string(type));
        return false;
    }

    AVStream* stream = formatContext->streams[streamIdx];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
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

bool WYMediaPlayer::initOpenSLES() {
    SLresult result;

    // 创建引擎
    result = slCreateEngine(&mAudioInfo.engineObject, 0, nullptr, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to create engine: %d", result);
        return false;
    }

    result = (*mAudioInfo.engineObject)->Realize(mAudioInfo.engineObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to realize engine: %d", result);
        return false;
    }

    result = (*mAudioInfo.engineObject)->GetInterface(mAudioInfo.engineObject, SL_IID_ENGINE,
                                                      &mAudioInfo.engineEngine);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to get engine interface: %d", result);
        return false;
    }

    // 创建混音器
    result = (*mAudioInfo.engineEngine)->CreateOutputMix(mAudioInfo.engineEngine,
                                                         &mAudioInfo.outputMixObject, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to create output mix: %d", result);
        return false;
    }

    result = (*mAudioInfo.outputMixObject)->Realize(mAudioInfo.outputMixObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to realize output mix: %d", result);
        return false;
    }

    // 配置音频源
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2
    };

    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,
            static_cast<SLuint32>(mAudioInfo.channels),
            static_cast<SLuint32>(mAudioInfo.sampleRate * 1000), // 转换为毫赫兹
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            mAudioInfo.channels == 2 ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT) : SL_SPEAKER_FRONT_CENTER,
            SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataSource audioSrc = { &loc_bufq, &format_pcm };

    // 配置音频接收器
    SLDataLocator_OutputMix loc_outmix = { SL_DATALOCATOR_OUTPUTMIX, mAudioInfo.outputMixObject };
    SLDataSink audioSnk = { &loc_outmix, nullptr };

    // 创建音频播放器
    const SLInterfaceID ids[] = { SL_IID_BUFFERQUEUE };
    const SLboolean req[] = { SL_BOOLEAN_TRUE };

    result = (*mAudioInfo.engineEngine)->CreateAudioPlayer(mAudioInfo.engineEngine,
                                                           &mAudioInfo.playerObject,
                                                           &audioSrc, &audioSnk,
                                                           1, ids, req);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to create audio player: %d", result);
        return false;
    }

    result = (*mAudioInfo.playerObject)->Realize(mAudioInfo.playerObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to realize audio player: %d", result);
        return false;
    }

    // 获取播放接口
    result = (*mAudioInfo.playerObject)->GetInterface(mAudioInfo.playerObject, SL_IID_PLAY,
                                                      &mAudioInfo.playerPlay);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to get play interface: %d", result);
        return false;
    }

    // 获取缓冲区队列接口
    result = (*mAudioInfo.playerObject)->GetInterface(mAudioInfo.playerObject, SL_IID_BUFFERQUEUE,
                                                      &mAudioInfo.playerBufferQueue);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to get buffer queue interface: %d", result);
        return false;
    }

    // 注册回调函数
    result = (*mAudioInfo.playerBufferQueue)->RegisterCallback(mAudioInfo.playerBufferQueue,
                                                               audioCallbackWrapper, this);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to register buffer queue callback: %d", result);
        return false;
    }

    return true;
}

bool WYMediaPlayer::initVideoRenderer() {
    if (!mVideoInfo.window) {
        return false;
    }

    // 获取视频尺寸
    mVideoInfo.width = mVideoInfo.codecContext->width;
    mVideoInfo.height = mVideoInfo.codecContext->height;

    // 设置窗口尺寸
    ANativeWindow_setBuffersGeometry(mVideoInfo.window,
                                     mVideoInfo.width,
                                     mVideoInfo.height,
                                     WINDOW_FORMAT_RGBA_8888);

    return true;
}

void WYMediaPlayer::audioCallback(SLAndroidSimpleBufferQueueItf bufferQueue) {
    // 从音频队列获取一帧数据
    AudioFrame* aframe = getAudioFrame();
    if (aframe) {
        // 更新音频时钟
        setAudioClock(aframe->pts);

        // 将数据送入OpenSL ES
        (*bufferQueue)->Enqueue(bufferQueue, aframe->data, aframe->size);

        delete aframe;
    } else {
        // 队列为空，送入静音数据
        static uint8_t silence[4096] = {0};
        (*bufferQueue)->Enqueue(bufferQueue, silence, sizeof(silence));
    }
}

void WYMediaPlayer::audioCallbackWrapper(SLAndroidSimpleBufferQueueItf bufferQueue, void* context) {
    WYMediaPlayer* player = static_cast<WYMediaPlayer*>(context);
    player->audioCallback(bufferQueue);
}