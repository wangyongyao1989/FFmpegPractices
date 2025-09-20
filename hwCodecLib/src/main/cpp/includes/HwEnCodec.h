//
// Created by wangyao on 2025/9/20.
//

#ifndef FFMPEGPRACTICE_HWENCODEC_H
#define FFMPEGPRACTICE_HWENCODEC_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "BenchmarkCommon.h"
#include "Stats.h"

//#define LOG_NDEBUG 0
#define LOG_TAG "encoder"

#include <fstream>

// constant not defined in NDK api
constexpr int32_t COLOR_FormatYUV420Flexible = 0x7F420888;

struct encParameter {
    int32_t bitrate = -1;
    int32_t numFrames = -1;
    int32_t frameSize = -1;
    int32_t sampleRate = 0;
    int32_t numChannels = 0;
    int32_t maxFrameSize = -1;
    int32_t width = 0;
    int32_t height = 0;
    int32_t frameRate = -1;
    int32_t iFrameInterval = 0;
    int32_t profile = -1;
    int32_t level = -1;
    int32_t colorFormat = COLOR_FormatYUV420Flexible;
};


class HwEnCodec : public CallBackHandle {
public:
    HwEnCodec()
            : mCodec(nullptr),
              mFormat(nullptr),
              mNumInputFrame(0),
              mNumOutputFrame(0),
              mSawInputEOS(false),
              mSawOutputEOS(false),
              mSignalledError(false),
              mErrorCode(AMEDIA_OK) {}

    virtual ~HwEnCodec() {}

    // Encoder related utilities
    void setupEncoder();

    void deInitCodec();

    void resetEncoder();

    // Async callback APIs
    void onInputAvailable(AMediaCodec *codec, int32_t index) override;

    void onFormatChanged(AMediaCodec *codec, AMediaFormat *format) override;

    void onError(AMediaCodec *mediaCodec, media_status_t err) override;

    void onOutputAvailable(AMediaCodec *codec, int32_t index,
                           AMediaCodecBufferInfo *bufferInfo) override;

    // Process the frames and give encoded output
    int32_t encode(std::string &codecName, std::ifstream &eleStream, size_t eleSize, bool asyncMode,
                   encParameter encParams, char *mime);

    void dumpStatistics(string inputReference, int64_t durationUs, string codecName = "",
                        string mode = "", string statsFile = "");

private:
    AMediaCodec *mCodec;
    AMediaFormat *mFormat;

    int32_t mNumInputFrame;
    int32_t mNumOutputFrame;
    bool mSawInputEOS;
    bool mSawOutputEOS;
    bool mSignalledError;
    media_status_t mErrorCode;

    char *mMime;
    int32_t mOffset;
    std::ifstream *mEleStream;
    size_t mInputBufferSize;
    encParameter mParams;

    // Asynchronous locks
    std::mutex mMutex;
    std::condition_variable mEncoderDoneCondition;


};


#endif //FFMPEGPRACTICE_HWENCODEC_H
