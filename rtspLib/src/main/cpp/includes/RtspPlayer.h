//  Author : wangyao https://github.com/wangyongyao1989
// RTSP 拉流播放器：
//  - FFmpeg avformat 解封装（rtsp，强制 TCP 传输）
//  - 视频双路径：软件解码（avcodec + sws + ANativeWindow）/ 硬件解码（AMediaCodec NDK 直出 Surface）
//  - 音频统一路径：FFmpeg 解码 → swr 重采样 S16 → OpenSL ES buffer-queue 播放
//  - 音视频同步：音频主时钟策略（10ms 阈值 / 100ms 最大延迟），无音频流时退化为系统时钟
//  - 断流自动重连（最多 5 次），重连后冲刷队列并重置时钟

#ifndef FFMPEGPRACTICE_RTSP_PLAYER_H
#define FFMPEGPRACTICE_RTSP_PLAYER_H

#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <string>
#include <deque>
#include <inttypes.h>
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "BasicCommon.h"
#include "ThreadSafeQueue.h"
#include "OpenslHelper.h"
#include "RtspHardDecoder.h"

using namespace std;

// 解码后的 PCM 数据块
struct AudioChunk {
    uint8_t *data = nullptr;  // av_malloc，S16 交错
    int size = 0;
    double pts = 0;           // 数据块起始时间（秒，流时间轴）

    ~AudioChunk() {
        if (data) {
            av_freep(&data);
        }
    }
};

// 软渲染各阶段耗时累计（诊断用：定位 sws 缩放 / lock / 拷贝 / post 哪一段拖慢）
struct RenderStageStat {
    long long swsUs = 0;
    long long lockUs = 0;
    long long copyUs = 0;
    long long postUs = 0;
    long long frames = 0;   // 成功走完四段的帧数，作为均值分母
};

// 软解输出的视频帧
struct VideoFrameData {
    AVFrame *frame = nullptr;
    double pts = 0;

