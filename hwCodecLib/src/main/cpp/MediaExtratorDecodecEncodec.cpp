//
// Created by wangyao on 2025/10/5.
//

#include <sys/stat.h>
#include "MediaExtratorDecodecEncodec.h"


MediaExtratorDecodecEncodec::MediaExtratorDecodecEncodec(JNIEnv *env, jobject thiz) {
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

MediaExtratorDecodecEncodec::~MediaExtratorDecodecEncodec() {
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
MediaExtratorDecodecEncodec::startMediaExtratorDecodecEncodec(const char *inputPath,
                                                              const char *outpath1,
                                                              const char *outPath2) {
    sSrcPath = inputPath;
    sOutPath1 = outpath1;
    sOutPath2 = outPath2;
    // 本对象在 JNI 层是缓存复用的单例，下面这些「一轮一清」的运行态标志如果不复位，
    // 第二轮进来时 mSawOutputDecodecEOS/mSawOutputEncodecEOS 还是上一轮的 true，
    // 解码/编码的 drain 循环一次都不进 —— 输出文件 0 字节，界面却提示成功。
    mSawInputDecodecEOS = false;
    mSawOutputDecodecEOS = false;
    mSignalledDecodecError = false;
    mSawInputEncodecEOS = false;
    mSawOutputEncodecEOS = false;
    mSignalledEncodecError = false;
    mErrorCode = AMEDIA_OK;
    mNumOutputDecodecVideoFrame = 0;
    mNumOutputDecodecAudioFrame = 0;
    mNumInputFrame = 0;
    mNumOutputVideoFrame = 0;
    CallBackHandle::mSawError = false;
    CallBackHandle::mIsDone = false;

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
        // 失败分支也要 release()：否则 AMediaExtractor_new() 的对象和输入 FILE*
        // 挂在复用的单例上，下一轮直接被覆盖，每失败一次泄漏一份。
        release();
        return;
    }

    // 2. 选择轨道
    if (!selectTracksAndGetFormat()) {
        LOGE("No valid tracks found");
        callbackInfo =
                "No valid tracks found \n";
        PostStatusMessage(callbackInfo.c_str());
        release();
        return;
    }

    // 3. 初始化解码器
    if (!initDecodec(false)) {
        LOGE("Failed to initialize Decodec");
        callbackInfo =
                "Failed to initialize Decodec \n";
        PostStatusMessage(callbackInfo.c_str());
        release();
        return;
    }

    // 4. 执行解码
    if (!decodec()) {
        LOGE("Decodec failed");
        callbackInfo =
                "Decodec failed \n";
        PostStatusMessage(callbackInfo.c_str());
        release();
        return;
    }

    // 5. 初始化编码器
    if (!initEncodec(false)) {
        LOGE("Failed to initialize Encodec");
        callbackInfo =
                "Failed to initialize Encodec \n";
        PostStatusMessage(callbackInfo.c_str());
        release();
        return;
    }

    // 6. 执行编码
    if (!encodec(false)) {
        LOGE("Encodec failed");
        callbackInfo =
                "Encodec failed \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    // 释放资源
    release();
}

// 初始化提取器
bool MediaExtratorDecodecEncodec::initExtractor() {
    LOGI("initExtractor===========");
    callbackInfo =
            "initExtractor================ \n";
    PostStatusMessage(callbackInfo.c_str());
    extractor = AMediaExtractor_new();
    if (!extractor) {
        LOGE("Failed to create media extractor ");
        callbackInfo =
                "Failed to create media extractor \n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }
    LOGE("inputPath:%s", sSrcPath.c_str());
    mInputFp = fopen(sSrcPath.c_str(), "rb");
    if (!mInputFp) {
        LOGE("Unable to open output file :%s", sSrcPath.c_str());
        callbackInfo =
                "Unable to open output file :" + sSrcPath + "\n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }
    struct stat buf;
    stat(sSrcPath.c_str(), &buf);
    size_t fileSize = buf.st_size;
    int32_t input_fd = fileno(mInputFp);

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

// 选择轨道获取AMediaFormat
bool MediaExtratorDecodecEncodec::selectTracksAndGetFormat() {
    LOGI("selectTracksAndGetFormat===========");
    callbackInfo =
            "selectTracksAndGetFormat================ \n";
    PostStatusMessage(callbackInfo.c_str());
    size_t trackCount = AMediaExtractor_getTrackCount(extractor);
    LOGI("Total tracks: %zu", trackCount);
    callbackInfo =
            "Total tracks:" + to_string(trackCount) + "\n";
    PostStatusMessage(callbackInfo.c_str());

    for (size_t i = 0; i < trackCount; i++) {
        AMediaFormat *format = AMediaExtractor_getTrackFormat(extractor, i);
        if (!format) continue;

        const char *mime;
        // Get encoder params
        if (AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
            LOGI("Track %zu: MIME=%s", i, mime);

            if (strncmp(mime, "video/", 6) == 0 && videoTrackIndex == -1) {
                videoTrackIndex = i;
                hasVideo = true;
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &mEncParams.width);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &mEncParams.height);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, &mEncParams.frameRate);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, &mEncParams.bitrate);
                if (mEncParams.bitrate <= 0 || mEncParams.frameRate <= 0) {
                    mEncParams.frameRate = 25;
                    if (!strcmp(mime, "video/3gpp") || !strcmp(mime, "video/mp4v-es")) {
                        mEncParams.bitrate = kEncodeMinVideoBitRate;
                    } else {
                        mEncParams.bitrate = kEncodeDefaultVideoBitRate;
                    }
                }
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT,
                                      &mEncParams.colorFormat);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_PROFILE, &mEncParams.profile);
//            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_LEVEL, &encParams.level);
                LOGI("Get encoder params: %dx%d, frameRate: %lld us,profile :%d,level:%d",
                     mEncParams.width, mEncParams.height, mEncParams.frameRate,
                     mEncParams.profile, mEncParams.level);
            } else if (strncmp(mime, "audio/", 6) == 0 && audioTrackIndex == -1) {
                audioTrackIndex = i;
                hasAudio = true;

                // 获取音频格式信息
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &mEncParams.sampleRate);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                                      &mEncParams.numChannels);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, &mEncParams.bitrate);

                LOGI("Audio track: sampleRate=%d, channels=%d",
                     mEncParams.sampleRate, mEncParams.numChannels);

                LOGI("Selected audio track: %d", audioTrackIndex);
                callbackInfo =
                        "Selected audio track:" + to_string(audioTrackIndex) + "\n";
                callbackInfo = callbackInfo + ",audioSampleRate:" + to_string(mEncParams.sampleRate)
                               + ",audioChannelCount:" + to_string(mEncParams.numChannels) + "\n";
                PostStatusMessage(callbackInfo.c_str());
            }
        }

        AMediaFormat_delete(format);
    }

    return hasVideo || hasAudio;
}

