//
// Created by wangyao on 2025/9/20.
//

#ifndef FFMPEGPRACTICE_HWEXTRACTOR_H
#define FFMPEGPRACTICE_HWEXTRACTOR_H
#include <media/NdkMediaExtractor.h>
#include <string>
#include "Stats.h"
#include "BenchmarkCommon.h"
#include "LogUtils.h"


class HwExtractor {

public:
    HwExtractor()
            : mFormat(nullptr),
              mExtractor(nullptr),
              mStats(nullptr),
              mFrameBuf{nullptr},
              mDurationUs{0} {}

    ~HwExtractor() {
        if (mStats) delete mStats;
    }

    int32_t initExtractor(int32_t fd, size_t fileSize);

    int32_t setupTrackFormat(int32_t trackId);

    void *getCSDSample(AMediaCodecBufferInfo &frameInfo, int32_t csdIndex);

    int32_t getFrameSample(AMediaCodecBufferInfo &frameInfo);

    int32_t getCurFrameSample(AMediaCodecBufferInfo &frameInfo);

    int32_t extract(int32_t trackId);

    void dumpStatistics(string inputReference, string componentName = "", string statsFile = "");

    void deInitExtractor();

    AMediaFormat *getFormat() { return mFormat; }

    uint8_t *getFrameBuf() { return mFrameBuf; }

    uint8_t *getCurFrameBuf() { return mCurFrameBuf; }

    int64_t getClipDuration() { return mDurationUs; }

private:
    AMediaFormat *mFormat;
    AMediaExtractor *mExtractor;
    Stats *mStats;
    uint8_t *mFrameBuf;
    uint8_t *mCurFrameBuf = nullptr;
    int64_t mDurationUs;

};


#endif //FFMPEGPRACTICE_HWEXTRACTOR_H
