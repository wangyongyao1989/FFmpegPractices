//
// Created by wangyao on 2025/9/20.
//

#ifndef FFMPEGPRACTICE_HWDECODEC_H
#define FFMPEGPRACTICE_HWDECODEC_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "BenchmarkCommon.h"
#include "HwExtractor.h"
#include "Stats.h"
#include "AndroidThreadManager.h"
//#define LOG_NDEBUG 0
#define LOG_TAG "decoder"

#include <iostream>


class HwDeCodec : CallBackHandle {
public:
    HwDeCodec()
            : mCodec(nullptr),
              mFormat(nullptr),
              mExtractor(nullptr),
              mNumInputFrame(0),
              mNumOutputFrame(0),
              mSawInputEOS(false),
              mSawOutputEOS(false),
              mSignalledError(false),
              mErrorCode(AMEDIA_OK),
              mInputBuffer(nullptr),
              mOutFp(nullptr) {
        mExtractor = new HwExtractor();
        g_threadManager = std::make_unique<AndroidThreadManager>();

        // 初始化线程池
        ThreadPoolConfig config;
        config.minThreads = 2;
        config.maxThreads = 4;
        config.idleTimeoutMs = 30000;
        config.queueSize = 50;
        g_threadManager->initThreadPool(config);
    }

    virtual ~HwDeCodec() {
        if (mExtractor) delete mExtractor;
        g_threadManager.reset();
    }

    HwExtractor *getExtractor() { return mExtractor; }

    // Decoder related utilities
    void setupDecoder();

    void deInitCodec();

    void resetDecoder();

    AMediaFormat *getFormat();

    // Async callback APIs
    void onInputAvailable(AMediaCodec *codec, int32_t index) override;

    void onFormatChanged(AMediaCodec *codec, AMediaFormat *format) override;

    void onError(AMediaCodec *mediaCodec, media_status_t err) override;

    void onOutputAvailable(AMediaCodec *codec, int32_t index,
                           AMediaCodecBufferInfo *bufferInfo) override;

    // Process the frames and give decoded output
    int32_t decode(uint8_t *inputBuffer, vector<AMediaCodecBufferInfo> &frameInfo,
                   string &codecName, bool asyncMode, FILE *outFp = nullptr);

    void dumpStatistics(string inputReference, string componentName = "", string mode = "",
                        string statsFile = "");

private:
    // 本类没有自定义构造函数（MediaExtratorDecodec/Encodec 同理），这些标量成员
    // 之前全部处于 indeterminate 值。而它们是解码循环的守卫条件：
    //     while (!mSawOutputEOS && !mSignalledError)
    // 读到非 0 残值就直接不进循环，一帧都不解，输出文件 0 字节且没有任何报错。
    // 更隐蔽的是对象复用：ProcessDeCodec 每轮 delete 后 new 同样大小的对象，
    // 内存块被回收再使用，上一轮的 mSawOutputEOS=true 会原样留在里面，
    // 于是「第一次点有输出、第二次点输出 0 字节」。
    AMediaCodec *mCodec = nullptr;
    AMediaFormat *mFormat = nullptr;

    HwExtractor *mExtractor = nullptr;

    int32_t mNumInputFrame = 0;
    int32_t mNumOutputFrame = 0;

    bool mSawInputEOS = false;
    bool mSawOutputEOS = false;
    bool mSignalledError = false;
    media_status_t mErrorCode = AMEDIA_OK;

    int32_t mOffset = 0;
    uint8_t *mInputBuffer = nullptr;
    vector<AMediaCodecBufferInfo> mFrameMetaData;
    FILE *mOutFp = nullptr;

    /* Asynchronous locks */
    mutex mMutex;
    condition_variable mDecoderDoneCondition;

    std::unique_ptr<AndroidThreadManager> g_threadManager;
};

// Read input samples
tuple<ssize_t, uint32_t, int64_t> readSampleData(uint8_t *inputBuffer, int32_t &offset,
                                                 vector<AMediaCodecBufferInfo> &frameSizes,
                                                 uint8_t *buf, int32_t frameID, size_t bufSize);


#endif //FFMPEGPRACTICE_HWDECODEC_H