// 初始化解码器
bool MediaExtratorDecodecEncodec::initDecodec(bool asyncMode) {
    LOGI("initDecodec===========");
    callbackInfo =
            "initDecodec================ \n";
    PostStatusMessage(callbackInfo.c_str());
    // 添加视频轨道
    if (hasVideo) {
        AMediaExtractor_selectTrack(extractor, videoTrackIndex);
        mVideoFormat = AMediaExtractor_getTrackFormat(extractor, videoTrackIndex);
        AMediaFormat_getString(mVideoFormat, AMEDIAFORMAT_KEY_MIME, &video_mime);
        LOGI("video_mime: %s", video_mime);
        callbackInfo =
                "video_mime:" + string(video_mime) + "\n";
        PostStatusMessage(callbackInfo.c_str());

        mVideoDeCodec = createMediaCodec(mVideoFormat, video_mime, "", false /*isEncoder*/);

        if (!mVideoDeCodec) {
            LOGE("Failed to create video codec");
            callbackInfo =
                    "Failed to create video codec \n";
            PostStatusMessage(callbackInfo.c_str());
            return false;
        }

        if (asyncMode) {
            AMediaCodecOnAsyncNotifyCallback aCB = {OnInputAvailableCB, OnOutputAvailableCB,
                                                    OnFormatChangedCB, OnErrorCB};
            AMediaCodec_setAsyncNotifyCallback(mVideoDeCodec, aCB, this);

            ThreadTask task = []() {
                CallBackHandle();
            };

            g_threadManager->submitTask("video-decode-Thread", task, PRIORITY_NORMAL);

        }

        LOGI("create video codec success");
        callbackInfo =
                "create video codec success:  \n";
        PostStatusMessage(callbackInfo.c_str());
        AMediaCodec_start(mVideoDeCodec);
    }


    // 添加音频轨道
    if (hasAudio) {
        AMediaExtractor_selectTrack(extractor, audioTrackIndex);
        mAudioFormat = AMediaExtractor_getTrackFormat(extractor, audioTrackIndex);
        AMediaFormat_getString(mAudioFormat, AMEDIAFORMAT_KEY_MIME, &audio_mime);
        LOGI("audio_mime: %s", audio_mime);
        callbackInfo =
                "audio_mime:" + string(audio_mime) + "\n";
        PostStatusMessage(callbackInfo.c_str());

        mAudioDeCodec = createMediaCodec(mAudioFormat, audio_mime, "", false /*isEncoder*/);

        if (!mAudioDeCodec) {
            LOGE("Failed to create audio codec");
            callbackInfo =
                    "Failed to create audio codec \n";
            PostStatusMessage(callbackInfo.c_str());
            return false;
        }

        if (asyncMode) {
            AMediaCodecOnAsyncNotifyCallback aCB = {OnInputAvailableCB, OnOutputAvailableCB,
                                                    OnFormatChangedCB, OnErrorCB};
            AMediaCodec_setAsyncNotifyCallback(mAudioDeCodec, aCB, this);

            ThreadTask task = []() {
                CallBackHandle();
            };

            g_threadManager->submitTask("audio-decode-Thread", task, PRIORITY_NORMAL);

        }

        LOGI("create audio codec success");
        callbackInfo =
                "create audio codec success:  \n";
        PostStatusMessage(callbackInfo.c_str());
        AMediaCodec_start(mAudioDeCodec);
    }


    LOGI("initDecodec initialized successfully");
    callbackInfo =
            "initDecodec initialized successfully \n";
    PostStatusMessage(callbackInfo.c_str());
    return true;
}


