//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/20.
//

#ifndef FFMPEGPRACTICE_FFSURFACEPLAYER_H
#define FFMPEGPRACTICE_FFSURFACEPLAYER_H

#include <pthread.h>
#include <atomic>
#include <string>
#include <queue>
#include "BasicCommon.h"
#include "jni.h"
#include "android/native_window.h"
#include "android/native_window_jni.h"

using namespace std;

class FFSurfacePlayer {
public:
    FFSurfacePlayer(JNIEnv *env, jobject thiz);

    ~FFSurfacePlayer();

    // 初始化播放器
    bool init(const std::string &filePath, jobject surface);

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
    SwsContext *mSwsContext = nullptr;
    AVFrame *mRgbFrame = nullptr;

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


    bool initANativeWindow();

    static void *decodeThreadWrapper(void *context);

    void decodeThread();

    int sendFrameDataToANativeWindow(AVFrame *frame);

    void processBufferQueue();

    void cleanup();

    void cleanupFFmpeg();

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

};


#endif //FFMPEGPRACTICE_FFSURFACEPLAYER_H
