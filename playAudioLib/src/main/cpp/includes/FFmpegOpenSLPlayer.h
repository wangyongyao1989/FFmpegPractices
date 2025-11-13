//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/13.
//

#ifndef FFMPEGPRACTICE_FFMPEGOPENSLPLAYER_H
#define FFMPEGPRACTICE_FFMPEGOPENSLPLAYER_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <pthread.h>
#include <atomic>
#include <string>
#include <queue>
#include "BasicCommon.h"

class FFmpegOpenSLPlayer {
public:
    FFmpegOpenSLPlayer();

    ~FFmpegOpenSLPlayer();

    // 初始化播放器
    bool init(const std::string &filePath);

    // 开始播放
    bool start();

    // 暂停播放
    bool pause();

    // 停止播放
    void stop();

    // 获取播放状态
    bool isPlaying() const { return mIsPlaying; }

    bool isInitialized() const { return mInitialized; }

    // 获取音频信息
    int getSampleRate() const { return mSampleRate; }

    int getChannels() const { return mChannels; }

    int64_t getDuration() const { return mDuration; }

    int64_t getCurrentPosition() const { return mCurrentPosition; }

private:
    // OpenSL ES 对象
    SLObjectItf mEngineObj;
    SLEngineItf mEngine;
    SLObjectItf mOutputMixObj;
    SLObjectItf mPlayerObj;
    SLPlayItf mPlayer;
    SLAndroidSimpleBufferQueueItf mBufferQueue;

    // FFmpeg 相关
    AVFormatContext *mFormatContext;
    AVCodecContext *mCodecContext;
    SwrContext *mSwrContext;
    int mAudioStreamIndex;

    // 音频参数
    int mSampleRate;
    int mChannels;
    int64_t mDuration;
    AVSampleFormat mSampleFormat;

    // 播放状态
    std::atomic<bool> mIsPlaying;
    std::atomic<bool> mInitialized;
    std::atomic<bool> mStopRequested;

    // 缓冲区
    static const int NUM_BUFFERS = 4;
    static const int BUFFER_SIZE = 4096;
    uint8_t *mBuffers[NUM_BUFFERS];
    int mCurrentBuffer;

    // 位置跟踪
    std::atomic<int64_t> mCurrentPosition;

    // 线程同步
    pthread_t mDecodeThread;
    pthread_mutex_t mMutex;
    pthread_cond_t mBufferReadyCond;

    // 缓冲区状态
    bool mBufferFull;

    // 私有方法
    bool initFFmpeg(const std::string &filePath);

    bool initOpenSL();

    static void *decodeThreadWrapper(void *context);

    void decodeThread();

    static void bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

    void processBufferQueue();

    void cleanup();

    void cleanupFFmpeg();

    void cleanupOpenSL();
};


#endif //FFMPEGPRACTICE_FFMPEGOPENSLPLAYER_H
