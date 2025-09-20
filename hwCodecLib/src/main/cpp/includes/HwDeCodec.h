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
    }

    virtual ~HwDeCodec() {
        if (mExtractor) delete mExtractor;
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
    AMediaCodec *mCodec;
    AMediaFormat *mFormat;

    HwExtractor *mExtractor;

    int32_t mNumInputFrame;
    int32_t mNumOutputFrame;

    bool mSawInputEOS;
    bool mSawOutputEOS;
    bool mSignalledError;
    media_status_t mErrorCode;

    int32_t mOffset;
    uint8_t *mInputBuffer;
    vector<AMediaCodecBufferInfo> mFrameMetaData;
    FILE *mOutFp;

    /* Asynchronous locks */
    mutex mMutex;
    condition_variable mDecoderDoneCondition;
};

// Read input samples
tuple<ssize_t, uint32_t, int64_t> readSampleData(uint8_t *inputBuffer, int32_t &offset,
                                                 vector<AMediaCodecBufferInfo> &frameSizes,
                                                 uint8_t *buf, int32_t frameID, size_t bufSize);


#endif //FFMPEGPRACTICE_HWDECODEC_H