// 执行解码
bool MediaExtratorDecodecEncodec::decodec() {
    LOGI("decodec===========");
    isDeCodec = true;
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
    mDecodecOutFp = fopen(sOutPath1.c_str(), "wb");
    if (!mDecodecOutFp) {
        LOGE("Unable to open :%s", sOutPath1.c_str());
        callbackInfo =
                "Unable to open " + sOutPath1 + "\n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }

    while (!sawEOS) {
        ssize_t trackIndex = AMediaExtractor_getSampleTrackIndex(extractor);

        if (trackIndex < 0) {
            sawEOS = true;
            break;
        }

        if (trackIndex == videoTrackIndex && hasVideo) {
            // 检查时间戳是否有效（避免重复或倒退的时间戳）
            if (info.presentationTimeUs > lastVideoPts) {
                if (!asyncMode) {
                    while (!mSawOutputDecodecEOS && !mSignalledDecodecError) {
                        /* Queue input data */
                        if (!mSawInputDecodecEOS) {
                            ssize_t inIdx = AMediaCodec_dequeueInputBuffer(mVideoDeCodec,
                                                                           kQueueDequeueTimeoutUs);
                            if (inIdx < 0 && inIdx != AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                                LOGE("AMediaCodec_dequeueInputBuffer returned invalid index %zd\n",
                                     inIdx);
                                mErrorCode = (media_status_t) inIdx;
                                return mErrorCode;
                            } else if (inIdx >= 0) {
                                onInputAvailable(mVideoDeCodec, inIdx);
                            }
                        }

                        /* Dequeue output data */
                        ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(mVideoDeCodec, &info,
                                                                         kQueueDequeueTimeoutUs);
                        if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                            mVideoFormat = AMediaCodec_getOutputFormat(mVideoDeCodec);
                            const char *s = AMediaFormat_toString(mVideoFormat);
                            LOGI("Output format: %s\n", s);
                        } else if (outIdx >= 0) {
                            onOutputAvailable(mVideoDeCodec, outIdx, &info);
                        } else if (!(outIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER ||
                                     outIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)) {
                            LOGE("AMediaCodec_dequeueOutputBuffer returned invalid index %zd\n",
                                 outIdx);
                            mErrorCode = (media_status_t) outIdx;
                            return mErrorCode;
                        }
                    }
                } else {
                    unique_lock<mutex> lock(mMutex);
                    mDecoderDoneCondition.wait(lock, [this]() {
                        return (mSawOutputDecodecEOS || mSignalledDecodecError);
                    });
                }
                if (mSignalledDecodecError) {
                    LOGE("Received Error while Decoding");
                    return mErrorCode;
                }

                lastVideoPts = info.presentationTimeUs;
            }
        } else if (trackIndex == audioTrackIndex && hasAudio) {     //音频轨道的解码
            // 检查时间戳是否有效
            if (info.presentationTimeUs > lastAudioPts) {
                // 检查时间戳是否有效（避免重复或倒退的时间戳）
                if (!asyncMode) {
                    while (!mSawOutputDecodecEOS && !mSignalledDecodecError) {
                        /* Queue input data */
                        if (!mSawInputDecodecEOS) {
                            ssize_t inIdx = AMediaCodec_dequeueInputBuffer(mAudioDeCodec,
                                                                           kQueueDequeueTimeoutUs);
                            if (inIdx < 0 && inIdx != AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                                LOGE("AMediaCodec_dequeueInputBuffer returned invalid index %zd\n",
                                     inIdx);
                                mErrorCode = (media_status_t) inIdx;
                                return mErrorCode;
                            } else if (inIdx >= 0) {
                                onInputAvailable(mAudioDeCodec, inIdx);
                            }
                        }

                        /* Dequeue output data */
                        ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(mAudioDeCodec, &info,
                                                                         kQueueDequeueTimeoutUs);
                        if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                            mAudioFormat = AMediaCodec_getOutputFormat(mAudioDeCodec);
                            const char *s = AMediaFormat_toString(mAudioFormat);
                            LOGI("Output format: %s\n", s);
                        } else if (outIdx >= 0) {
                            onOutputAvailable(mVideoDeCodec, outIdx, &info);
                        } else if (!(outIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER ||
                                     outIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)) {
                            LOGE("AMediaCodec_dequeueOutputBuffer returned invalid index %zd\n",
                                 outIdx);
                            mErrorCode = (media_status_t) outIdx;
                            return mErrorCode;
                        }
                    }
                } else {
                    unique_lock<mutex> lock(mMutex);
                    mDecoderDoneCondition.wait(lock, [this]() {
                        return (mSawOutputDecodecEOS || mSignalledDecodecError);
                    });
                }
                if (mSignalledDecodecError) {
                    ALOGE("Received Error while Decoding");
                    return mErrorCode;
                }
                lastAudioPts = info.presentationTimeUs;
            }
        }

        // 短暂休眠以避免过度占用CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    LOGI("media decodec completed out file:%s", sOutPath1.c_str());
    // 必须在这里就关掉：encodec() 第二步会把 sOutPath1 当输入文件读它的大小和内容
    // （ifstream eleStream(sOutPath1...)），mDecodecOutFp 一直不 fclose 的话，
    // 还留在 stdio 缓冲区里的尾部数据对它不可见，第二遍拿到的尺寸/帧就是残缺的。
    if (mDecodecOutFp) {
        fclose(mDecodecOutFp);
        mDecodecOutFp = nullptr;
    }
    callbackInfo =
            "media decodec completed file:" + sOutPath1 + "\n";
    PostStatusMessage(callbackInfo.c_str());
    return true;
}

