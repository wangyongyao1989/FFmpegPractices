//
// Created by wangyao on 2025/9/20.
//

#ifndef FFMPEGPRACTICE_HWMUXER_H
#define FFMPEGPRACTICE_HWMUXER_H
//#define LOG_NDEBUG 0
#define LOG_TAG "muxer"

#include <fstream>
#include <iostream>

#include <media/NdkMediaMuxer.h>

#include "BenchmarkCommon.h"
#include "Stats.h"
#include "HwExtractor.h"

typedef enum {
    MUXER_OUTPUT_FORMAT_MPEG_4 = 0,
    MUXER_OUTPUT_FORMAT_WEBM = 1,
    MUXER_OUTPUT_FORMAT_3GPP = 2,
    MUXER_OUTPUT_FORMAT_OGG = 4,
    MUXER_OUTPUT_FORMAT_INVALID = 5,
} MUXER_OUTPUT_T;


class HwMuxer {
public:
    HwMuxer() : mFormat(nullptr), mMuxer(nullptr),
                mStats(nullptr) { mExtractor = new HwExtractor(); }

    virtual ~HwMuxer() {
        if (mStats) delete mStats;
        if (mExtractor) delete mExtractor;
    }

    Stats *getStats() { return mStats; }

    HwExtractor *getExtractor() { return mExtractor; }

    /* Muxer related utilities */
    int32_t initMuxer(int32_t fd, MUXER_OUTPUT_T outputFormat);

    void deInitMuxer();

    void resetMuxer();

    /* Process the frames and give Muxed output */
    int32_t mux(uint8_t *inputBuffer, vector<AMediaCodecBufferInfo> &frameSizes);

    int32_t muxFrame(uint8_t *inputBuffer, int trackIdx, AMediaCodecBufferInfo &frameInfo);

    void dumpStatistics(string inputReference, string codecName = "", string statsFile = "");

private:
    AMediaFormat *mFormat;
    AMediaMuxer *mMuxer;
    HwExtractor *mExtractor;
    Stats *mStats;
};


#endif //FFMPEGPRACTICE_HWMUXER_H
