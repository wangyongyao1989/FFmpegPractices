//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/12/6.
//

#ifndef FFMPEGPRACTICE_FFGLPLAYER_H
#define FFMPEGPRACTICE_FFGLPLAYER_H

#include <pthread.h>
#include <atomic>
#include <string>
#include "ThreadSafeQueue.h"
#include "BasicCommon.h"
#include "jni.h"
#include "android/native_window.h"
#include "android/native_window_jni.h"
#include "EGLSurfaceViewVideoRender.h"

class FFGLPlayer {
public:
    FFGLPlayer(JNIEnv *env, jobject thiz);

    ~FFGLPlayer();

    // 初始化播放器
    bool
    init(const string &filePath, const string &fragPath, const string &vertexPath, jobject surface);

    // 开始播放
    bool start();

    // 暂停播放
    bool pause();

    // 停止播放
    void stop();

    // 获取播放状态
    bool isPlaying() const { return mIsPlaying; }

    bool isInitialized() const { return mInitialized; }


private:
    string playMediaInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    int mWidth;
    int mHeight;
    uint8_t *mOutbuffer;
    jobject androidSurface = NULL;

    //  NativeWindow;
    ANativeWindow *mNativeWindow = nullptr;

    // EGLRender
    EGLSurfaceViewVideoRender *eglsurfaceViewRender;

    // FFmpeg 相关
    AVFormatContext *mFormatContext;
    AVCodecContext *mCodecContext;
    int mVideoStreamIndex;

    int64_t mDuration;
    AVSampleFormat mSampleFormat;

    // 播放状态
    std::atomic<bool> mIsPlaying;
    std::atomic<bool> mInitialized;
    std::atomic<bool> mStopRequested;

    // 视频帧队列
    ThreadSafeQueue<AVFrame *> videoFrameQueue;

    int maxVideoFrames = 50;

    // 线程同步
    pthread_t mDecodeThread;
    pthread_t mRenderThread;
    pthread_mutex_t mDecodeMutex;
    pthread_cond_t mBufferMaxCond;
    pthread_cond_t mRenderCond;
    pthread_mutex_t mRenderMutex;

    // 私有方法
    bool initFFmpeg(const std::string &filePath);

    bool initEGLRender(const string &fragPath, const string &vertexPath);

    static void *decodeThreadWrapper(void *context);

    void decodeThread();

    static void *renderVideoFrame(void *context);

    void renderVideoThread();

    int sendFrameDataToEGL(AVFrame *frame);

    int yuv420p_frame_to_buffer(AVFrame *frame, uint8_t **buffer, int *length);

    void cleanup();

    void cleanupFFmpeg();

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);
};


#endif //FFMPEGPRACTICE_FFGLPLAYER_H