bool MediaExtratorDecodecEncodec::initEncodec(bool asyncMode) {
    LOGI("initEncodec===========");

    // 添加视频轨道
    if (hasVideo) {
        // 创建一个新的 AMediaFormat 用于编码，不要直接使用 Extractor 的 format
        // Extractor 的 format 是给解码器用的，包含了 CSD (SPS/PPS) 数据且缺少编码器必要的参数
        AMediaFormat *videoFormat = AMediaFormat_new();
        AMediaFormat_setString(videoFormat, AMEDIAFORMAT_KEY_MIME, video_mime);
        AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_WIDTH, mEncParams.width);
        AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_HEIGHT, mEncParams.height);
        AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT, mEncParams.colorFormat);
        AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_BIT_RATE, mEncParams.bitrate);
        AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_FRAME_RATE, mEncParams.frameRate);
        AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, mEncParams.iFrameInterval);

        LOGI("encodec video_mime: %s", video_mime);
        callbackInfo = "encodec video_mime:" + string(video_mime) + "\n";
        PostStatusMessage(callbackInfo.c_str());

        mVideoEnCodec = createMediaCodec(videoFormat, video_mime, "", true /*isEncoder*/);
        AMediaFormat_delete(videoFormat);

        if (!mVideoEnCodec) {
            LOGE("Failed to create video encodec");
            callbackInfo =
                    "Failed to create video encodec \n";
            PostStatusMessage(callbackInfo.c_str());
            return false;
        }

        if (asyncMode) {
            AMediaCodecOnAsyncNotifyCallback aCB = {OnInputAvailableCB, OnOutputAvailableCB,
                                                    OnFormatChangedCB, OnErrorCB};
            AMediaCodec_setAsyncNotifyCallback(mVideoEnCodec, aCB, this);

            ThreadTask task = []() {
                CallBackHandle();
            };

            g_threadManager->submitTask("video-encode-Thread", task, PRIORITY_NORMAL);

        }

        LOGI("create video encodec success");
        callbackInfo =
                "create video encodec success:  \n";
        PostStatusMessage(callbackInfo.c_str());
        AMediaCodec_start(mVideoEnCodec);
    }


    // 添加音频轨道
    if (hasAudio) {
        AMediaFormat *audioFormat = AMediaFormat_new();
        AMediaFormat_setString(audioFormat, AMEDIAFORMAT_KEY_MIME, audio_mime);
        AMediaFormat_setInt32(audioFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, mEncParams.sampleRate);
        AMediaFormat_setInt32(audioFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, mEncParams.numChannels);
        AMediaFormat_setInt32(audioFormat, AMEDIAFORMAT_KEY_BIT_RATE, mEncParams.bitrate);

        LOGI("audio_mime: %s", audio_mime);
        callbackInfo =
                "audio_mime:" + string(audio_mime) + "\n";
        PostStatusMessage(callbackInfo.c_str());

        mAudioEnCodec = createMediaCodec(audioFormat, audio_mime, "", true /*isEncoder*/);
        AMediaFormat_delete(audioFormat);

        if (!mAudioEnCodec) {
            LOGE("Failed to create audio encodec");
            callbackInfo =
                    "Failed to create audio encodec \n";
            PostStatusMessage(callbackInfo.c_str());
            return false;
        }

        if (asyncMode) {
            AMediaCodecOnAsyncNotifyCallback aCB = {OnInputAvailableCB, OnOutputAvailableCB,
                                                    OnFormatChangedCB, OnErrorCB};
            AMediaCodec_setAsyncNotifyCallback(mAudioEnCodec, aCB, this);

            ThreadTask task = []() {
                CallBackHandle();
            };

            g_threadManager->submitTask("audio-encode-Thread", task, PRIORITY_NORMAL);

        }

        LOGI("create audio encodec success");
        callbackInfo =
                "create audio encodec success:  \n";
        PostStatusMessage(callbackInfo.c_str());
        AMediaCodec_start(mAudioEnCodec);
    }


    LOGI("initEncodec initialized successfully");
    callbackInfo =
            "initEncodec initialized successfully \n";
    PostStatusMessage(callbackInfo.c_str());
    return true;
}

