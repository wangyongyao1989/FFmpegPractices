//
// Created by wangyao on 2025/9/20.
//

#include "includes/HwDeCodec.h"


tuple<ssize_t, uint32_t, int64_t> readSampleData(uint8_t *inputBuffer, int32_t &offset,
                                                 vector<AMediaCodecBufferInfo> &frameInfo,
                                                 uint8_t *buf, int32_t frameID, size_t bufSize) {
    ALOGV("In %s", __func__);
    if (frameID == (int32_t)frameInfo.size()) {
        return make_tuple(0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM, 0);
    }
    uint32_t flags = frameInfo[frameID].flags;
    int64_t timestamp = frameInfo[frameID].presentationTimeUs;
    ssize_t bytesCount = frameInfo[frameID].size;
    if (bufSize < bytesCount) {
        ALOGE("Error : Buffer size is insufficient to read sample");
        return make_tuple(0, AMEDIA_ERROR_MALFORMED, 0);
    }

    memcpy(buf, inputBuffer + offset, bytesCount);
    offset += bytesCount;
    return make_tuple(bytesCount, flags, timestamp);
}

void HwDeCodec::onInputAvailable(AMediaCodec *mediaCodec, int32_t bufIdx) {
    ALOGV("In %s", __func__);
    if (mediaCodec == mCodec && mediaCodec) {
        if (mSawInputEOS || bufIdx < 0) return;
        if (mSignalledError) {
            CallBackHandle::mSawError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        size_t bufSize;
        uint8_t *buf = AMediaCodec_getInputBuffer(mCodec, bufIdx, &bufSize);
        if (!buf) {
            mErrorCode = AMEDIA_ERROR_IO;
            mSignalledError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        ssize_t bytesRead = 0;
        uint32_t flag = 0;
        int64_t presentationTimeUs = 0;
        tie(bytesRead, flag, presentationTimeUs) =
                readSampleData(mInputBuffer, mOffset, mFrameMetaData, buf, mNumInputFrame, bufSize);
        if (flag == AMEDIA_ERROR_MALFORMED) {
            mErrorCode = (media_status_t)flag;
            mSignalledError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        if (flag == AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) mSawInputEOS = true;
        ALOGV("%s bytesRead : %zd presentationTimeUs : %" PRId64 " mSawInputEOS : %s", __FUNCTION__,
              bytesRead, presentationTimeUs, mSawInputEOS ? "TRUE" : "FALSE");

        media_status_t status = AMediaCodec_queueInputBuffer(mCodec, bufIdx, 0 /* offset */,
                                                             bytesRead, presentationTimeUs, flag);
        if (AMEDIA_OK != status) {
            mErrorCode = status;
            mSignalledError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }
        mStats->addFrameSize(bytesRead);
        mNumInputFrame++;
    }
}

void HwDeCodec::onOutputAvailable(AMediaCodec *mediaCodec, int32_t bufIdx,
                                AMediaCodecBufferInfo *bufferInfo) {
    ALOGV("In %s", __func__);
    if (mediaCodec == mCodec && mediaCodec) {
        if (mSawOutputEOS || bufIdx < 0) return;
        if (mSignalledError) {
            CallBackHandle::mSawError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        if (mOutFp != nullptr) {
            size_t bufSize;
            uint8_t *buf = AMediaCodec_getOutputBuffer(mCodec, bufIdx, &bufSize);
            if (buf) {
                fwrite(buf, sizeof(char), bufferInfo->size, mOutFp);
                ALOGV("bytes written into file  %d\n", bufferInfo->size);
            }
        }

        AMediaCodec_releaseOutputBuffer(mCodec, bufIdx, false);
        mSawOutputEOS = (0 != (bufferInfo->flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM));
        mNumOutputFrame++;
        ALOGV("%s index : %d  mSawOutputEOS : %s count : %u", __FUNCTION__, bufIdx,
              mSawOutputEOS ? "TRUE" : "FALSE", mNumOutputFrame);

        if (mSawOutputEOS) {
            CallBackHandle::mIsDone = true;
            mDecoderDoneCondition.notify_one();
        }
    }
}

void HwDeCodec::onFormatChanged(AMediaCodec *mediaCodec, AMediaFormat *format) {
    ALOGV("In %s", __func__);
    if (mediaCodec == mCodec && mediaCodec) {
        ALOGV("%s { %s }", __FUNCTION__, AMediaFormat_toString(format));
        mFormat = format;
    }
}

void HwDeCodec::onError(AMediaCodec *mediaCodec, media_status_t err) {
    ALOGV("In %s", __func__);
    if (mediaCodec == mCodec && mediaCodec) {
        ALOGE("Received Error %d", err);
        mErrorCode = err;
        mSignalledError = true;
        mDecoderDoneCondition.notify_one();
    }
}

void HwDeCodec::setupDecoder() {
    if (!mFormat) mFormat = mExtractor->getFormat();
}

AMediaFormat *HwDeCodec::getFormat() {
    ALOGV("In %s", __func__);
    return AMediaCodec_getOutputFormat(mCodec);
}

int32_t HwDeCodec::decode(uint8_t *inputBuffer, vector<AMediaCodecBufferInfo> &frameInfo,
                        string &codecName, bool asyncMode, FILE *outFp) {
    ALOGV("In %s", __func__);
    mInputBuffer = inputBuffer;
    mFrameMetaData = frameInfo;
    mOffset = 0;
    mOutFp = outFp;

    const char *mime = nullptr;
    AMediaFormat_getString(mFormat, AMEDIAFORMAT_KEY_MIME, &mime);
    if (!mime) return AMEDIA_ERROR_INVALID_OBJECT;

    int64_t sTime = mStats->getCurTime();
    mCodec = createMediaCodec(mFormat, mime, codecName, false /*isEncoder*/);
    if (!mCodec) return AMEDIA_ERROR_INVALID_OBJECT;

    if (asyncMode) {
        AMediaCodecOnAsyncNotifyCallback aCB = {OnInputAvailableCB, OnOutputAvailableCB,
                                                OnFormatChangedCB, OnErrorCB};
        AMediaCodec_setAsyncNotifyCallback(mCodec, aCB, this);

        ThreadTask task = []() {
            CallBackHandle();
        };

        g_threadManager->submitTask("HwDeCodec::decode-Thread", task, PRIORITY_NORMAL);

//        mIOThread = thread(&CallBackHandle::ioThread, this);
    }

    AMediaCodec_start(mCodec);
    int64_t eTime = mStats->getCurTime();
    int64_t timeTaken = mStats->getTimeDiff(sTime, eTime);
    mStats->setInitTime(timeTaken);

    mStats->setStartTime();
    if (!asyncMode) {
        while (!mSawOutputEOS && !mSignalledError) {
            /* Queue input data */
            if (!mSawInputEOS) {
                ssize_t inIdx = AMediaCodec_dequeueInputBuffer(mCodec, kQueueDequeueTimeoutUs);
                if (inIdx < 0 && inIdx != AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                    ALOGE("AMediaCodec_dequeueInputBuffer returned invalid index %zd\n", inIdx);
                    mErrorCode = (media_status_t)inIdx;
                    return mErrorCode;
                } else if (inIdx >= 0) {
                    mStats->addInputTime();
                    onInputAvailable(mCodec, inIdx);
                }
            }

            /* Dequeue output data */
            AMediaCodecBufferInfo info;
            ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(mCodec, &info, kQueueDequeueTimeoutUs);
            if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                mFormat = AMediaCodec_getOutputFormat(mCodec);
                const char *s = AMediaFormat_toString(mFormat);
                ALOGI("Output format: %s\n", s);
            } else if (outIdx >= 0) {
                mStats->addOutputTime();
                onOutputAvailable(mCodec, outIdx, &info);
            } else if (!(outIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER ||
                         outIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)) {
                ALOGE("AMediaCodec_dequeueOutputBuffer returned invalid index %zd\n", outIdx);
                mErrorCode = (media_status_t)outIdx;
                return mErrorCode;
            }
        }
    } else {
        unique_lock<mutex> lock(mMutex);
        mDecoderDoneCondition.wait(lock, [this]() { return (mSawOutputEOS || mSignalledError); });
    }
    if (mSignalledError) {
        ALOGE("Received Error while Decoding");
        return mErrorCode;
    }

    if (codecName.empty()) {
        char *decName;
        AMediaCodec_getName(mCodec, &decName);
        codecName.assign(decName);
        AMediaCodec_releaseName(mCodec, decName);
    }
    return AMEDIA_OK;
}

void HwDeCodec::deInitCodec() {
    if (mFormat) {
        AMediaFormat_delete(mFormat);
        mFormat = nullptr;
    }
    if (!mCodec) return;
    int64_t sTime = mStats->getCurTime();
    AMediaCodec_stop(mCodec);
    AMediaCodec_delete(mCodec);
    // delete 后必须置空：本对象的 mCodec 还会被后面的 if (mCodec) 判断用到
    // （例如再来一次 deInitCodec() 或析构），留着悬垂地址就是二次释放。
    mCodec = nullptr;
    int64_t eTime = mStats->getCurTime();
    int64_t timeTaken = mStats->getTimeDiff(sTime, eTime);
    mStats->setDeInitTime(timeTaken);
}

void HwDeCodec::dumpStatistics(string inputReference, string componentName, string mode,
                             string statsFile) {
    int64_t durationUs = mExtractor->getClipDuration();
    string operation = "decode";
    mStats->dumpStatistics(operation, inputReference, durationUs, componentName, mode, statsFile);
}

void HwDeCodec::resetDecoder() {
    if (mStats) mStats->reset();
    if (mInputBuffer) mInputBuffer = nullptr;
    if (!mFrameMetaData.empty()) mFrameMetaData.clear();
    // ProcessDeCodec 是同一条码流里每条轨道复用同一个 HwDeCodec 对象（for curTrack 循环
    // 里 setupDecoder()/decode()/deInitCodec()/resetDecoder() 各一次），而这些是
    // 「一条轨道一清」的运行态：视频轨解到 EOS 后 mSawOutputEOS 一直是 true，
    // 音频轨的 drain 循环 while (!mSawOutputEOS && !mSignalledError) 一次都不进，
    // 于是音频一帧都不解（基线日志里第二条轨道紧跟一句 E Stats: No output produced）。
    // 头文件里的默认初值只保证第一条轨道干净，跨轨道必须在这里复位。
    mSawInputEOS = false;
    mSawOutputEOS = false;
    mSignalledError = false;
    mErrorCode = AMEDIA_OK;
    mNumInputFrame = 0;
    mNumOutputFrame = 0;
    CallBackHandle::mSawError = false;
    CallBackHandle::mIsDone = false;
}
