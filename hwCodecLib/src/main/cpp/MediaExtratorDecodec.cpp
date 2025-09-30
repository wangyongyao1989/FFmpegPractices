//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/29.
//

#include <sys/stat.h>
#include "includes/MediaExtratorDecodec.h"


MediaExtratorDecodec::MediaExtratorDecodec(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
    // 初始化线程池
    g_threadManager = std::make_unique<AndroidThreadManager>();
    ThreadPoolConfig config;
    config.minThreads = 2;
    config.maxThreads = 4;
    config.idleTimeoutMs = 30000;
    config.queueSize = 50;
    g_threadManager->initThreadPool(config);

}

MediaExtratorDecodec::~MediaExtratorDecodec() {
    mEnv->DeleteGlobalRef(mJavaObj);
    if (mEnv) {
        mEnv = nullptr;
    }

    if (mJavaVm) {
        mJavaVm = nullptr;
    }

    if (mJavaObj) {
        mJavaObj = nullptr;
    }

    release();
    g_threadManager.reset();
}

void
MediaExtratorDecodec::startMediaExtratorDecodec(const char *inputPath) {
    sSrcPath = inputPath;

    LOGI("sSrcPath :%s \n ", sSrcPath.c_str());
    callbackInfo =
            "sSrcPath:" + sSrcPath + "\n";
    PostStatusMessage(callbackInfo.c_str());
    // 1. 初始化提取器
    if (!initExtractor()) {
        LOGE("Failed to initialize extractor");
        callbackInfo =
                "Failed to initialize extractor \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    // 2. 选择轨道
    if (!selectTracksAndGetFormat()) {
        LOGE("No valid tracks found");
        callbackInfo =
                "No valid tracks found \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    // 3. 初始化复用器
    if (!initDecodec(false)) {
        LOGE("Failed to initialize Decodec");
        callbackInfo =
                "Failed to initialize Decodec \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    // 4. 执行转封装
    if (!decodec()) {
        LOGE("Decodec failed");
        callbackInfo =
                "Decodec failed \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    // 释放资源
    release();
}

// 初始化提取器
bool MediaExtratorDecodec::initExtractor() {

    extractor = AMediaExtractor_new();
    if (!extractor) {
        LOGE("Failed to create media extractor ");
        callbackInfo =
                "Failed to create media extractor \n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }
    LOGE("inputPath:%s", sSrcPath.c_str());
    FILE *inputFp = fopen(sSrcPath.c_str(), "rb");
    if (!inputFp) {
        LOGE("Unable to open output file :%s", sSrcPath.c_str());
        callbackInfo =
                "Unable to open output file :" + sSrcPath + "\n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }
    struct stat buf;
    stat(sSrcPath.c_str(), &buf);
    size_t fileSize = buf.st_size;
    int32_t input_fd = fileno(inputFp);

    LOGE("input_fd:%d", input_fd);
    media_status_t status = AMediaExtractor_setDataSourceFd(extractor, input_fd, 0, fileSize);
    if (status != AMEDIA_OK) {
        LOGE("Failed to set data source: %d", status);
        callbackInfo =
                "Failed to set data source :" + to_string(status) + "\n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }

    LOGI("Extractor initialized successfully");
    return true;
}

// 选择轨道
bool MediaExtratorDecodec::selectTracksAndGetFormat() {
    LOGI("selectTracksAndGetFormat===========");

    size_t trackCount = AMediaExtractor_getTrackCount(extractor);
    LOGI("Total tracks: %zu", trackCount);
    callbackInfo =
            "Total tracks:" + to_string(trackCount) + "\n";
    PostStatusMessage(callbackInfo.c_str());

    for (size_t i = 0; i < trackCount; i++) {
        AMediaFormat *format = AMediaExtractor_getTrackFormat(extractor, i);
        if (!format) continue;

        const char *mime;
        if (AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
            LOGI("Track %zu: MIME=%s", i, mime);

            if (strncmp(mime, "video/", 6) == 0 && videoTrackIndex == -1) {
                videoTrackIndex = i;
                hasVideo = true;
                // 获取视频格式信息
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &videoWidth);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &videoHeight);
                AMediaFormat_getInt64(format, AMEDIAFORMAT_KEY_DURATION, &videoDuration);
                LOGI("Selected video track: %d", videoTrackIndex);

                LOGI("Video track: %dx%d, duration: %lld us",
                     videoWidth, videoHeight, videoDuration);
                callbackInfo =
                        "Selected video track:" + to_string(videoTrackIndex) + "\n";
                PostStatusMessage(callbackInfo.c_str());
            } /*else if (strncmp(mime, "audio/", 6) == 0 && audioTrackIndex == -1) {
                audioTrackIndex = i;
                hasAudio = true;

                // 获取音频格式信息
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &audioSampleRate);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &audioChannelCount);

                LOGI("Audio track: sampleRate=%d, channels=%d",
                     audioSampleRate, audioChannelCount);

                LOGI("Selected audio track: %d", audioTrackIndex);
                callbackInfo =
                        "Selected audio track:" + to_string(audioTrackIndex) + "\n";
                PostStatusMessage(callbackInfo.c_str());
            }*/
        }

        AMediaFormat_delete(format);
    }

    return hasVideo || hasAudio;
}

// 初始化复用器
bool MediaExtratorDecodec::initDecodec(bool asyncMode) {

    // 添加视频轨道
    if (hasVideo) {
        AMediaExtractor_selectTrack(extractor, videoTrackIndex);
        mVideoFormat = AMediaExtractor_getTrackFormat(extractor, videoTrackIndex);
        AMediaFormat_getString(mVideoFormat, AMEDIAFORMAT_KEY_MIME, &video_mime);
        LOGI("video_mime: %s", video_mime);
        callbackInfo =
                "video_mime:" + string(video_mime) + "\n";
        PostStatusMessage(callbackInfo.c_str());

        mVideoCodec = createMediaCodec(mVideoFormat, video_mime, "", false /*isEncoder*/);

//        AMediaFormat_delete(videoFormat);

        if (!mVideoCodec) {
            LOGE("Failed to create video codec");
            callbackInfo =
                    "Failed to create video codec \n";
            PostStatusMessage(callbackInfo.c_str());
            return false;
        }

        if (asyncMode) {
            AMediaCodecOnAsyncNotifyCallback aCB = {OnInputAvailableCB, OnOutputAvailableCB,
                                                    OnFormatChangedCB, OnErrorCB};
            AMediaCodec_setAsyncNotifyCallback(mVideoCodec, aCB, this);

            ThreadTask task = []() {
                CallBackHandle();
            };

            g_threadManager->submitTask("video-decode-Thread", task, PRIORITY_NORMAL);

        }

        LOGI("create video codec success");
        callbackInfo =
                "create video codec success:  \n";
        PostStatusMessage(callbackInfo.c_str());
        AMediaCodec_start(mVideoCodec);
    }


    // 添加音频轨道
//    if (hasAudio) {
    if (false) {
        AMediaExtractor_selectTrack(extractor, audioTrackIndex);
        mAudioFormat = AMediaExtractor_getTrackFormat(extractor, audioTrackIndex);
        AMediaFormat_getString(mAudioFormat, AMEDIAFORMAT_KEY_MIME, &audio_mime);
        LOGI("audio_mime: %s", audio_mime);
        callbackInfo =
                "audio_mime:" + string(audio_mime) + "\n";
        PostStatusMessage(callbackInfo.c_str());

        mAudioCodec = createMediaCodec(mAudioFormat, audio_mime, "", false /*isEncoder*/);
//        AMediaFormat_delete(audioFormat);

        if (!mAudioCodec) {
            LOGE("Failed to create audio codec");
            callbackInfo =
                    "Failed to create audio codec \n";
            PostStatusMessage(callbackInfo.c_str());
            return false;
        }

        if (asyncMode) {
            AMediaCodecOnAsyncNotifyCallback aCB = {OnInputAvailableCB, OnOutputAvailableCB,
                                                    OnFormatChangedCB, OnErrorCB};
            AMediaCodec_setAsyncNotifyCallback(mAudioCodec, aCB, this);

            ThreadTask task = []() {
                CallBackHandle();
            };

            g_threadManager->submitTask("video-decode-Thread", task, PRIORITY_NORMAL);

        }

        LOGI("create audio codec success");
        callbackInfo =
                "create audio codec success:  \n";
        PostStatusMessage(callbackInfo.c_str());
        AMediaCodec_start(mAudioCodec);
    }


    LOGI("initDecodec initialized successfully");
    callbackInfo =
            "initDecodec initialized successfully \n";
    PostStatusMessage(callbackInfo.c_str());
    return true;
}


// 执行转封装
bool MediaExtratorDecodec::decodec() {
    LOGI("decodec===========");
    bool asyncMode = false;

    AMediaCodecBufferInfo info;
    bool sawEOS = false;
    int64_t lastVideoPts = -1;
    int64_t lastAudioPts = -1;

    // 重新选择所有轨道以重置读取位置
    if (hasVideo) AMediaExtractor_selectTrack(extractor, videoTrackIndex);
    if (hasAudio) AMediaExtractor_selectTrack(extractor, audioTrackIndex);

    // 设置读取位置到开始
    AMediaExtractor_seekTo(extractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);

    while (!sawEOS) {
        ssize_t trackIndex = AMediaExtractor_getSampleTrackIndex(extractor);

        if (trackIndex < 0) {
            sawEOS = true;
            break;
        }

        // 获取样本信息
        info.size = AMediaExtractor_getSampleSize(extractor);
        info.offset = 0;
        info.flags = AMediaExtractor_getSampleFlags(extractor);
        info.presentationTimeUs = AMediaExtractor_getSampleTime(extractor);

        // 读取样本数据
        uint8_t *sampleData = new uint8_t[info.size];
        ssize_t bytesRead = AMediaExtractor_readSampleData(extractor, sampleData, info.size);

        if (bytesRead < 0) {
            LOGE("Error reading sample data: %zd", bytesRead);
            callbackInfo =
                    "Error reading sample data:" + to_string(bytesRead) + "\n";
            PostStatusMessage(callbackInfo.c_str());
            delete[] sampleData;
            break;
        }

        mInputBuffer = sampleData;
        mFrameMetaData.size = info.size;
        mFrameMetaData.flags = info.flags;
        mFrameMetaData.offset = info.offset;
        mFrameMetaData.presentationTimeUs = info.presentationTimeUs;

        if (trackIndex == videoTrackIndex && hasVideo) {
            // 检查时间戳是否有效（避免重复或倒退的时间戳）
            if (info.presentationTimeUs > lastVideoPts) {
                if (!asyncMode) {
                    while (!mSawOutputEOS && !mSignalledError) {
                        /* Queue input data */
                        if (!mSawInputEOS) {
                            ssize_t inIdx = AMediaCodec_dequeueInputBuffer(mVideoCodec,
                                                                           kQueueDequeueTimeoutUs);
                            if (inIdx < 0 && inIdx != AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                                ALOGE("AMediaCodec_dequeueInputBuffer returned invalid index %zd\n",
                                      inIdx);
                                mErrorCode = (media_status_t) inIdx;
                                return mErrorCode;
                            } else if (inIdx >= 0) {

                                onInputAvailable(mVideoCodec, inIdx);
                            }
                        }

                        /* Dequeue output data */
                        AMediaCodecBufferInfo info;
                        ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(mVideoCodec, &info,
                                                                         kQueueDequeueTimeoutUs);
                        if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                            mVideoFormat = AMediaCodec_getOutputFormat(mVideoCodec);
                            const char *s = AMediaFormat_toString(mVideoFormat);
                            ALOGI("Output format: %s\n", s);
                        } else if (outIdx >= 0) {
                            onOutputAvailable(mVideoCodec, outIdx, &info);
                        } else if (!(outIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER ||
                                     outIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)) {
                            ALOGE("AMediaCodec_dequeueOutputBuffer returned invalid index %zd\n",
                                  outIdx);
                            mErrorCode = (media_status_t) outIdx;
                            return mErrorCode;
                        }
                    }
                } else {
                    unique_lock<mutex> lock(mMutex);
                    mDecoderDoneCondition.wait(lock, [this]() {
                        return (mSawOutputEOS || mSignalledError);
                    });
                }
                if (mSignalledError) {
                    ALOGE("Received Error while Decoding");
                    return mErrorCode;
                }


                lastVideoPts = info.presentationTimeUs;
            }
        } else if (trackIndex == audioTrackIndex && hasAudio) {
            // 检查时间戳是否有效
            if (info.presentationTimeUs > lastAudioPts) {
//                media_status_t status = AMediaMuxer_writeSampleData(muxer,
//                                                                    muxerAudioTrackIndex,
//                                                                    sampleData,
//                                                                    &info);
//                if (status != AMEDIA_OK) {
//                    LOGE("Failed to write audio sample : %d", status);
//                    callbackInfo =
//                            "Failed to write audio sample:" + to_string(status) + "\n";
//                    PostStatusMessage(callbackInfo.c_str());
//                }
//                callbackInfo =
//                        "AMediaMuxer_writeSampleData audio size:" + to_string(info.size) + "\n";
//                PostStatusMessage(callbackInfo.c_str());
                lastAudioPts = info.presentationTimeUs;
            }
        }

        delete[] sampleData;
        delete[] mInputBuffer;

        // 前进到下一个样本
        if (!AMediaExtractor_advance(extractor)) {
            sawEOS = true;
        }
    }

    LOGI("Transmuxing completed");
    callbackInfo =
            "Transmuxing completed \n";
    PostStatusMessage(callbackInfo.c_str());
    return true;
}


// 释放资源
void MediaExtratorDecodec::release() {

    if (extractor) {
        AMediaExtractor_delete(extractor);
        extractor = nullptr;
    }

    if (mVideoFormat) {
        AMediaFormat_delete(mVideoFormat);
        mVideoFormat = nullptr;
    }

    if (mVideoCodec) {
        AMediaCodec_stop(mVideoCodec);
        AMediaCodec_delete(mVideoCodec);
    }

    if (mAudioCodec) {
        AMediaCodec_stop(mAudioCodec);
        AMediaCodec_delete(mAudioCodec);
    }
    if (mAudioFormat) {
        AMediaFormat_delete(mAudioFormat);
        mAudioFormat = nullptr;
    }
    LOGI("Resources released");
}

void MediaExtratorDecodec::onInputAvailable(AMediaCodec *mediaCodec, int32_t bufIdx) {
    LOGD("onInputAvailable %s", __func__);
    if (mediaCodec == mVideoCodec && mediaCodec) {
        if (mSawInputEOS || bufIdx < 0) return;
        if (mSignalledError) {
            CallBackHandle::mSawError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        size_t bufSize;
        uint8_t *buf = AMediaCodec_getInputBuffer(mVideoCodec, bufIdx, &bufSize);
        if (!buf) {
            mErrorCode = AMEDIA_ERROR_IO;
            mSignalledError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }
        ssize_t bytesRead = mFrameMetaData.size;
        uint32_t flag = mFrameMetaData.flags;
        int64_t presentationTimeUs = mFrameMetaData.presentationTimeUs;

//        ssize_t bytesRead = 0;
//        uint32_t flag = 0;
//        int64_t presentationTimeUs = 0;
//        tie(bytesRead, flag, presentationTimeUs) =
//                readSampleData(mInputBuffer, mOffset, mFrameMetaData, buf, mNumInputFrame,
//                               bufSize);
        if (flag == AMEDIA_ERROR_MALFORMED) {
            mErrorCode = (media_status_t) flag;
            mSignalledError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        if (flag == AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) mSawInputEOS = true;
        LOGD("%s bytesRead : %zd presentationTimeUs : %" PRId64 " mSawInputEOS : %s", __FUNCTION__,
             bytesRead, presentationTimeUs, mSawInputEOS ? "TRUE" : "FALSE");

        media_status_t status = AMediaCodec_queueInputBuffer(mVideoCodec, bufIdx, 0 /* offset */,
                                                             bytesRead, presentationTimeUs, flag);
        if (AMEDIA_OK != status) {
            mErrorCode = status;
            mSignalledError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }
        mNumInputFrame++;
    }
}


void MediaExtratorDecodec::onOutputAvailable(AMediaCodec *mediaCodec, int32_t bufIdx,
                                             AMediaCodecBufferInfo *bufferInfo) {
    LOGD("In %s", __func__);
    if (mediaCodec == mVideoCodec && mediaCodec) {
        if (mSawOutputEOS || bufIdx < 0) return;
        if (mSignalledError) {
            CallBackHandle::mSawError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        if (mOutFp != nullptr) {
            size_t bufSize;
            uint8_t *buf = AMediaCodec_getOutputBuffer(mVideoCodec, bufIdx, &bufSize);
            if (buf) {
                fwrite(buf, sizeof(char), bufferInfo->size, mOutFp);
                LOGD("bytes written into file  %d\n", bufferInfo->size);
            }
        }

        AMediaCodec_releaseOutputBuffer(mVideoCodec, bufIdx, false);
        mSawOutputEOS = (0 != (bufferInfo->flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM));
        mNumOutputFrame++;
        LOGD("%s index : %d  mSawOutputEOS : %s count : %u", __FUNCTION__, bufIdx,
             mSawOutputEOS ? "TRUE" : "FALSE", mNumOutputFrame);

        if (mSawOutputEOS) {
            CallBackHandle::mIsDone = true;
            mDecoderDoneCondition.notify_one();
        }
    }
}

void MediaExtratorDecodec::onFormatChanged(AMediaCodec *mediaCodec, AMediaFormat *format) {
    LOGD("In %s", __func__);
    if (mediaCodec == mVideoCodec && mediaCodec) {
        LOGD("%s { %s }", __FUNCTION__, AMediaFormat_toString(format));
        mVideoFormat = format;
    }
    if (mediaCodec == mAudioCodec && mediaCodec) {
        LOGD("%s { %s }", __FUNCTION__, AMediaFormat_toString(format));
        mAudioFormat = format;
    }
}

void MediaExtratorDecodec::onError(AMediaCodec *mediaCodec, media_status_t err) {
    LOGD("In %s", __func__);
    if (mediaCodec == mVideoCodec && mediaCodec) {
        ALOGE("Received Error %d", err);
        mErrorCode = err;
        mSignalledError = true;
        mDecoderDoneCondition.notify_one();
    }
}

tuple<ssize_t, uint32_t, int64_t>
MediaExtratorDecodec::readSampleData(uint8_t *inputBuffer, int32_t &offset,
                                     AMediaCodecBufferInfo &frameInfo,
                                     uint8_t *buf, int32_t frameID, size_t bufSize) {
    LOGD("In %s", __func__);
//    if (frameID == (int32_t) frameInfo.size()) {
//        return make_tuple(0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM, 0);
//    }
    LOGD("frameInfo: %d", frameInfo.size);

    uint32_t flags = frameInfo.flags;
    int64_t timestamp = frameInfo.presentationTimeUs;
    ssize_t bytesCount = frameInfo.size;
    if (bufSize < bytesCount) {
        LOGE("Error : Buffer size is insufficient to read sample");
        return make_tuple(0, AMEDIA_ERROR_MALFORMED, 0);
    }

    memcpy(buf, inputBuffer + offset, bytesCount);
    offset += bytesCount;
    return make_tuple(bytesCount, flags, timestamp);
}


JNIEnv *MediaExtratorDecodec::GetJNIEnv(bool *isAttach) {
    JNIEnv *env;
    int status;
    if (nullptr == mJavaVm) {
        LOGD("GetJNIEnv mJavaVm == nullptr");
        return nullptr;
    }
    *isAttach = false;
    status = mJavaVm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status != JNI_OK) {
        status = mJavaVm->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGD("GetJNIEnv failed to attach current thread");
            return nullptr;
        }
        *isAttach = true;
    }
    return env;
}

void MediaExtratorDecodec::PostStatusMessage(const char *msg) {
    bool isAttach = false;
    JNIEnv *pEnv = GetJNIEnv(&isAttach);
    if (pEnv == nullptr) {
        return;
    }
    jobject javaObj = mJavaObj;
    jmethodID mid = pEnv->GetMethodID(pEnv->GetObjectClass(javaObj), "CppStatusCallback",
                                      "(Ljava/lang/String;)V");
    jstring pJstring = pEnv->NewStringUTF(msg);
    pEnv->CallVoidMethod(javaObj, mid, pJstring);
    if (isAttach) {
        JavaVM *pJavaVm = mJavaVm;
        pJavaVm->DetachCurrentThread();
    }
}


