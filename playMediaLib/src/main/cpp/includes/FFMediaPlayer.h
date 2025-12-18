//
// Created by wangyao on 2025/12/13.
//

#ifndef FFMPEGPRACTICE_FFMEDIAPLAYER_H
#define FFMPEGPRACTICE_FFMEDIAPLAYER_H

#include <pthread.h>
#include <atomic>
#include <string>
#include "ThreadSafeQueue.h"
#include "BasicCommon.h"
#include "jni.h"
#include "android/native_window.h"
#include "android/native_window_jni.h"

#include "OpenslHelper.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>


// 音频帧结构
struct AudioFrame {
    AVFrame *frame;
    double pts;
    int64_t pos;

    AudioFrame() : frame(nullptr), pts(0), pos(0) {}

    ~AudioFrame() {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

// 视频帧结构
struct VideoFrame {
    AVFrame *frame;
    double pts;
    int64_t pos;

    VideoFrame() : frame(nullptr), pts(0), pos(0) {}

    ~VideoFrame() {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

// 音频信息
struct AudioInfo {
    int streamIndex;
    AVCodecContext *codecContext;
    SwrContext *swrContext;
    AVRational timeBase;
    // 音频参数
    int sampleRate;
    int channels;
    int bytesPerSample;
    AVChannelLayout *channelLayout;
    enum AVSampleFormat format;

    // 音频队列
    std::queue<AudioFrame *> audioQueue;
    pthread_mutex_t audioMutex;
    pthread_cond_t audioCond;
    int maxAudioFrames;

    // 时钟
    double clock;
    pthread_mutex_t clockMutex;

    AudioInfo() : streamIndex(-1), codecContext(nullptr), swrContext(nullptr),
                  sampleRate(0), channels(0), bytesPerSample(0),
                  channelLayout(nullptr), format(AV_SAMPLE_FMT_NONE),
                  maxAudioFrames(100), clock(0) {
        pthread_mutex_init(&audioMutex, nullptr);
        pthread_cond_init(&audioCond, nullptr);
        pthread_mutex_init(&clockMutex, nullptr);
    }

    ~AudioInfo() {
        pthread_mutex_destroy(&audioMutex);
        pthread_cond_destroy(&audioCond);
        pthread_mutex_destroy(&clockMutex);
    }
};

// 视频信息
struct VideoInfo {
    int streamIndex;
    AVCodecContext *codecContext;
    AVRational timeBase;
    int width;
    int height;

    // 视频队列
    std::queue<VideoFrame *> videoQueue;
    pthread_mutex_t videoMutex;
    pthread_cond_t videoCond;
    int maxVideoFrames;

    // 时钟
    double clock;
    pthread_mutex_t clockMutex;

    VideoInfo() : streamIndex(-1), codecContext(nullptr),
                  width(0), height(0), maxVideoFrames(30), clock(0) {
        pthread_mutex_init(&videoMutex, nullptr);
        pthread_cond_init(&videoCond, nullptr);
        pthread_mutex_init(&clockMutex, nullptr);
    }

    ~VideoInfo() {
        pthread_mutex_destroy(&videoMutex);
        pthread_cond_destroy(&videoCond);
        pthread_mutex_destroy(&clockMutex);
    }
};

class FFMediaPlayer {
public:
    FFMediaPlayer(JNIEnv *env, jobject thiz);

    ~FFMediaPlayer();

    bool init(const char *url, jobject surface);

    bool prepare();

    bool start();

    bool pause();

    bool stop();

    void release();

    int getState() const { return mState; }

    int64_t getDuration() const { return mDuration; }

    int64_t getCurrentPosition() const;

private:
    enum State {
        STATE_IDLE,
        STATE_INITIALIZED,
        STATE_PREPARING,
        STATE_PREPARED,
        STATE_STARTED,
        STATE_PAUSED,
        STATE_STOPPED,
        STATE_ERROR
    };

    string playAudioInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    OpenslHelper helper;

    // OpenSL ES回调
    void processBufferQueue();

    static void bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

    // OpenSL ES回调
    void audioCallback(SLAndroidSimpleBufferQueueItf bufferQueue);

    static void audioCallbackWrapper(SLAndroidSimpleBufferQueueItf bufferQueue, void *context);

    // 缓冲区
    static const int NUM_BUFFERS = 4;
    static const int BUFFER_SIZE = 4096;
    uint8_t *mBuffers[NUM_BUFFERS];
    int mCurrentBuffer;
    std::atomic<int> mQueuedBufferCount; // 跟踪已入队的缓冲区数量

    pthread_mutex_t mBufferMutex;
    pthread_cond_t mBufferReadyCond;

    int count;


    jobject androidSurface = NULL;
    //  NativeWindow;
    ANativeWindow *mNativeWindow = nullptr;
    SwsContext *mSwsContext = nullptr;
    AVFrame *mRgbFrame = nullptr;
    uint8_t *mOutbuffer = nullptr;

    // 成员变量
    State mState;
    char *mUrl;
    AVFormatContext *mFormatContext;
    AudioInfo mAudioInfo;
    VideoInfo mVideoInfo;

    // 线程
    pthread_t mDemuxThread;
    pthread_t mAudioDecodeThread;
    pthread_t mVideoDecodeThread;
    pthread_t mAudioPlayThread;
    pthread_t mVideoPlayThread;

    // 控制变量
    std::atomic<bool> mExit;
    std::atomic<bool> mPause;
    int64_t mDuration;

    // 同步变量
    pthread_mutex_t mStateMutex;
    pthread_cond_t mStateCond;

    // 数据包队列
    std::queue<AVPacket *> mAudioPackets;
    std::queue<AVPacket *> mVideoPackets;
    pthread_mutex_t mPacketMutex;
    pthread_cond_t mPacketCond;
    int mMaxPackets;

    // 私有方法
    bool openCodecContext(int *streamIndex, AVCodecContext **codecContext,
                          AVFormatContext *formatContext, enum AVMediaType type);

    bool initOpenSLES();

    bool initANativeWindow();

    void cleanupANativeWindow();

    bool initVideoRenderer();

    // 线程函数
    static void *demuxThread(void *arg);

    static void *audioDecodeThread(void *arg);

    static void *videoDecodeThread(void *arg);

    static void *audioPlayThread(void *arg);

    static void *videoPlayThread(void *arg);

    void demux();

    void audioDecode();

    void videoDecode();

    void audioPlay();

    void videoPlay();

    // 数据处理
    void processAudioPacket(AVPacket *packet);

    void processVideoPacket(AVPacket *packet);

    AudioFrame *decodeAudioFrame(AVFrame *frame);

    VideoFrame *decodeVideoFrame(AVFrame *frame);

    void renderVideoFrame(VideoFrame *vframe);

    // 队列操作
    void putAudioPacket(AVPacket *packet);

    void putVideoPacket(AVPacket *packet);

    AVPacket *getAudioPacket();

    AVPacket *getVideoPacket();

    void clearAudioPackets();

    void clearVideoPackets();

    void putAudioFrame(AudioFrame *frame);

    void putVideoFrame(VideoFrame *frame);

    AudioFrame *getAudioFrame();

    VideoFrame *getVideoFrame();

    void clearAudioFrames();

    void clearVideoFrames();

    // 同步方法
    double getMasterClock();

    double getAudioClock();

    double getVideoClock();

    void setAudioClock(double pts);

    void setVideoClock(double pts);

    void syncVideo(double pts);


    int64_t getCurrentPosition();

    const char *getSLErrorString(SLresult result);

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);
};


#endif //FFMPEGPRACTICE_FFMEDIAPLAYER_H
