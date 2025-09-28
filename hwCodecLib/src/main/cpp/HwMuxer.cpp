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
    LOGE("muxFrame inputBuffer:%p, trackIdx: %d, size: %d, offset: %d, pts: %lld",
         inputBuffer, trackIdx, frameInfo.size, frameInfo.offset, frameInfo.presentationTimeUs);

    // 1. 检查 muxer 是否有效
    if (mMuxer == nullptr) {
        LOGE("mMuxer is NULL!");
        return AMEDIA_ERROR_INVALID_OPERATION;
    }
    // 2. 检查输入缓冲区是否有效
    if (inputBuffer == nullptr) {
        LOGE("inputBuffer is NULL!");
        return AMEDIA_ERROR_INVALID_PARAMETER;
    }

    // 3. 检查缓冲区大小合理性
    if (frameInfo.size <= 0 || frameInfo.size > 10 * 1024 * 1024) { // 限制最大10MB
        LOGE("Invalid frame size: %d", frameInfo.size);
        return AMEDIA_ERROR_INVALID_PARAMETER;
    }

    // 4. 检查 track 索引有效性
    if (trackIdx < 0 || trackIdx >= 2) {
        LOGE("Invalid track index: %d, total tracks: %d", trackIdx, 2);
        return AMEDIA_ERROR_INVALID_PARAMETER;
    }

    // 5. 检查偏移量有效性
    if (frameInfo.offset < 0) {
        LOGE("Invalid offset: %d", frameInfo.offset);
        frameInfo.offset = 0;
    }

    // 6. 检查offset和size的和是否超出合理范围
    if (frameInfo.offset + frameInfo.size > 10 * 1024 * 1024) {
        LOGE("Offset + size too large: offset=%d, size=%d, sum=%d",
             frameInfo.offset, frameInfo.size, frameInfo.offset + frameInfo.size);
        return AMEDIA_ERROR_INVALID_PARAMETER;
    }

    // 7. 检查时间戳有效性（避免负值或异常大值）
    if (frameInfo.presentationTimeUs < 0 || frameInfo.presentationTimeUs > INT64_MAX / 2) {
        LOGE("Invalid PTS: %lld", frameInfo.presentationTimeUs);
        return AMEDIA_ERROR_INVALID_PARAMETER;
    }

    // 8. 检查内存访问权限（尝试读取首尾字节）
    try {
        volatile uint8_t testStart = inputBuffer[frameInfo.offset];
        volatile uint8_t testEnd = inputBuffer[frameInfo.offset + frameInfo.size - 1];
        (void)testStart;
        (void)testEnd;
    } catch (...) {
        LOGE("Cannot access inputBuffer memory! offset: %d, size: %d",
             frameInfo.offset, frameInfo.size);
        return AMEDIA_ERROR_INVALID_PARAMETER;
    }

    AMediaCodecBufferInfo info = frameInfo; // 直接复制，避免手动赋值错误

    LOGE("Writing sample: track=%d, size=%d, offset=%d, flags=0x%x, pts=%lld",
         trackIdx, info.size, info.offset, info.flags, info.presentationTimeUs);

    // 9. 使用正确的trackIdx，而不是硬编码的0
    media_status_t status = AMediaMuxer_writeSampleData(mMuxer, trackIdx,
                                                        inputBuffer, &info);

    if (status != AMEDIA_OK) {
        LOGE("AMediaMuxer_writeSampleData failed: %d, trackIdx: %d", status, trackIdx);

        // 详细的错误处理
        switch (status) {
            case AMEDIA_ERROR_INVALID_OPERATION:
                LOGE("Invalid operation - muxer may not be started or track invalid");
                break;
            case AMEDIA_ERROR_MALFORMED:
                LOGE("Malformed data");
                break;
            case AMEDIA_ERROR_UNSUPPORTED:
                LOGE("Unsupported operation");
                break;
            case AMEDIA_ERROR_IO:
                LOGE("I/O error");
                break;
            default:
                LOGE("Unknown error");
                break;
        }

        return status;
    }

    LOGD("Successfully muxed frame, track: %d, size: %d", trackIdx, info.size);
    return AMEDIA_OK;
}