bool MediaExtratorDecodecEncodec::encodec(bool asyncMode) {
    isDeCodec = false;
    AMediaCodecBufferInfo info;
    bool sawEOS = false;
    int64_t lastVideoPts = -1;
    int64_t lastAudioPts = -1;

    // 重新选择所有轨道以重置读取位置
    if (hasVideo) AMediaExtractor_selectTrack(extractor, videoTrackIndex);
    if (hasAudio) AMediaExtractor_selectTrack(extractor, audioTrackIndex);

    mEncodecOutFp = fopen(sOutPath2.c_str(), "wb");
    if (!mEncodecOutFp) {
        LOGE("Unable to open :%s", sOutPath2.c_str());
        callbackInfo =
                "Unable to open " + sOutPath2 + "\n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }

    // 设置读取位置到开始
    AMediaExtractor_seekTo(extractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
    ifstream eleStream(sOutPath1.c_str(), ifstream::binary | ifstream::ate);
    mEleStream = &eleStream;
    if (!eleStream.is_open()) {
        LOGE("%s file not found", sOutPath1.c_str());
        callbackInfo = "not found file:" + sOutPath1 + " \n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }
    size_t eleSize = eleStream.tellg();
    eleStream.seekg(0, ifstream::beg);
    mEncodecInputBufferSize = eleSize;
    while (!sawEOS) {
        ssize_t trackIndex = AMediaExtractor_getSampleTrackIndex(extractor);

        if (trackIndex < 0) {
            sawEOS = true;
            break;
        }

        if (trackIndex == videoTrackIndex && hasVideo) {
            mEncParams.frameSize = mEncParams.width * mEncParams.height * 3 / 2;
            // 检查时间戳是否有效（避免重复或倒退的时间戳）
            if (info.presentationTimeUs > lastVideoPts) {
                if (!asyncMode) {
                    while (!mSawOutputEncodecEOS && !mSignalledEncodecError) {
                        /* Queue input data */
                        if (!mSawInputEncodecEOS) {
                            ssize_t inIdx = AMediaCodec_dequeueInputBuffer(mVideoEnCodec,
                                                                           kQueueDequeueTimeoutUs);
                            if (inIdx < 0 && inIdx != AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                                LOGE("AMediaCodec_dequeueInputBuffer returned invalid index %zd\n",
                                     inIdx);
                                mErrorCode = (media_status_t) inIdx;
                                return mErrorCode;
                            } else if (inIdx >= 0) {
                                onInputAvailable(mVideoEnCodec, inIdx);
                            }
                        }

                        /* Dequeue output data */
                        ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(mVideoEnCodec, &info,
                                                                         kQueueDequeueTimeoutUs);
                        if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                            mVideoFormat = AMediaCodec_getOutputFormat(mVideoEnCodec);
                            const char *s = AMediaFormat_toString(mVideoFormat);
                            LOGI("Output format: %s\n", s);
                        } else if (outIdx >= 0) {
                            onOutputAvailable(mVideoEnCodec, outIdx, &info);
                        } else if (!(outIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER ||
                                     outIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)) {
                            LOGE("AMediaCodec_dequeueOutputBuffer returned invalid index %zd\n",
                                 outIdx);
                            mErrorCode = (media_status_t) outIdx;
                            return mErrorCode;
                        }
                    }
                } else {
                    unique_lock<mutex> lock(mMutex);
                    mDecoderDoneCondition.wait(lock, [this]() {
                        return (mSawOutputEncodecEOS || mSignalledEncodecError);
                    });
                }
                if (mSignalledEncodecError) {
                    LOGE("Received Error while Decoding");
                    return mErrorCode;
                }

                lastVideoPts = info.presentationTimeUs;
            }
        } else if (trackIndex == audioTrackIndex && hasAudio) {     //音频轨道的解码


        }

        // 短暂休眠以避免过度占用CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    LOGI("media encodec completed out file:%s", sOutPath2.c_str());
    // fwrite 之后必须 fclose，否则最后一缓冲区的数据不会落盘（界面上已经提示完成，
    // 文件却是残缺的），fd 也一直泄漏。
    if (mEncodecOutFp) {
        fclose(mEncodecOutFp);
        mEncodecOutFp = nullptr;
    }
    // mEleStream 指的是 encodec() 的栈上 ifstream（eleStream），函数一返回它就是悬垂
    // 指针；就地置空，避免后面任何一处再通过成员去摸已经析构的对象。
    mEleStream = nullptr;
    callbackInfo =
            "media encodec completed file:" + sOutPath2 + "\n";
    PostStatusMessage(callbackInfo.c_str());
    return true;

}


// 释放资源
void MediaExtratorDecodecEncodec::release() {

    if (extractor) {
        AMediaExtractor_delete(extractor);
        extractor = nullptr;
    }

    if (mVideoFormat) {
        AMediaFormat_delete(mVideoFormat);
        mVideoFormat = nullptr;
    }

    if (mVideoDeCodec) {
        AMediaCodec_stop(mVideoDeCodec);
        AMediaCodec_delete(mVideoDeCodec);
        // delete 之后必须置空：对象是 JNI 层缓存复用的单例，留着悬垂地址的话，
        // 下一轮任何 if (mVideoDeCodec) 判断都会拿它去 stop/delete（二次释放）。
        mVideoDeCodec = nullptr;
    }

    if (mVideoEnCodec) {
        AMediaCodec_stop(mVideoEnCodec);
        AMediaCodec_delete(mVideoEnCodec);
        mVideoEnCodec = nullptr;
    }

    if (mAudioDeCodec) {
        AMediaCodec_stop(mAudioDeCodec);
        AMediaCodec_delete(mAudioDeCodec);
        mAudioDeCodec = nullptr;
    }

    if (mAudioEnCodec) {
        AMediaCodec_stop(mAudioEnCodec);
        AMediaCodec_delete(mAudioEnCodec);
        mAudioEnCodec = nullptr;
    }

    if (mAudioFormat) {
        AMediaFormat_delete(mAudioFormat);
        mAudioFormat = nullptr;
    }
    // 三个 FILE* 原先没有任何一处 fclose：解码/编码输出用 fwrite 写，缓冲区最后
    // 一段永远不落盘，fd 也每轮各泄漏一个；mInputFp 是 initExtractor() 打开的输入。
    if (mDecodecOutFp) {
        fclose(mDecodecOutFp);
        mDecodecOutFp = nullptr;
    }
    if (mEncodecOutFp) {
        fclose(mEncodecOutFp);
        mEncodecOutFp = nullptr;
    }
    if (mInputFp) {
        fclose(mInputFp);
        mInputFp = nullptr;
    }
    mEleStream = nullptr;
    LOGI("Resources released");
}

void MediaExtratorDecodecEncodec::onInputAvailable(AMediaCodec *mediaCodec, int32_t bufIdx) {
    if (isDeCodec) {
        onDecodecInputAvailable(mediaCodec, bufIdx);
    } else {
        onEncodecInputAvailable(mediaCodec, bufIdx);
    }

}

void
MediaExtratorDecodecEncodec::onEncodecInputAvailable(AMediaCodec *mediaEnCodec, int32_t bufIdx) {
    LOGI("In %s", __func__);
    if (mediaEnCodec == mVideoEnCodec && mediaEnCodec) {
        if (mSawInputEncodecEOS || bufIdx < 0) return;
        if (mSignalledEncodecError) {
            CallBackHandle::mSawError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        size_t bufSize = 0;
        char *buf = (char *) AMediaCodec_getInputBuffer(mVideoEnCodec, bufIdx, &bufSize);
        if (!buf) {
            mErrorCode = AMEDIA_ERROR_IO;
            mSignalledEncodecError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        if (mEncodecInputBufferSize < mOffset) {
            LOGE("Out of bound access of input buffer\n");
            mErrorCode = AMEDIA_ERROR_MALFORMED;
            mSignalledEncodecError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }
        size_t bytesToRead = mEncParams.frameSize;
        if (mEncodecInputBufferSize - mOffset < mEncParams.frameSize) {
            bytesToRead = mEncodecInputBufferSize - mOffset;
        }
        //b/148655275 - Update Frame size, as Format value may not be valid
        if (bufSize < bytesToRead) {
            if (mNumInputFrame == 0) {
                mEncParams.frameSize = bufSize;
                bytesToRead = bufSize;
                mEncParams.numFrames = (mEncodecInputBufferSize + mEncParams.frameSize - 1)
                                       / mEncParams.frameSize;
            } else {
                LOGE("bytes to read %zu bufSize %zu \n", bytesToRead, bufSize);
                mErrorCode = AMEDIA_ERROR_MALFORMED;
                mSignalledEncodecError = true;
                mDecoderDoneCondition.notify_one();
                return;
            }
        }
        if (bytesToRead < mEncParams.frameSize && mNumInputFrame < mEncParams.numFrames - 1) {
            LOGE("Partial frame at frameID %d bytesToRead %zu frameSize %d total numFrames %d\n",
                 mNumInputFrame, bytesToRead, mEncParams.frameSize, mEncParams.numFrames);
            mErrorCode = AMEDIA_ERROR_MALFORMED;
            mSignalledDecodecError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }
        mEleStream->read(buf, bytesToRead);
        size_t bytesgcount = mEleStream->gcount();
        if (bytesgcount != bytesToRead) {
            LOGE("bytes to read %zu actual bytes read %zu \n", bytesToRead, bytesgcount);
            mErrorCode = AMEDIA_ERROR_MALFORMED;
            mSignalledDecodecError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        uint32_t flag = 0;
        if (mNumInputFrame == mEncParams.numFrames - 1 || bytesToRead == 0) {
            ALOGD("Sending EOS on input Last frame\n");
            flag |= AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
        }

        uint64_t presentationTimeUs;
        presentationTimeUs = mNumInputFrame * (1000000 / mEncParams.frameRate);
//        if (!strncmp(mMime, "video/", 6)) {
//            presentationTimeUs = mNumInputFrame * (1000000 / mEncParams.frameRate);
//        } else {
//            presentationTimeUs =
//                    (uint64_t)mNumInputFrame * mEncParams.frameSize * 1000000 / mEncParams.sampleRate;
//        }

        if (flag == AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) mSawInputDecodecEOS = true;
        LOGI("%s bytesRead : %zd presentationTimeUs : %" PRIu64 " mSawInputEOS : %s", __FUNCTION__,
             bytesToRead, presentationTimeUs, mSawInputDecodecEOS ? "TRUE" : "FALSE");

        media_status_t status = AMediaCodec_queueInputBuffer(mVideoEnCodec, bufIdx, 0 /* offset */,
                                                             bytesToRead, presentationTimeUs, flag);
        if (AMEDIA_OK != status) {
            mErrorCode = status;
            mSignalledDecodecError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }
        mNumInputFrame++;
        mOffset += bytesToRead;
    }
}

void
MediaExtratorDecodecEncodec::onDecodecInputAvailable(AMediaCodec *mediaDeCodec, int32_t bufIdx) {
    LOGD("onInputAvailable %s", __func__);
    if (mediaDeCodec == mVideoDeCodec && mediaDeCodec) {
        if (mSawInputDecodecEOS || bufIdx < 0) return;
        if (mSignalledDecodecError) {
            CallBackHandle::mSawError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        size_t bufSize = AMediaExtractor_getSampleSize(extractor);
        if (bufSize <= 0) {
            LOGE("AMediaExtractor_getSampleSize====");
            return;
        }
        // 获取输入缓冲区
        uint8_t *buf = AMediaCodec_getInputBuffer(mVideoDeCodec, bufIdx, &bufSize);
        if (!buf) {
            mErrorCode = AMEDIA_ERROR_IO;
            mSignalledDecodecError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        // 从提取器读取数据
        ssize_t bytesRead = AMediaExtractor_readSampleData(extractor, buf, bufSize);
        if (bytesRead < 0) {
            LOGI("reading video sample data: %zd", bytesRead);
            // 输入结束
            AMediaCodec_queueInputBuffer(mVideoDeCodec, bufIdx, 0, 0, 0,
                                         AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
            LOGI("从提取器读取数据到结束尾");
            callbackInfo =
                    "视频轨道从提取器读取数据到结束尾 reading sample data:" + to_string(bytesRead) +
                    "\n";
            PostStatusMessage(callbackInfo.c_str());
            return;
        }
        uint32_t flag = AMediaExtractor_getSampleFlags(extractor);
        int64_t presentationTimeUs = AMediaExtractor_getSampleTime(extractor);

        if (flag == AMEDIA_ERROR_MALFORMED) {
            mErrorCode = (media_status_t) flag;
            mSignalledDecodecError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        if (flag == AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) mSawInputDecodecEOS = true;
        LOGD("video - %s bytesRead : %zd presentationTimeUs : %" PRId64 " mSawInputDecodecEOS : %s",
             __FUNCTION__,
             bytesRead, presentationTimeUs, mSawInputDecodecEOS ? "TRUE" : "FALSE");

        // 将数据送入解码器
        media_status_t status = AMediaCodec_queueInputBuffer(mVideoDeCodec, bufIdx, 0 /* offset */,
                                                             bytesRead, presentationTimeUs, flag);
        if (AMEDIA_OK != status) {
            mErrorCode = status;
            mSignalledDecodecError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        if (!AMediaExtractor_advance(extractor)) {
            return;
        }

    } else if (mediaDeCodec == mAudioDeCodec && mediaDeCodec) {
        if (mSawInputDecodecEOS || bufIdx < 0) return;
        if (mSignalledDecodecError) {
            CallBackHandle::mSawError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        size_t bufSize = AMediaExtractor_getSampleSize(extractor);
        if (bufSize <= 0) {
            LOGE("AMediaExtractor_getSampleSize====");
            return;
        }
        // 获取输入缓冲区
        uint8_t *buf = AMediaCodec_getInputBuffer(mAudioDeCodec, bufIdx, &bufSize);
        if (!buf) {
            mErrorCode = AMEDIA_ERROR_IO;
            mSignalledDecodecError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        // 从提取器读取数据
        ssize_t bytesRead = AMediaExtractor_readSampleData(extractor, buf, bufSize);
        if (bytesRead < 0) {
            LOGI("reading audio sample data: %zd", bytesRead);
            // 输入结束
            AMediaCodec_queueInputBuffer(mAudioDeCodec, bufIdx, 0, 0, 0,
                                         AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
            LOGI("从提取器读取音频轨道数据到结束尾");
            callbackInfo =
                    "音频轨道从提取器读取数据到结束尾 reading sample data:" + to_string(bytesRead) +
                    "\n";
            PostStatusMessage(callbackInfo.c_str());
            return;
        }
        uint32_t flag = AMediaExtractor_getSampleFlags(extractor);
        int64_t presentationTimeUs = AMediaExtractor_getSampleTime(extractor);

        if (flag == AMEDIA_ERROR_MALFORMED) {
            mErrorCode = (media_status_t) flag;
            mSignalledDecodecError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        if (flag == AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) mSawInputDecodecEOS = true;
        LOGD("audio - %s bytesRead : %zd presentationTimeUs : %" PRId64 " mSawInputDecodecEOS : %s",
             __FUNCTION__,
             bytesRead, presentationTimeUs, mSawInputDecodecEOS ? "TRUE" : "FALSE");

        // 将数据送入解码器
        media_status_t status = AMediaCodec_queueInputBuffer(mAudioDeCodec, bufIdx, 0 /* offset */,
                                                             bytesRead, presentationTimeUs, flag);
        if (AMEDIA_OK != status) {
            mErrorCode = status;
            mSignalledDecodecError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        if (!AMediaExtractor_advance(extractor)) {
            return;
        }

    }
}


void MediaExtratorDecodecEncodec::onOutputAvailable(AMediaCodec *mediaCodec, int32_t bufIdx,
                                                    AMediaCodecBufferInfo *bufferInfo) {

    if (isDeCodec) {
        onDecodecOutputAvailable(mediaCodec, bufIdx, bufferInfo);
    } else {
        onEncodecOutputAvailable(mediaCodec, bufIdx, bufferInfo);
    }

}

void
MediaExtratorDecodecEncodec::onEncodecOutputAvailable(AMediaCodec *mediaEnCodec, int32_t bufIdx,
                                                      AMediaCodecBufferInfo *bufferInfo) {
    LOGD("In %s", __func__);
    if (mediaEnCodec == mVideoEnCodec && mediaEnCodec) {
        if (mSawOutputEncodecEOS || bufIdx < 0) return;
        if (mSignalledEncodecError) {
            CallBackHandle::mSawError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        if (mEncodecOutFp != nullptr) {
            size_t bufSize;
            uint8_t *buf = AMediaCodec_getOutputBuffer(mVideoEnCodec, bufIdx, &bufSize);
            if (buf) {
                fwrite(buf, sizeof(char), bufferInfo->size, mEncodecOutFp);
                LOGD("encodec bytes written into file  %d\n", bufferInfo->size);
            }
        }

        AMediaCodec_releaseOutputBuffer(mVideoEnCodec, bufIdx, false);
        mSawOutputEncodecEOS = (0 != (bufferInfo->flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM));
        mNumOutputVideoFrame++;
        LOGD("video - %s index : %d  mSawOutputEOS : %s count : %u", __FUNCTION__, bufIdx,
             mSawOutputEncodecEOS ? "TRUE" : "FALSE", mNumOutputVideoFrame);

        if (mSawOutputEncodecEOS) {
            CallBackHandle::mIsDone = true;
            mDecoderDoneCondition.notify_one();
        }
    } else if (mediaEnCodec == mAudioEnCodec && mediaEnCodec) {

    }

}

void
MediaExtratorDecodecEncodec::onDecodecOutputAvailable(AMediaCodec *mediaDeCodec, int32_t bufIdx,
                                                      AMediaCodecBufferInfo *bufferInfo) {
    LOGD("In %s", __func__);
    if (mediaDeCodec == mVideoDeCodec && mediaDeCodec) {
        if (mSawOutputDecodecEOS || bufIdx < 0) return;
        if (mSignalledDecodecError) {
            CallBackHandle::mSawError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        if (mDecodecOutFp != nullptr) {
            size_t bufSize;
            uint8_t *buf = AMediaCodec_getOutputBuffer(mVideoDeCodec, bufIdx, &bufSize);
            if (buf) {
                fwrite(buf, sizeof(char), bufferInfo->size, mDecodecOutFp);
                LOGD("bytes written into file  %d\n", bufferInfo->size);
            }
        }

        AMediaCodec_releaseOutputBuffer(mVideoDeCodec, bufIdx, false);
        mSawOutputDecodecEOS = (0 != (bufferInfo->flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM));
        mNumOutputDecodecVideoFrame++;
        LOGD("video - %s index : %d  mSawOutputDecodecEOS : %s count : %u", __FUNCTION__, bufIdx,
             mSawOutputDecodecEOS ? "TRUE" : "FALSE", mNumOutputDecodecVideoFrame);

        if (mSawOutputDecodecEOS) {
            CallBackHandle::mIsDone = true;
            mDecoderDoneCondition.notify_one();
        }
    } else if (mediaDeCodec == mAudioDeCodec && mediaDeCodec) {
        if (mSawOutputDecodecEOS || bufIdx < 0) return;
        if (mSignalledDecodecError) {
            CallBackHandle::mSawError = true;
            mDecoderDoneCondition.notify_one();
            return;
        }

        if (mDecodecOutFp != nullptr) {
            size_t bufSize;
            uint8_t *buf = AMediaCodec_getOutputBuffer(mAudioDeCodec, bufIdx, &bufSize);
            if (buf) {
                fwrite(buf, sizeof(char), bufferInfo->size, mDecodecOutFp);
                LOGD("decodec bytes written into file  %d\n", bufferInfo->size);
            }
        }

        AMediaCodec_releaseOutputBuffer(mAudioDeCodec, bufIdx, false);
        mSawOutputDecodecEOS = (0 != (bufferInfo->flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM));
        mNumOutputDecodecAudioFrame++;
        LOGD("video - %s index : %d  mSawOutputDecodecEOS : %s count : %u", __FUNCTION__, bufIdx,
             mSawOutputDecodecEOS ? "TRUE" : "FALSE", mNumOutputDecodecAudioFrame);

        if (mSawOutputDecodecEOS) {
            CallBackHandle::mIsDone = true;
            mDecoderDoneCondition.notify_one();
        }
    }

}


void MediaExtratorDecodecEncodec::onFormatChanged(AMediaCodec *mediaCodec, AMediaFormat *format) {
    LOGD("In %s", __func__);
    if (mediaCodec == mVideoDeCodec && mediaCodec) {
        LOGD("%s { %s }", __FUNCTION__, AMediaFormat_toString(format));
        mVideoFormat = format;
    }
    if (mediaCodec == mAudioDeCodec && mediaCodec) {
        LOGD("%s { %s }", __FUNCTION__, AMediaFormat_toString(format));
        mAudioFormat = format;
    }
}

void MediaExtratorDecodecEncodec::onError(AMediaCodec *mediaCodec, media_status_t err) {
    LOGD("In %s", __func__);
    if (mediaCodec == mVideoDeCodec && mediaCodec) {
        ALOGE("Received Error %d", err);
        mErrorCode = err;
        mSignalledDecodecError = true;
        mDecoderDoneCondition.notify_one();
    }
}


JNIEnv *MediaExtratorDecodecEncodec::GetJNIEnv(bool *isAttach) {
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

void MediaExtratorDecodecEncodec::PostStatusMessage(const char *msg) {
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


