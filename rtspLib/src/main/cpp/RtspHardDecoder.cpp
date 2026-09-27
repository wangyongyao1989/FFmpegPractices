//  Author : wangyao https://github.com/wangyongyao1989
#include "includes/RtspHardDecoder.h"
#include <cstring>

bool RtspHardDecoder::init(const char *mime, int width, int height,
                           const uint8_t *sps, size_t spsSize,
                           const uint8_t *pps, size_t ppsSize,
                           ANativeWindow *window) {
    if (mCodec != nullptr) {
        LOGE("RtspHardDecoder::init codec already created");
        return mStarted;
    }

    mCodec = AMediaCodec_createDecoderByType(mime);
    if (mCodec == nullptr) {
        LOGE("RtspHardDecoder: createDecoderByType failed, mime=%s", mime);
        return false;
    }

    mFormat = AMediaFormat_new();
    AMediaFormat_setString(mFormat, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_HEIGHT, height);
    if (sps != nullptr && spsSize > 0) {
        AMediaFormat_setBuffer(mFormat, "csd-0", sps, spsSize);
    }
    if (pps != nullptr && ppsSize > 0) {
        AMediaFormat_setBuffer(mFormat, "csd-1", pps, ppsSize);
    }

    media_status_t status = AMediaCodec_configure(mCodec, mFormat, window, nullptr, 0);
    if (status != AMEDIA_OK) {
        LOGE("RtspHardDecoder: configure failed, status=%d", status);
        release();
        return false;
    }

    status = AMediaCodec_start(mCodec);
    if (status != AMEDIA_OK) {
        LOGE("RtspHardDecoder: start failed, status=%d", status);
        release();
        return false;
    }

    mStarted = true;
    LOGI("RtspHardDecoder: started mime=%s %dx%d sps=%zu pps=%zu",
         mime, width, height, spsSize, ppsSize);
    return true;
}

bool RtspHardDecoder::queueData(const uint8_t *data, size_t size, int64_t ptsUs,
                                uint32_t flags) {
    if (!mStarted) {
        return false;
    }
    ssize_t index = dequeueInput(0);
    if (index < 0) {
        return false;
    }
    size_t bufferLen = 0;
    uint8_t *buffer = AMediaCodec_getInputBuffer(mCodec, index, &bufferLen);
    if (buffer == nullptr || size > bufferLen) {
        LOGE("RtspHardDecoder: input buffer too small %zu > %zu", size, bufferLen);
        return false;
    }
    memcpy(buffer, data, size);
    media_status_t status = AMediaCodec_queueInputBuffer(mCodec, index, 0, size,
                                                         static_cast<size_t>(ptsUs), flags);
    return status == AMEDIA_OK;
}

bool RtspHardDecoder::releaseOutput(ssize_t index, bool render) {
    if (!mStarted) {
        return false;
    }
    media_status_t status = AMediaCodec_releaseOutputBuffer(mCodec, index, render);
    return status == AMEDIA_OK;
}

bool RtspHardDecoder::onFormatChanged(ssize_t code, int &width, int &height) {
    if (code != kFormatChanged) {
        return false;
    }
    AMediaFormat *outFormat = AMediaCodec_getOutputFormat(mCodec);
    if (outFormat != nullptr) {
        AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_WIDTH, &width);
        AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_HEIGHT, &height);
        LOGI("RtspHardDecoder: output format %dx%d", width, height);
        AMediaFormat_delete(outFormat);
    }
    return true;
}

void RtspHardDecoder::stop() {
    if (mCodec != nullptr && mStarted) {
        AMediaCodec_stop(mCodec);
        mStarted = false;
        LOGI("RtspHardDecoder: stopped");
    }
}

void RtspHardDecoder::release() {
    stop();
    if (mCodec != nullptr) {
        AMediaCodec_delete(mCodec);
        mCodec = nullptr;
    }
    if (mFormat != nullptr) {
        AMediaFormat_delete(mFormat);
        mFormat = nullptr;
    }
}