    ~VideoFrameData() {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

class RtspPlayer {
public:
    RtspPlayer(JNIEnv *env, jobject thiz);

    ~RtspPlayer();

    RtspPlayer(const RtspPlayer &) = delete;

    RtspPlayer &operator=(const RtspPlayer &) = delete;

    // surface 为 NewGlobalRef 的 Android Surface，play 内部转换 ANativeWindow 后释放
    bool play(const char *url, jobject surface, bool useHardDecode);

    // 任意线程可调用：置停止标志并唤醒所有队列消费者
    void requestStop();

    // 看门狗收尾：join 线程 + 清理资源 + 状态落位（幂等，仅一个调用者执行）
    void finishSession();

    bool isPlaying() const {
        int s = mState.load();
        return s == STATE_RUNNING || s == STATE_STOPPING;
    }

private:
    enum State {
        STATE_IDLE = 0,
        STATE_RUNNING,
        STATE_STOPPING,
        STATE_STOPPED,
        STATE_ERROR,
    };

    // 队列容量：live 流低延迟优先，队列从浅
    constexpr static int MAX_VIDEO_PACKETS = 60;
    constexpr static int MAX_AUDIO_PACKETS = 120;
    constexpr static int MAX_VIDEO_FRAMES = 5;
    constexpr static int MAX_AUDIO_CHUNKS = 10;
    constexpr static int NUM_BUFFERS = 4;
    constexpr static int PLAY_BUFFER_SIZE = 64 * 1024;
    constexpr static int RECONNECT_MAX = 5;
    // 软解 CPU 渲染上限：本次 FFmpeg 以 --disable-asm 编译，libswscale 走标量路径，
    // 实测 960x428 输出 sws_scale 需要 23~45ms/帧（跟不上 25fps），
    // 640 宽 ≈ 10ms/帧，SurfaceView 仍按 view 尺寸拉伸显示，观感可接受
    constexpr static int MAX_RENDER_WIDTH = 640;

    constexpr static double SYNC_THRESHOLD = 0.01;   // 10ms 同步阈值
    constexpr static double MAX_FRAME_DELAY = 0.1;   // 最大 100ms 延迟
    constexpr static double STALE_FRAME_DROP = 0.5;  // 落后 0.5s 的帧直接丢弃

    // JNI 回调
    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    // 会话建立
    bool openInput(const char *url);

    void closeInput();

    bool openAudioCodec();

    void closeAudioCodec();

    bool openVideoCodecSoft();

    void closeVideoCodecSoft();

    static int interruptCallback(void *ctx);

    // 线程
    static void *demuxThreadWrapper(void *ctx);

    static void *audioDecodeThreadWrapper(void *ctx);

    static void *audioPlayThreadWrapper(void *ctx);

    static void *videoDecodeThreadWrapper(void *ctx);

    static void *videoRenderThreadWrapper(void *ctx);

    static void *videoHardThreadWrapper(void *ctx);

    void demuxThread();

    void audioDecodeThread();

    void audioPlayThread();

    void videoDecodeThread();

    void videoRenderThread();

    void videoHardThread();

    // 重连：冲刷队列 + 重开输入/解码器 + 递增会话代
    bool reconnectSession();

    void drainPacketQueues();

    // 会话起点复位音频播放状态：OpenSL 缓冲计数、pts 环、音频主时钟。
    // play() 与 reconnectSession() 都必须调用，否则同一实例二次起播会继承上一会话的缓冲计数
    void resetAudioPipelineState();

    // 软解渲染
    bool prepareSwRenderer(int width, int height);

    void releaseSwRenderer();

    void renderFrame(AVFrame *frame, RenderStageStat *stat = nullptr);

    // 时钟
    double getMasterClock();

    // 直播渲染 pacing：等待主时钟最多 LIVE_MAX_WAIT_US，超时立即上屏。
    // 音频管线本身有 100~300ms 缓冲，主时钟会稳定落后画面 pts，
    // 等待过久会让软解渲染线程被饿死（实测 25fps 掉到 4fps）
    constexpr static int64_t LIVE_MAX_WAIT_US = 40 * 1000;
    void waitUntilClock(double targetPts, int64_t maxWaitUs = 2 * 1000000);

    // 硬解辅助
    bool initHardDecoder();

    void extractSpsPps(const uint8_t *extra, int extraSize,
                       vector<uint8_t> &sps, vector<uint8_t> &pps);

    static void extractSpsPpsFromAnnexB(const uint8_t *buf, int size,
                                        vector<uint8_t> &sps, vector<uint8_t> &pps);

    // OpenSL
    bool initOpenSL(int sampleRate, int channels);

    static void bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

    void processBufferQueue();

    void releaseOpenSL();

    // 入队并做容量控制（生产者限速）
    template<typename T>
    bool pushBounded(ThreadSafeQueue<T> &queue, T value, int cap) {
        while (!mStopFlag.load() && queue.size() >= (size_t) cap) {
            usleep(10 * 1000);
        }
        if (mStopFlag.load()) {
            delete value;
            return false;
        }
        return queue.push(value);
    }

    string infoMsg;
    JavaVM *mJavaVm = nullptr;
    JNIEnv *mEnv = nullptr;
    jobject mJavaObj = nullptr;

    char *mUrl = nullptr;
    std::atomic<bool> mUseHardDecode{false};
    std::atomic<int> mState{STATE_IDLE};
    std::atomic<bool> mStopFlag{false};
    std::atomic<bool> mReopening{false};
    std::atomic<bool> mStreamLost{false};
    // 会话代：重连一次 +1，各线程发现变化即冲刷本地状态
    std::atomic<int> mSessionGen{0};

    // FFmpeg
    AVFormatContext *mFormatContext = nullptr;
    int mVideoStreamIndex = -1;
    int mAudioStreamIndex = -1;
    int mWidth = 0;
    int mHeight = 0;
    std::string mHardMime;   // "video/avc" / "video/hevc"，空表示不支持硬解
    AVRational mVideoTimeBase{1, 1};
    AVRational mAudioTimeBase{1, 1};
    AVCodecContext *mVideoCodecCtx = nullptr;   // 软解
    AVCodecContext *mAudioCodecCtx = nullptr;
    SwrContext *mSwrContext = nullptr;
    int mSampleRate = 0;
    int mChannels = 0;
    AVChannelLayout mOutChLayout{};
    std::atomic<bool> mHasAudio{false};
    std::atomic<bool> mAudioDisabled{false};
    // 直播积压时丢包到下一个关键帧，保证低延迟（丢中间 P 帧会花屏，必须从 I 帧恢复）
    bool mVideoDropping{false};

    // 软/硬件解码上下文重开与使用。音频与视频各用一把锁：
    // 软解 1920x858 时 videoDecodeThread 会连续持有锁几十毫秒，
    // 与音频解码共用一把锁会让音频块队列被抽干、主时钟停顿（实测上屏从 25fps 掉到 11~17fps）
    pthread_mutex_t mAudioCodecMutex{};
    pthread_mutex_t mVideoCodecMutex{};

    // 队列
    ThreadSafeQueue<AVPacket *> mVideoPackets;
    ThreadSafeQueue<AVPacket *> mAudioPackets;
    ThreadSafeQueue<VideoFrameData *> mVideoFrames;
    ThreadSafeQueue<AudioChunk *> mAudioChunks;

    // Surface / 渲染
    jobject mSurfaceGlobal = nullptr;
    ANativeWindow *mNativeWindow = nullptr;
    SwsContext *mSwsContext = nullptr;
    AVFrame *mRgbaFrame = nullptr;
    uint8_t *mOutBuffer = nullptr;
    int mRgbWidth = 0;
    int mRgbHeight = 0;
    int mSrcWidth = 0;    // sws 源尺寸（视频真实分辨率）
    int mSrcHeight = 0;

    // 硬解
    RtspHardDecoder mHardDecoder;
    std::atomic<bool> mHardInited{false};
    std::atomic<bool> mHardFailed{false};
    vector<uint8_t> mSpsData;
    vector<uint8_t> mPpsData;

    // 音频播放
    OpenslHelper mOpensl;
    bool mOpenSLReady = false;
    uint8_t *mPlayBuffers[NUM_BUFFERS] = {nullptr};
    uint8_t mSilenceBuffer[PLAY_BUFFER_SIZE];
    int mFillIndex = 0;
    std::atomic<int> mQueuedBufferCount{0};
    std::mutex mPtsRingMutex;;
    // 每个已入队 buffer 的结束 pts（秒），回调消费时出队作为音频时钟
    std::deque<double> mPtsRing;
    double mLastEnqueuedEndPts = 0;
    std::atomic<double> mAudioClock{0};

    // 无音频时的系统时钟基准
    std::atomic<bool> mSysBaseSet{false};
    double mSysBasePts = 0;
    int64_t mSysBaseTimeUs = 0;

    // 播放线程
    pthread_t mDemuxThread = 0;
    pthread_t mAudioDecodeThread = 0;
    pthread_t mAudioPlayThread = 0;
    pthread_t mVideoDecodeThread = 0;
    pthread_t mVideoRenderThread = 0;
    pthread_t mVideoHardThread = 0;

    void joinThreads();

    void cleanupAll();
};

#endif //FFMPEGPRACTICE_RTSP_PLAYER_H
