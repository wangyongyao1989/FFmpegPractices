//
// Created by wangyao on 2025/9/20.
//

#include "includes/HwMuxer.h"


int32_t HwMuxer::initMuxer(int32_t fd, MUXER_OUTPUT_T outputFormat) {
    if (!mFormat) mFormat = mExtractor->getFormat();
    if (!mStats) mStats = new Stats();

    int64_t sTime = mStats->getCurTime();
    mMuxer = AMediaMuxer_new(fd, (OutputFormat) outputFormat);
    if (!mMuxer) {
        ALOGV("Unable to create muxer");
        return AMEDIA_ERROR_INVALID_OBJECT;
    }
    /*
     * AMediaMuxer_addTrack returns the index of the new track or a negative value
     * in case of failure, which can be interpreted as a media_status_t.
     */
    ssize_t index = AMediaMuxer_addTrack(mMuxer, mFormat);
    if (index < 0) {
        ALOGV("Format not supported");
        return index;
    }
    AMediaMuxer_start(mMuxer);
    int64_t eTime = mStats->getCurTime();
    int64_t timeTaken = mStats->getTimeDiff(sTime, eTime);
    mStats->setInitTime(timeTaken);
    return AMEDIA_OK;
}

void HwMuxer::deInitMuxer() {
    if (mFormat) {
        AMediaFormat_delete(mFormat);
        mFormat = nullptr;
    }
    if (!mMuxer) return;
    int64_t sTime = mStats->getCurTime();
    AMediaMuxer_stop(mMuxer);
    AMediaMuxer_delete(mMuxer);
    int64_t eTime = mStats->getCurTime();
    int64_t timeTaken = mStats->getTimeDiff(sTime, eTime);
    mStats->setDeInitTime(timeTaken);
}

void HwMuxer::resetMuxer() {
    if (mStats) mStats->reset();
}

void HwMuxer::dumpStatistics(string inputReference, string componentName, string statsFile) {
    string operation = "mux";
    mStats->dumpStatistics(operation, inputReference, mExtractor->getClipDuration(), componentName,
                           "", statsFile);
}

int32_t HwMuxer::mux(uint8_t *inputBuffer, vector<AMediaCodecBufferInfo> &frameInfos) {
    // Mux frame data
    size_t frameIdx = 0;
    mStats->setStartTime();
    while (frameIdx < frameInfos.size()) {
        AMediaCodecBufferInfo info = frameInfos.at(frameIdx);
        media_status_t status = AMediaMuxer_writeSampleData(mMuxer, 0, inputBuffer, &info);
        if (status != 0) {
            ALOGE("Error in AMediaMuxer_writeSampleData");
            return status;
        }
        mStats->addOutputTime();
        mStats->addFrameSize(info.size);
        frameIdx++;
    }
    return AMEDIA_OK;
}


//写一帧的数据
int32_t HwMuxer::muxFrame(uint8_t *inputBuffer, int trackIdx, AMediaCodecBufferInfo &frameInfo) {
    LOGE("muxFrame inputBuffer:%p", inputBuffer);
    AMediaCodecBufferInfo Info ;
    Info.size = frameInfo.size;
    Info.flags = frameInfo.flags;
    Info.offset = frameInfo.offset;
    Info.presentationTimeUs = frameInfo.presentationTimeUs;
    // Mux frame data
    media_status_t status = AMediaMuxer_writeSampleData(mMuxer, trackIdx, inputBuffer, &Info);
    if (status != 0) {
        ALOGE("Error in AMediaMuxer_writeSampleData");
        return status;
    }
    return AMEDIA_OK;
}